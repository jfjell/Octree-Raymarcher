#pragma once

#ifndef DRAW_H
#define DRAW_H

#include <vector>
#include <GL/glew.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

struct Shader
{
    static unsigned compileAttach(const char *path, GLenum type, unsigned program);
    static unsigned link(unsigned program);
};

struct Mesh
{
    std::vector<float>    vtxcoords;
    std::vector<float>    uvcoords;
    std::vector<unsigned> indices;
    unsigned vbo = 0, uv = 0, ebo = 0;

    ~Mesh();

    void add(const float *v, size_t nv, 
             glm::vec3 tv, glm::vec3 sv, 
             const unsigned *i, size_t ni, 
             const float *u, size_t nu);
    void cube(glm::vec3 t, glm::vec3 s);

    void bind();
};

struct Ocroot;

struct OctreeCubeDrawer
{
    Mesh mesh;
    unsigned vao, shader, tex;
    int mvp, sampler;

    ~OctreeCubeDrawer();

    void loadTree(const Ocroot *root);
    void loadGL(const char *texture);
    void draw(const glm::mat4 MVP);
};

#endif