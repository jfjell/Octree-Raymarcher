#pragma once

#ifndef WORLD_H
#define WORLD_H

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

struct Ocroot;
struct Ocdelta;
struct BoundsPyramid;

struct WorldGPUContext
{
    unsigned int vao, vbo, ebo;
    unsigned int shader_near, shader_far;
    unsigned int *ssbo_tree, *ssbo_twig;
    int model_near, mvp_near, bmin_near, size_near, eye_near;
    int model_far, mvp_far, bmin_far, size_far, eye_far;
};

struct World
{
    Ocroot *chunk;
    BoundsPyramid *heightmap;
    WorldGPUContext gpu;
    int *order;
    int width, height, depth, plane, volume, chunksize;
    glm::ivec3 bmin, bmax;

    int index(int x, int y, int z) const;
    int index(int x, int z) const;
    void init(int w, int h, int d, int s);
    void deinit();
    void load_gpu();
    void draw(glm::mat4 mvp, glm::vec3 eye);
    void modify(int i, const Ocdelta *tree, const Ocdelta *twig);
    void g_pyramid(int x, int z);
    void g_chunk(int x, int y, int z);
    void shift(glm::ivec3 s);
};

#endif