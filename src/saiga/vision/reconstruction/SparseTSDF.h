﻿/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once
#include "saiga/core/geometry/all.h"
#include "saiga/core/image/all.h"
#include "saiga/core/util/BinaryFile.h"
#include "saiga/core/util/ProgressBar.h"
#include "saiga/core/util/Thread/SpinLock.h"
#include "saiga/core/util/Thread/omp.h"

#include "MarchingCubes.h"

namespace Saiga
{
// A block sparse truncated signed distance field.
// Generated by integrating (fusing) aligned depth maps.
// Each block consists of VOXEL_BLOCK_SIZE^3 voxels.
// A typical value for VOXEL_BLOCK_SIZE is 8.
//
// The size in meters is given in the constructor.
//
// The voxel blocks are stored sparse using a hashmap. For each hashbucket,
// we store a linked-list with all blocks inside this bucket.
struct SAIGA_VISION_API SparseTSDF
{
    static constexpr int VOXEL_BLOCK_SIZE = 8;
    using VoxelBlockIndex                 = ivec3;
    using VoxelIndex                      = ivec3;

    // Each TSDF voxel consists of the (signed) distance to the surface and a confidence weight.
    // If desired, you can add a RGB member to estimate the color during integration.
    struct Voxel
    {
        float distance = 0;
        float weight   = 0;
    };


    // A voxel block is a 3 dimensional array of voxels.
    // Given a VOXEL_BLOCK_SIZE of 8 a voxel blocks consists of 8*8*8=512 voxels.
    //
    // Due to the sparse storage, each voxel block has to known it's own index.
    // The next_index points to the next voxel block in the same hash bucket.
    struct VoxelBlock
    {
        //        Voxel data[VOXEL_BLOCK_SIZE][VOXEL_BLOCK_SIZE][VOXEL_BLOCK_SIZE];
        std::array<std::array<std::array<Voxel, 8>, 8>, 8> data;
        VoxelBlockIndex index = VoxelBlockIndex(-973454, -973454, -973454);
        int next_index        = -1;

        // the weight of all voxels is 0
        bool Empty()
        {
            for (auto& vz : data)
            {
                for (auto& vy : vz)
                {
                    for (auto& v : vy)
                    {
                        if (v.weight > 0) return false;
                    }
                }
            }
            return true;
        }
    };


    SparseTSDF(float voxel_size = 0.01, int reserve_blocks = 1000, int hash_size = 100000)
        : voxel_size(voxel_size),
          voxel_size_inv(1.0 / voxel_size),
          hash_size(hash_size),
          blocks(reserve_blocks),
          first_hashed_block(hash_size, -1),
          hash_locks(hash_size)

    {
        static_assert(sizeof(VoxelBlock::data) == 8 * 8 * 8 * 2 * sizeof(float), "Incorrect Voxel Size");
        block_size_inv = 1.0 / (voxel_size * VOXEL_BLOCK_SIZE);

        //        std::cout << "SparseTSDF created. Allocated Memory: " << Memory() / (1000 * 1000) << " MB." <<
        //        std::endl;
    }

    SparseTSDF(const std::string& file) { Load(file); }


    SparseTSDF(const SparseTSDF& other)
    {
        voxel_size         = other.voxel_size;
        voxel_size_inv     = other.voxel_size_inv;
        hash_size          = other.hash_size;
        blocks             = other.blocks;
        first_hashed_block = other.first_hashed_block;
        hash_locks         = std::vector<SpinLock>(hash_size);
        current_blocks     = other.current_blocks.load();
    }

    SAIGA_VISION_API friend std::ostream& operator<<(std::ostream& os, const SparseTSDF& tsdf);



    size_t Memory()
    {
        size_t mem_blocks = blocks.size() * sizeof(VoxelBlock);
        size_t mem_hash   = first_hashed_block.size() * sizeof(int);
        return mem_blocks + mem_hash + sizeof(*this);
    }

    // Returns the voxel block or 0 if it doesn't exist.
    VoxelBlock* GetBlock(const VoxelBlockIndex& i) { return GetBlock(i, H(i)); }


    // Insert a new block into the TSDF and returns a pointer to it.
    // If the block already exists, nothing is inserted.
    VoxelBlock* InsertBlock(const VoxelBlockIndex& i)
    {
        int h      = H(i);
        auto block = GetBlock(i, h);

        if (block)
        {
            // block already exists
            return block;
        }

        // Create block and insert as the first element.
        int new_index = current_blocks.fetch_add(1);

        if (new_index >= blocks.size())
        {
            blocks.resize(blocks.size() * 2);
        }

        int hash                 = H(i);
        auto* new_block          = &blocks[new_index];
        new_block->index         = i;
        new_block->next_index    = first_hashed_block[hash];
        first_hashed_block[hash] = new_index;
        return new_block;
    }

