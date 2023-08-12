#pragma once

#ifndef WORLD_H
#define WORLD_H

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include "Allocator.h"
#include "Atlas.h"

struct Ocroot;
struct Ocdelta;
struct BoundsPyramid;

struct GPUChunk
{
    glm::vec3 bmin;
    uint32_t tr_reg, tr_off;
    uint32_t tw_reg, tw_off;
    uint32_t _align;

    GPUChunk() = default;
    GPUChunk(const Ocroot *r, RootAllocation a);
};

static_assert(sizeof(GPUChunk) % 32 == 0);

struct WorldShaderContext
{
    static constexpr int CHUNK_SSBO_BINDING = 2;
    static constexpr int TREE1_SSBO_BINDING = 4;
    static constexpr int TWIG1_SSBO_BINDING = 5;

    unsigned int shader;
    int chunkmin_ul, chunkmax_ul, chunksize_ul, w_ul, h_ul, d_ul, eye_ul, model_ul, mvp_ul;
    int diffuse_ul, specular_ul;

    WorldShaderContext(unsigned int s = 0) : shader(s) { }
    void bind_ul();
};

struct World
{
    RootAllocator allocator;
    Ocroot *chunk;
    GPUChunk *gcd;
    BoundsPyramid *heightmap;
    int width, height, depth, plane, volume, chunksize;
    glm::ivec3 chunkcoordmin;
    TextureAtlas atlas;
    WorldShaderContext shader_context;
    unsigned int vao, vbo, ebo, chunk_ssbo;

    glm::ivec3 index_float(glm::vec3 p) const;
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