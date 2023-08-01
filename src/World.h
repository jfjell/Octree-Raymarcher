#pragma once

#ifndef WORLD_H
#define WORLD_H

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include "Allocator.h"

struct Ocroot;
struct Ocdelta;
struct BoundsPyramid;

struct WorldGPUContext
{
    unsigned int vao, vbo, ebo;
    unsigned int shader_near, shader_far;
    unsigned int *ssbo_tree, *ssbo_twig;
    int model_near, mvp_near, bmin_near, size_near, eye_near, depth_near, width_near;
    int model_far, mvp_far, bmin_far, size_far, eye_far, depth_far, width_far;
};

struct GPUChunk
{
    glm::vec3 bmin;
    int tr_reg, tr_off;
    int tw_reg, tw_off;
};

struct World
{
    RootAllocator allocator;
    Ocroot *chunk;
    GPUChunk *gcd;
    BoundsPyramid *heightmap;
    WorldGPUContext gpu;
    int width, height, depth, plane, volume, chunksize;
    glm::ivec3 chunkcoordmin;
    unsigned int vao, vbo, ebo, shader, chunk_ssbo;
    int chunkmin_ul, chunkmax_ul, chunksize_ul, chunks_ul, w_ul, h_ul, d_ul, eye_ul;

    glm::ivec3 index_float(glm::vec3 p) const;
    int index(int x, int y, int z) const;
    int index(int x, int z) const;
    void init(int w, int h, int d, int s);
    void deinit();
    void load_gpu();
    void draw(glm::mat4 mvp, glm::vec3 eye, int w, unsigned int ssbo_color);
    void modify(int i, const Ocdelta *tree, const Ocdelta *twig);
    void g_pyramid(int x, int z);
    void g_chunk(int x, int y, int z);
    void shift(glm::ivec3 s);
};

#endif