    bool EraseBlock(const VoxelBlockIndex& i)
    {
        int block_id = GetBlockId(i);
        //        if (block_id < 0) return false;
        SAIGA_ASSERT(block_id >= 0);

        int h = H(i);
        if (!EraseBlockWithHole(i, h)) return false;

        if (block_id == current_blocks - 1)
        {
            // The removed block is at the end of the array -> just remove
            //            std::cout << "remove end" << std::endl;
            current_blocks--;
            return true;
        }
        else
        {
            SAIGA_ASSERT(current_blocks >= 2);
            // The removed block is somewhere in the middle
            // -> Remove last
            auto last_b = blocks[current_blocks - 1];
            SAIGA_ASSERT(last_b.index != i);

            int last_h = H(last_b.index);
            SAIGA_ASSERT(GetBlockId(last_b.index) == current_blocks - 1);

            EraseBlockWithHole(last_b.index, last_h);
            auto* new_block = &blocks[block_id];

            *new_block                 = last_b;
            new_block->next_index      = first_hashed_block[last_h];
            first_hashed_block[last_h] = block_id;
            current_blocks--;
            return true;
        }
    }

    bool EraseBlockWithHole(const VoxelBlockIndex& i, int hash)
    {
        int* block_id_ptr = &first_hashed_block[hash];

        bool found = false;

        while (*block_id_ptr != -1)
        {
            auto& b = blocks[*block_id_ptr];

            if (b.index == i)
            {
                found = true;
                break;
            }

            block_id_ptr = &b.next_index;
        }
        if (!found) return false;

        *block_id_ptr = blocks[*block_id_ptr].next_index;
        return true;
    }

    VoxelBlock* InsertBlockLock(const VoxelBlockIndex& i)
    {
        int h = H(i);

        std::unique_lock lock(hash_locks[h]);

        auto block = GetBlock(i, h);

        if (block)
        {
            // block already exists
            return block;
        }

        // Create block and insert as the first element.
        int new_index = current_blocks.fetch_add(1);

        if (new_index >= blocks.size())
        {
            SAIGA_EXIT_ERROR("Resizing not allowed during parallel insertion!");
        }

        int hash                 = H(i);
        auto* new_block          = &blocks[new_index];
        new_block->index         = i;
        new_block->next_index    = first_hashed_block[hash];
        first_hashed_block[hash] = new_index;
        return new_block;
    }

    void AllocateAroundPoint(const vec3& position, int r = 1)
    {
        auto block_id = GetBlockIndex(position);
        for (int z = -r; z <= r; ++z)
        {
            for (int y = -r; y <= r; ++y)
            {
                for (int x = -r; x <= r; ++x)
                {
                    ivec3 current_id = ivec3(x, y, z) + block_id;
                    InsertBlock(current_id);
                }
            }
        }
    }

    // Returns the 8 voxel ids + weights for a trilinear access
    std::array<std::pair<VoxelIndex, float>, 8> TrilinearAccess(const vec3& position)
    {
        vec3 normalized_pos = (position * voxel_size_inv);
        vec3 ipos           = (normalized_pos).array().floor();
        vec3 frac           = normalized_pos - ipos;


        VoxelIndex corner = ipos.cast<int>();

        std::array<std::pair<VoxelIndex, float>, 8> result;
        result[0] = {corner + ivec3(0, 0, 0), (1.0f - frac.x()) * (1.0f - frac.y()) * (1.0f - frac.z())};
        result[1] = {corner + ivec3(0, 0, 1), (1.0f - frac.x()) * (1.0f - frac.y()) * (frac.z())};
        result[2] = {corner + ivec3(0, 1, 0), (1.0f - frac.x()) * (frac.y()) * (1.0f - frac.z())};
        result[3] = {corner + ivec3(0, 1, 1), (1.0f - frac.x()) * (frac.y()) * (frac.z())};

        result[4] = {corner + ivec3(1, 0, 0), (frac.x()) * (1.0f - frac.y()) * (1.0f - frac.z())};
        result[5] = {corner + ivec3(1, 0, 1), (frac.x()) * (1.0f - frac.y()) * (frac.z())};
        result[6] = {corner + ivec3(1, 1, 0), (frac.x()) * (frac.y()) * (1.0f - frac.z())};
        result[7] = {corner + ivec3(1, 1, 1), (frac.x()) * (frac.y()) * (frac.z())};


        return result;
    }

    // Returns the 8 voxel ids + weights for a trilinear access
    bool TrilinearAccess(const vec3& position, Voxel& result)
    {
        result.distance      = 0;
        result.weight        = 0;
        auto indices_weights = TrilinearAccess(position);

        float w_sum = 0;
        for (auto& iw : indices_weights)
        {
            auto v = GetVoxel(iw.first);
            if (v.weight == 0) return false;

            result.distance += v.distance * iw.second;
            result.weight += v.weight * iw.second;
            w_sum += iw.second;
        }

        SAIGA_ASSERT(std::abs(w_sum - 1) < 0.0001);

        return true;
    }

