#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <glm/vec3.hpp>
#include <GL/glew.h>
#include "Draw.h"

Mesh::~Mesh()
{
    glDeleteBuffers(1, &this->vbo);
    glDeleteBuffers(1, &this->uv);
    glDeleteBuffers(1, &this->ebo);
}

void Mesh::add(const float *v, size_t nv, 
               glm::vec3 tv, glm::vec3 sv, 
               const unsigned *i, size_t ni, 
               const float *u, size_t nu)
{
    assert(this->vtxcoords.size() % 3 == 0);

    size_t si = this->vtxcoords.size() / 3; // Shift the indices by what is already there

    for (size_t j = 0; j < nv; ++j)
        this->vtxcoords.push_back(v[j] * sv[j % 3] + tv[j % 3]);

    for (size_t j = 0; j < ni; ++j)
        this->indices.push_back(i[j] + (unsigned int)si);

    this->uvcoords.insert(this->uvcoords.end(), u, u + nu);
}

void Mesh::bind()
{
    assert(this->vtxcoords.size() > 0);
    assert(this->vtxcoords.size() % 3 == 0);
    assert(this->uvcoords.size() % 3 == 0 || this->uvcoords.size() % 2 == 0);

    int vao;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    assert(vao != 0);

    // Vertices
    glGenBuffers(1, &this->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    glBufferData(GL_ARRAY_BUFFER, this->vtxcoords.size() * sizeof(float), this->vtxcoords.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    if (this->indices.size()) // Indices
    {
        glGenBuffers(1, &this->ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size() * sizeof(unsigned int), this->indices.data(), GL_STATIC_DRAW);
    }

    if (this->uvcoords.size()) // UV
    {
        size_t pv = this->uvcoords.size() * 3 / this->vtxcoords.size();
        glGenBuffers(1, &this->uv);
        glBindBuffer(GL_ARRAY_BUFFER, this->uv);
        glBufferData(GL_ARRAY_BUFFER, this->uvcoords.size() * sizeof(float), this->uvcoords.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(1, (int)pv, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(1);
    }
}

/* aQuadVertices: flat quad when looking straight at a-axis
 * Vertices should be specified in counter-clockwise order!
 */
const float zQuadVertices[12] = {
    0.0, 0.0, 0.0, // Bottom left
    0.0, 1.0, 0.0, // Top left
    1.0, 0.0, 0.0, // Bottom right
    1.0, 1.0, 0.0, // Top right
};

const float yQuadVertices[12] = {
    0.0, 0.0, 0.0, // Bottom left
    0.0, 0.0, 1.0, // Top left
    1.0, 0.0, 0.0, // Bottom right
    1.0, 0.0, 1.0, // Top right
};

const float xQuadVertices[12] = {
    0.0, 0.0, 0.0, // Bottom left
    0.0, 1.0, 0.0, // Top left
    0.0, 0.0, 1.0, // Bottom right
    0.0, 1.0, 1.0, // Top right
};

const float quadUV[8] = {
    0.0, 1.0, // Top left
    0.0, 0.0, // Bottom left
    1.0, 1.0, // Top right
    1.0, 0.0, // Bottom right
};

const unsigned int quadIndices[6] = {
    3, 1, 2,
    0, 2, 1
};

void Mesh::cubeface(glm::vec3 t, glm::vec3 s)
{
    using glm::vec3;

    const vec3 x1 = t, x2 = x1 + vec3(s.x, 0.f, 0.f);
    const vec3 y1 = t, y2 = y1 + vec3(0.f, s.y, 0.f);
    const vec3 z1 = t, z2 = z1 + vec3(0.f, 0.f, s.z);

    add(xQuadVertices, 12, x1, s, quadIndices, 6, quadUV, 8);
    add(xQuadVertices, 12, x2, s, quadIndices, 6, quadUV, 8);
    add(yQuadVertices, 12, y1, s, quadIndices, 6, quadUV, 8);
    add(yQuadVertices, 12, y2, s, quadIndices, 6, quadUV, 8);
    add(zQuadVertices, 12, z1, s, quadIndices, 6, quadUV, 8);
    add(zQuadVertices, 12, z2, s, quadIndices, 6, quadUV, 8);
}

/*
      4----5
     /|   /|
    6----7 |
    | |  | |
    | 0--|-1
    |/   |/
    2----3
*/
const float cubeVertices[8*3] = {
    0., 0., 0., 
    1., 0., 0., 
    0., 0., 1., 
    1., 0., 1., 
    0., 1., 0., 
    1., 1., 0., 
    0., 1., 1., 
    1., 1., 1., 
};

#define N (1.0f / 3.0f) // Normalized value of (.5, .5, .5)

const float cubeUV[8*3] = {
    -N, -N, -N,
    +N, -N, -N,
    -N, -N, +N,
    +N, -N, +N,
    -N, +N, -N,
    +N, +N, -N,
    -N, +N, +N,
    +N, +N, +N,
};

#undef N

const unsigned int cubeIndices[6*6] = {
    4, 0, 5,
    0, 1, 5,
    6, 2, 4,
    2, 0, 4,
    7, 3, 6,
    3, 2, 6,
    5, 1, 7,
    1, 3, 7,
    0, 2, 1,
    2, 3, 1,
    5, 7, 4,
    7, 6, 4,
};

void Mesh::cubemap(glm::vec3 t, glm::vec3 s)
{
    add(cubeVertices, 8*3, t, s, cubeIndices, 6*6, cubeUV, 8*3);
}