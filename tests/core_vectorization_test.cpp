﻿/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "saiga/config.h"
#include "saiga/core/math/Eigen_Compile_Checker.h"
#include "saiga/core/math/all.h"
#include "saiga/core/util/Align.h"
#include "saiga/core/util/ConsoleColor.h"
#include "saiga/core/util/table.h"

#include "gtest/gtest.h"

namespace Saiga
{
Table* table;
void PrintVectorEnabled(const std::string& intruction_set, bool enabled)
{
    (*table) << intruction_set << (enabled ? ConsoleColor::GREEN : ConsoleColor::RED) << (enabled ? "YES" : "NO")
             << ConsoleColor::RESET;
}

void CheckVectorInstructions()
{
    table = new Table({10, 0, 1, 0});
    EigenHelper::EigenCompileFlags flags;
    flags.create<938476>();

    std::cout << "Enabled Vector Instructions:" << std::endl;
    PrintVectorEnabled("fma", flags.fma);
    PrintVectorEnabled("sse3", flags.sse3);
    PrintVectorEnabled("ssse3", flags.ssse3);
    PrintVectorEnabled("sse41", flags.sse41);
    PrintVectorEnabled("sse42", flags.sse42);
    PrintVectorEnabled("avx", flags.avx);
    PrintVectorEnabled("avx2", flags.avx2);
    PrintVectorEnabled("avx512", flags.avx512);
    PrintVectorEnabled("neon", flags.neon);
    PrintVectorEnabled("vsx", flags.vsx);
    PrintVectorEnabled("altivec", flags.altivec);
    PrintVectorEnabled("zvector", flags.zvector);
    std::cout << std::endl;
}
void CheckEigenVectorAlignment()
{
    Table table({20, 10, 10});

    table << "Type"
          << "size"
          << "alignment";

    using vec8  = Eigen::Vector<float, 8>;
    using vec16 = Eigen::Vector<float, 16>;
    using vec32 = Eigen::Vector<float, 32>;

    table << "vec2<float>" << sizeof(vec2) << alignof(vec2);
    table << "vec3<float>" << sizeof(vec3) << alignof(vec3);
    table << "vec4<float>" << sizeof(vec4) << alignof(vec4);
    table << "vec8<float>" << sizeof(vec8) << alignof(vec8);
    table << "vec16<float>" << sizeof(vec16) << alignof(vec16);
    table << "vec32<float>" << sizeof(vec32) << alignof(vec32);

    using Vec8  = Eigen::Vector<double, 8>;
    using Vec16 = Eigen::Vector<double, 16>;
    using Vec32 = Eigen::Vector<double, 32>;

    table << ""
          << ""
          << "";
    table << "Vec2<double>" << sizeof(Vec2) << alignof(Vec2);
    table << "Vec3<double>" << sizeof(Vec3) << alignof(Vec3);
    table << "Vec4<double>" << sizeof(Vec4) << alignof(Vec4);
    table << "Vec8<double>" << sizeof(Vec8) << alignof(Vec8);
    table << "Vec16<double>" << sizeof(Vec16) << alignof(Vec16);
    table << "Vec32<double>" << sizeof(vec32) << alignof(Vec32);


    std::cout << std::endl;
}

#ifdef EIGEN_VECTORIZE_NEON
#    include <arm_neon.h>
TEST(Vectorization, Neon)
{
    unsigned int data[4] = {1, 2, 3, 4};

    volatile uint32x4_t first  = vld1q_u32(data);
    volatile uint32x4_t second = vld1q_u32(data);
    volatile uint32x4_t result;

    result = vaddq_u32(first, second);

    vst1q_u32(data, result);

    int sum = 0;
    int ref = 0;
    for (int i = 0; i < 4; ++i)
    {
        sum += data[i];
        ref += 2 * (i + 1);
    }
    EXPECT_EQ(sum, ref);
}
#endif

#ifdef EIGEN_VECTORIZE_SSE
TEST(Vectorization, SSE)
{
    volatile __m128i first  = _mm_set_epi32(1, 2, 3, 4);
    volatile __m128i second = _mm_set_epi32(1, 2, 3, 4);
    volatile __m128i result = _mm_add_epi32(first, second);
    volatile int* values    = (int*)&result;

    int sum = 0;
    int ref = 0;
    for (int i = 0; i < 4; ++i)
    {
        sum += values[i];
        ref += 2 * (i + 1);
    }
    EXPECT_EQ(sum, ref);
}
#endif

#ifdef EIGEN_VECTORIZE_AVX2
TEST(Vectorization, AVX2)
{
    volatile __m256i first  = _mm256_set_epi32(1, 2, 3, 4, 5, 6, 7, 8);
    volatile __m256i second = _mm256_set_epi32(1, 2, 3, 4, 5, 6, 7, 8);
    volatile __m256i result = _mm256_add_epi32(first, second);
    volatile int* values    = (int*)&result;

    int sum = 0;
    int ref = 0;
    for (int i = 0; i < 8; ++i)
    {
        sum += values[i];
        ref += 2 * (i + 1);
    }
    EXPECT_EQ(sum, ref);
}
#endif


#ifdef EIGEN_VECTORIZE_AVX512
TEST(Vectorization, AVX512)
{
    volatile __m512i first  = _mm512_set_epi32(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    volatile __m512i second = _mm512_set_epi32(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    volatile __m512i result = _mm512_add_epi32(first, second);
    volatile int* values    = (int*)&result;

    int sum = 0;
    int ref = 0;
    for (int i = 0; i < 16; ++i)
    {
        sum += values[i];
        ref += 2 * (i + 1);
    }
    EXPECT_EQ(sum, ref);
}
#endif


}  // namespace Saiga

int main()
{
    Saiga::CheckVectorInstructions();
    Saiga::CheckEigenVectorAlignment();
    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}
