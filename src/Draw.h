#pragma once

#ifndef DRAW_H
#define DRAW_H

#include <vector>
#include <GL/glew.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

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
    void cubeface(glm::vec3 t, glm::vec3 s);
    void cubemap(glm::vec3 t, glm::vec3 s);

    void bind();
};

struct Ocroot;

struct OctreeCubefaceDrawer
{
    Mesh mesh;
    unsigned vao, shader, tex;
    int mvp, sampler;

    ~OctreeCubefaceDrawer();

    void loadTree(const Ocroot *root);
    void loadGL(const char *texture);
    void draw(const glm::mat4 MVP);
};

struct OctreeCubemapDrawer
{
    Mesh mesh;
    unsigned vao, shader, tex;
    int mvp, sampler;


    ~OctreeCubemapDrawer();

    void loadTree(const Ocroot *root);
    void loadGL(const char *t);
    void draw(const glm::mat4 MVP);
};

struct ParallaxDrawer
{
    const Ocroot *root;
    unsigned vao, vbo, ebo, shader, ssboTree, ssboTwig;
    int mvp, sampler, eye, pos, size, model;
    glm::mat4 srt;

    void destroy();

    void loadTree(const Ocroot *root);
    void loadGL(const char *t);
    void pre(const glm::mat4 MVP, glm::vec3 position);
    void draw();
    void post();
};

#endif