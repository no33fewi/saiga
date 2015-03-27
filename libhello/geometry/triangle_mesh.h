#pragma once

#include "libhello/util/glm.h"

#include "libhello/opengl/indexedVertexBuffer.h"
#include "libhello/geometry/aabb.h"
#include "libhello/geometry/triangle.h"


#include <cstring>
/*
 * Data structur for simple triangle meshes.
 * Can be turned into a IndexedVertexBuffer for drawing with OpenGL
 */

template<typename vertex_t, typename index_t>
class TriangleMesh
{
public:
    struct Face{
        index_t v1,v2,v3;
        Face(){}
        Face(const index_t& v1,const index_t& v2,const index_t& v3):v1(v1),v2(v2),v3(v3){}
    };

    typedef IndexedVertexBuffer<vertex_t,index_t> buffer_t;



    /*
     * Create empty triangle mesh
     */

    TriangleMesh(void);
    ~TriangleMesh(void){}

    /*
     * Transforms mesh with given matrix.
     * All vertices are multiplied with 'trafo'
     */

    void transform(const mat4 &trafo);
    void transformNormal(const mat4 &trafo);

    /*
     * Deletes all vertices and faces.
     */

    void clear(){vertices.resize(0);faces.resize(0);}

    /*
     * Adds vertex to mesh and updates enclosing aabb.
     * return: index of new vertex
     */

    int addVertex(const vertex_t &v){vertices.push_back(v);boundingBox.growBox(v.position);return vertices.size()-1;}

    /*
     * Adds face to mesh.
     * The indices of the face should match existing vertices
     * return: index of new face
     */

    int addFace(const Face &f){faces.push_back(f);return faces.size()-1;}

    int addFace(index_t f[3]){return addFace(Face(f[0],f[1],f[2]));}

    /*
     * Adds given vertices and the 2 corresponding triangles to mesh
     */

    void addQuad(vertex_t verts[4]);

    /*
     * Adds 2 Triangles given by 4 vertices and form a quad.
     * The vertices should be orderd counter clockwise
     */

    void addQuad(index_t inds[4]);

    /*
     * Creates OpenGL buffer from indices and vertices
     * 'buffer' is now ready to draw.
     */

    void createBuffers(buffer_t &buffer);

    /*
     * Updates OpenGL buffer with the data currently saved in this mesh
     * see VertexBuffer::updateVertexBuffer for more details
     */

    void updateVerticesInBuffer(buffer_t &buffer,int vertex_count, int vertex_offset);

    /*
     * Subdivides the triangle at index 'face' into 4 triangles.
     * The new triangles will be added to the mesh and the old will be overwritten
     */

    void subdivideFace(int face);

    /*
     * Inverts the triangle at index 'face'.
     * The order of the indices will be reversed.
     */

    void invertFace(int face);

    /*
     * Converts the index face data structur to a simple triangle list.
     */

    void toTriangleList(std::vector<Triangle> &output);

    /*
     * Adds the complete mesh 'other' to the current mesh.
     */
    void addMesh(const TriangleMesh<vertex_t,index_t> &other);


    aabb calculateAabb();


    template<typename v, typename i>
    friend std::ostream& operator<<(std::ostream& os, const TriangleMesh<v,i>& dt);

    aabb& getAabb(){return boundingBox;}
public:
    std::vector<vertex_t> vertices;
    std::vector<Face> faces;
    aabb boundingBox;
};



template<typename vertex_t, typename index_t>
TriangleMesh<vertex_t,index_t>::TriangleMesh(void){
   boundingBox.makeNegative();
}

template<typename vertex_t, typename index_t>
void TriangleMesh<vertex_t,index_t>::transform(const mat4 &trafo){
  for(vertex_t &v : vertices){
      v.position = vec3(trafo*vec4(v.position,1));
  }
  boundingBox.transform(trafo);
}