    // The sdf gradient on the surface (sdf=0) has the same direction as the surface normal.
    vec3 TrilinearGradient(const vec3& position)
    {
        Voxel vx1, vx2;
        TrilinearAccess(position - vec3(voxel_size * 0.5, 0, 0), vx1);
        TrilinearAccess(position + vec3(voxel_size * 0.5, 0, 0), vx2);

        Voxel vy1, vy2;
        TrilinearAccess(position - vec3(0, voxel_size * 0.5, 0), vy1);
        TrilinearAccess(position + vec3(0, voxel_size * 0.5, 0), vy2);

        Voxel vz1, vz2;
        TrilinearAccess(position - vec3(0, 0, voxel_size * 0.5), vz1);
        TrilinearAccess(position + vec3(0, 0, voxel_size * 0.5), vz2);

        vec3 grad = vec3((vx2.distance - vx1.distance), (vy2.distance - vy1.distance), (vz2.distance - vz1.distance)) /
                    float(voxel_size);
        return grad;
    }

    // The normal is the normalized gradient.
    // This function is only valid close to the surface.
    vec3 TrilinearNormal(const vec3& position)
    {
        vec3 grad = TrilinearGradient(position);
        float l   = grad.norm();
        return l < 0.00001 ? grad : grad / l;
    }

    VoxelIndex VirtualVoxelIndex(const vec3& position)
    {
        vec3 normalized_pos = position * voxel_size_inv;
        ivec3 ipos          = normalized_pos.array().round().cast<int>();
        return ipos;
    }

    VoxelBlockIndex GetBlockIndex(VoxelIndex virtual_voxel)
    {
        int x = iFloorDiv(virtual_voxel.x(), VOXEL_BLOCK_SIZE);
        int y = iFloorDiv(virtual_voxel.y(), VOXEL_BLOCK_SIZE);
        int z = iFloorDiv(virtual_voxel.z(), VOXEL_BLOCK_SIZE);
        return {x, y, z};
    }

    VoxelIndex GetLocalOffset(VoxelBlockIndex block, VoxelIndex virtual_voxel)
    {
        VoxelIndex result = virtual_voxel - block * VOXEL_BLOCK_SIZE;

        SAIGA_ASSERT((result.array() >= ivec3::Zero().array()).all());
        SAIGA_ASSERT((result.array() < ivec3(VOXEL_BLOCK_SIZE, VOXEL_BLOCK_SIZE, VOXEL_BLOCK_SIZE).array()).all());
        return result;
    }

    Voxel GetVoxel(VoxelIndex virtual_voxel)
    {
        auto block_id = GetBlockIndex(virtual_voxel);
        auto block    = GetBlock(block_id);
        if (block)
        {
            ivec3 local_offset = GetLocalOffset(block_id, virtual_voxel);
            return block->data[local_offset.z()][local_offset.y()][local_offset.x()];
        }
        else
        {
            return {0, 0};
        }
    }

    float IntersectionLinear(float t1, float t2, float d1, float d2) const { return t1 + (d1 / (d1 - d2)) * (t2 - t1); }

    template <int bisect_iterations>
    bool findIntersectionBisection(vec3 ray_origin, vec3 ray_dir, float t1, float t2, float d1, float d2, float& t)
    {
        float a     = t1;
        float b     = t2;
        float aDist = d1;
        float bDist = d2;
        float c     = 0.0f;
        c           = IntersectionLinear(a, b, aDist, bDist);

        for (int i = 0; i < bisect_iterations; i++)
        {
            SAIGA_ASSERT(c >= t1);
            SAIGA_ASSERT(c <= t2);

            Voxel sample;
            if (!TrilinearAccess(ray_origin + c * ray_dir, sample)) return false;

            float cDist = sample.distance;
            if (aDist * cDist > 0.0)
            {
                a     = c;
                aDist = cDist;
            }
            else
            {
                b     = c;
                bDist = cDist;
            }
            c = IntersectionLinear(a, b, aDist, bDist);
        }

        t = c;

        return true;
    }
    // Intersects the given ray with the implicit surface.
    template <int bisect_iterations>
    float RaySurfaceIntersection(vec3 ray_origin, vec3 ray_dir, float min_t, float max_t, float step,
                                 bool verbose = false)
    {
        float current_t = min_t;
        float last_t;

        Voxel last_sample;
        float current_step = step;

        while (current_t < max_t)
        {
            vec3 current_pos = ray_origin + ray_dir * current_t;

            Voxel current_sample;
            bool tril = TrilinearAccess(current_pos, current_sample);

            if (tril)
            {
                if (verbose)
                {
                    std::cout << "Trace " << current_t << " (" << current_sample.weight << ","
                              << current_sample.distance << ")" << std::endl;
                }

                SAIGA_ASSERT(current_sample.weight > 0);
                if (!verbose && last_sample.weight > 0 && last_sample.distance > 0.0f && current_sample.distance < 0.0f)
                {
                    float t_bi = 0;
                    if (findIntersectionBisection<bisect_iterations>(ray_origin, ray_dir, last_t, current_t,
                                                                     last_sample.distance, current_sample.distance,
                                                                     t_bi))
                    {
                        float result_t = t_bi;
                        SAIGA_ASSERT(result_t >= last_t);
                        SAIGA_ASSERT(result_t <= current_t);
                        return result_t;
                    }
                }

                //                current_step = std::max(voxel_size,std::min(std::abs(current_sample.distance),step));
            }
            else
            {
                //                current_step = step;
            }
            last_sample = current_sample;
            last_t      = current_t;
            current_t += current_step;
        }

        return max_t;
    }

