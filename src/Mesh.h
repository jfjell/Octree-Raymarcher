#pragma once

#include <vector>
#include "Shader.h"

extern const float zQuadVertices[12];
extern const float yQuadVertices[12];
extern const float xQuadVertices[12];
extern const float quadUV[8];
extern const unsigned short quadIndices[6];

struct Mesh
{
    std::vector<float>          vertices;
    std::vector<float>          uv;
    std::vector<unsigned short> indices;

    unsigned int vao, vbo[2], ebo, tex;

    Mesh();
    ~Mesh();
    void finalize();
    void add(const float *vertex, 
             size_t vertices,
             const float translate[3],
             const float scale[3],
             const float *uv, 
             const unsigned short *index,
             size_t indices);
    void optimize(float gran);
};

void addCube(Mesh& m, const glm::vec3& offset, const glm::vec3& size);