template<typename vertex_t, typename index_t>
void TriangleMesh<vertex_t,index_t>::transformNormal(const mat4 &trafo){
  for(vertex_t &v : vertices){
      v.normal = vec3(trafo*vec4(v.normal,0));
  }
}



template<typename vertex_t, typename index_t>
void TriangleMesh<vertex_t,index_t>::addQuad(vertex_t verts[]){
  int index = vertices.size();
  for(int i=0;i<4;i++){
      addVertex(verts[i]);
  }

  faces.push_back(Face(index,index+1,index+2));
  faces.push_back(Face(index,index+2,index+3));
}

template<typename vertex_t, typename index_t>
void TriangleMesh<vertex_t,index_t>::addQuad(index_t inds[]){
  faces.push_back(Face(inds[0],inds[1],inds[2]));
  faces.push_back(Face(inds[2],inds[3],inds[0]));
}

template<typename vertex_t, typename index_t>
void TriangleMesh<vertex_t,index_t>::createBuffers(buffer_t &buffer){
  std::vector<index_t> indices(faces.size()*3);
  std::memcpy(&indices[0],&faces[0],faces.size()*sizeof( Face));
  buffer.set(vertices,indices);
  buffer.setDrawMode(GL_TRIANGLES);
}

template<typename vertex_t, typename index_t>
void TriangleMesh<vertex_t,index_t>::updateVerticesInBuffer(buffer_t &buffer, int vertex_count, int vertex_offset){
  buffer.updateVertexBuffer(&vertices[vertex_offset],vertex_count,vertex_offset);
}

template<typename vertex_t, typename index_t>
std::ostream& operator<<(std::ostream& os, const TriangleMesh<vertex_t,index_t>& dt){
  os<<"TriangleMesh. Faces: "<<dt.faces.size()<<" Vertices: "<<dt.vertices.size();
  return os;
}

template<typename vertex_t, typename index_t>
void TriangleMesh<vertex_t,index_t>::subdivideFace(int f){

  Face face = faces[f];

#define P(xs) vertices[face.xs].position
  //create 3 new vertices in the middle of the edges

  int v1 = addVertex(vertex_t((P(v1)+P(v2))/2.0f));
  int v2 = addVertex(vertex_t((P(v1)+P(v3))/2.0f));
  int v3 = addVertex(vertex_t((P(v2)+P(v3))/2.0f));


  faces.push_back(Face(face.v2,v3,v1));

  faces.push_back(Face(face.v3,v2,v3));

  faces.push_back(Face(v1,v3,v2));
  faces[f] = Face(face.v1,v1,v2);
}



template<typename vertex_t, typename index_t>
void TriangleMesh<vertex_t,index_t>::invertFace(int f){
  Face& face = faces[f];

  Face face2;
  face2.v1 = face.v3;
  face2.v2 = face.v2;
  face2.v3 = face.v1;

  face = face2;

}

template<typename vertex_t, typename index_t>
void TriangleMesh<vertex_t,index_t>::toTriangleList(std::vector<Triangle> &output){
    Triangle t;
    for(Face &f : faces){
        t.a = vertices[f.v1].position;
        t.b = vertices[f.v2].position;
        t.c = vertices[f.v3].position;
        output.push_back(t);
    }
}

template<typename vertex_t, typename index_t>
 void TriangleMesh<vertex_t,index_t>::addMesh(const TriangleMesh<vertex_t,index_t> &other){
    int oldVertexCount = this->vertices.size();
    for(vertex_t v : other.vertices){
        this->vertices.push_back(v);
    }

    for(Face f : other.faces){
        f.v1 += oldVertexCount;
        f.v2 += oldVertexCount;
        f.v3 += oldVertexCount;
        this->addFace(f);
    }
 }


 template<typename vertex_t, typename index_t>
 aabb TriangleMesh<vertex_t,index_t>::calculateAabb(){
     aabb box;
     box.makeNegative();

     for(vertex_t &v : vertices){
         box.growBox(v.position);
     }
     return box;
 }