    // Computes the 3D box which contains all valid blocks.
    iRect<3> Bounds();
    int NumBlocksInRect(const iRect<3>& rect);

    // Removes all block, where every weight is 0
    void EraseEmptyBlocks();

    // Erase all blocks not included in rect
    void CropToRect(const iRect<3>& rect);


    VoxelBlockIndex GetBlockIndex(const vec3& position) { return GetBlockIndex(VirtualVoxelIndex(position)); }

    vec3 BlockCenter(const VoxelBlockIndex& i)
    {
        int half_size = VOXEL_BLOCK_SIZE / 2;
        return GlobalPosition(i, half_size, half_size, half_size);
    }

    // The bottom left corner of this voxelblock
    vec3 GlobalBlockOffset(const VoxelBlockIndex& i) { return i.cast<float>() * voxel_size * VOXEL_BLOCK_SIZE; }

    // Global position of one grid point.
    // Arguments: voxel block + relative index in this block
    vec3 GlobalPosition(const VoxelBlockIndex& i, int z, int y, int x)
    {
        return vec3(x, y, z) * voxel_size + GlobalBlockOffset(i);
    }


    using Triangle = std::array<vec3, 3>;

    // Triangle surface extraction on the sparse TSDF.
    // Returns for each block a list of triangles
    std::vector<std::vector<Triangle>> ExtractSurface(double iso, int threads = OMP::getMaxThreads(),
                                                      bool verbose = true);

    // Create a triangle mesh from the list of triangles
    TriangleMesh<VertexNC, uint32_t> CreateMesh(const std::vector<std::vector<Triangle>>& triangles, bool post_process);

    void Compact() { blocks.resize(current_blocks); }

    int Size() { return current_blocks; }

    void ClampDistance(float distance);

    // The number of inserted voxels with weight==0
    int NumZeroVoxels();

    // Sets all voxels to this values
    void SetForAll(float distance, float weight);

   public:
    float voxel_size;
    float voxel_size_inv;

    float block_size_inv;


    unsigned int hash_size;
    std::atomic_int current_blocks = 0;
    std::vector<VoxelBlock> blocks;
    std::vector<int> first_hashed_block;
    std::vector<SpinLock> hash_locks;


    void Clear()
    {
        current_blocks = 0;
        for (auto& b : blocks)
        {
            b = VoxelBlock();
        }
        for (auto& i : first_hashed_block)
        {
            i = -1;
        }
    }

    void Save(const std::string& file);
    void Load(const std::string& file);

    // Use zlib compression.
    // Only valid if saiga was compiled with zlib support.
    void SaveCompressed(const std::string& file);
    void LoadCompressed(const std::string& file);

    bool operator==(const SparseTSDF& other) const;

    int H(const VoxelBlockIndex& i)
    {
        unsigned int u = i.x() + i.y() * 1000 + i.z() * 1000 * 1000;
        int result     = u % hash_size;
        SAIGA_ASSERT(result >= 0 && result < hash_size);
        return result;
    }


    int GetBlockId(const VoxelBlockIndex& i) { return GetBlockId(i, H(i)); }

    // Returns the actual (memory) block id
    // returns -1 if it does not exist
    int GetBlockId(const VoxelBlockIndex& i, int hash)
    {
        int block_id = first_hashed_block[hash];

        while (block_id != -1)
        {
            auto* block = &blocks[block_id];
            if (block->index == i)
            {
                break;
            }

            block_id = block->next_index;
        }
        return block_id;
    }

    VoxelBlock* GetBlock(const VoxelBlockIndex& i, int hash)
    {
        auto id = GetBlockId(i, hash);
        if (id >= 0)
        {
            return &blocks[id];
        }
        else
        {
            return nullptr;
        }
    }
};



}  // namespace Saiga
