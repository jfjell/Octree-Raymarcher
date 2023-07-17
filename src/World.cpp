#include <algorithm>
#include <GL/glew.h>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "World.h"
#include "Octree.h"
#include "BoundsPyramid.h"
#include "Shader.h"
#include "Traverse.h"

#define TREE_MAX_DEPTH 9
#define PYRAMID_RESOLUTION 64

using glm::vec3;
using glm::ivec3;
using glm::mat4;

void World::init(int w, int h, int d, int s)
{
    width  = w;
    height = h;
    depth  = d;
    plane  = width * depth;
    volume = plane * height;
    chunksize = s;

    bmin = ivec3(0);
    bmax = ivec3(width, height, depth);

    heightmap = new BoundsPyramid[plane];
    for (int z = 0; z < depth; ++z)
        for (int x = 0; x < width; ++x)
            g_pyramid(x, z);

    chunk = new Ocroot[volume];
    for (int z = 0; z < depth; ++z)
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                g_chunk(x, y, z);

    order = new int[volume];
    for (int i = 0; i < volume; ++i)
        order[i] = i;
}

extern const float CUBE_VERTICES[8*3];
extern const unsigned short CUBE_INDICES[6*6];

void World::load_gpu()
{
    glGenVertexArrays(1, &gpu.vao);
    glBindVertexArray(gpu.vao);

    glGenBuffers(1, &gpu.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE_VERTICES), CUBE_VERTICES, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &gpu.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(CUBE_INDICES), CUBE_INDICES, GL_STATIC_DRAW);

    gpu.ssbo_tree = new unsigned int[volume];
    glGenBuffers(volume, gpu.ssbo_tree);
    for (int i = 0; i < volume; ++i)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu.ssbo_tree[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, chunk[i].treestoragesize * sizeof(Octree), chunk[i].tree, GL_STATIC_DRAW); 
        // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbotree[i]);
    }

    gpu.ssbo_twig = new unsigned int[volume];
    glGenBuffers(volume, gpu.ssbo_twig);
    for (int i = 0; i < volume; ++i)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu.ssbo_twig[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, chunk[i].twigstoragesize * sizeof(Octwig), chunk[i].twig, GL_STATIC_DRAW); 
        // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbotwig[i]);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    gpu.shader_near = Shader(glCreateProgram())
        .vertex("shaders/Parallax.Vertex.glsl")
        .include("shaders/Raymarch.glsl")
        .fragment("shaders/ParallaxN.Fragment.glsl")
        .link();
    
    gpu.model_near = glGetUniformLocation(gpu.shader_near, "model");
    gpu.mvp_near   = glGetUniformLocation(gpu.shader_near, "mvp");
    gpu.eye_near   = glGetUniformLocation(gpu.shader_near, "eye");
    gpu.bmin_near  = glGetUniformLocation(gpu.shader_near, "root.pos");
    gpu.size_near  = glGetUniformLocation(gpu.shader_near, "root.size");

    gpu.shader_far = Shader(glCreateProgram())
        .vertex("shaders/Parallax.Vertex.glsl")
        .include("shaders/Raymarch.glsl")
        .fragment("shaders/ParallaxF.Fragment.glsl")
        .link();

    gpu.model_far = glGetUniformLocation(gpu.shader_far, "model");
    gpu.mvp_far   = glGetUniformLocation(gpu.shader_far, "mvp");
    gpu.eye_far   = glGetUniformLocation(gpu.shader_far, "eye");
    gpu.bmin_far  = glGetUniformLocation(gpu.shader_far, "root.pos");
    gpu.size_far  = glGetUniformLocation(gpu.shader_far, "root.size");
}

void World::deinit()
{
    glDeleteVertexArrays(1, &gpu.vao);
    glDeleteBuffers(1, &gpu.vbo);
    glDeleteBuffers(1, &gpu.ebo);
    glDeleteBuffers(volume, gpu.ssbo_tree);
    glDeleteBuffers(volume, gpu.ssbo_twig);
    glDeleteProgram(gpu.shader_near);
    glDeleteProgram(gpu.shader_far);

    delete[] gpu.ssbo_tree;
    delete[] gpu.ssbo_twig;

    for (int i = 0; i < volume; ++i)
    {
        free(chunk[i].tree);
        free(chunk[i].twig);
    }
    delete[] chunk;

    for (int i = 0; i < plane; ++i)
        heightmap[i].deinit();

    delete[] heightmap;

    delete[] order;
}

static float cubeDF(vec3 p, vec3 bmin, vec3 bmax)
{
    vec3 bmid = (bmax + bmin) * 0.5f;
    return glm::distance(p, bmid);
}

static float chunkDF(vec3 p, const Ocroot *chunk)
{
    return cubeDF(p, chunk->position, chunk->position + chunk->size);
}

static glm::mat4 computeSRT(const Ocroot *chunk)
{
    mat4 T = glm::translate(mat4(1.0), chunk->position);
    mat4 S = glm::scale(mat4(1.0), vec3(chunk->size));
    mat4 R = mat4(1.0);
    mat4 SRT = T * R * S;
    return SRT;
}

void World::draw(glm::mat4 mvp, glm::vec3 eye)
{
    auto less = [=](int i, int j) { return chunkDF(eye, &chunk[i]) < chunkDF(eye, &chunk[j]); };
    std::sort(&order[0], &order[volume], less);

    int midpoint = volume;
    for (int i = 0; i < midpoint; ++i)
    {
        int j = order[i];
        if (!isInsideCube(eye, chunk[j].position, chunk[j].position + chunk[j].size))
            midpoint = i;
    }

    glDisable(GL_STENCIL_TEST);

    glEnable(GL_CULL_FACE);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glBindVertexArray(gpu.vao);

    // Near
    glUseProgram(gpu.shader_near);

    glUniformMatrix4fv(gpu.mvp_near, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3fv(gpu.eye_near, 1, glm::value_ptr(eye));

    for (int i = 0; i < midpoint; ++i)
    {
        int j = order[i];

        mat4 srt = computeSRT(&chunk[j]);
        glUniformMatrix4fv(gpu.model_near, 1, GL_FALSE, glm::value_ptr(srt));
        glUniform3fv(gpu.bmin_near, 1, glm::value_ptr(chunk[j].position));
        glUniform1f(gpu.size_near, chunk[j].size);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpu.ssbo_tree[j]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpu.ssbo_twig[j]);

        glDrawElements(GL_TRIANGLES, sizeof(CUBE_INDICES) / sizeof(unsigned short), GL_UNSIGNED_SHORT, (void *)0);
    }

    // Far
    glUseProgram(gpu.shader_far);

    glUniformMatrix4fv(gpu.mvp_far, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3fv(gpu.eye_far, 1, glm::value_ptr(eye));

    for (int i = midpoint; i < volume; ++i)
    {
        int j = order[i];

        mat4 srt = computeSRT(&chunk[j]);
        glUniformMatrix4fv(gpu.model_far, 1, GL_FALSE, glm::value_ptr(srt));
        glUniform3fv(gpu.bmin_far, 1, glm::value_ptr(chunk[j].position));
        glUniform1f(gpu.size_far, chunk[j].size);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpu.ssbo_tree[j]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpu.ssbo_twig[j]);

        glDrawElements(GL_TRIANGLES, sizeof(CUBE_INDICES) / sizeof(unsigned short), GL_UNSIGNED_SHORT, (void *)0);
    }

    glBindVertexArray(0);
    glUseProgram(0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void World::modify(int i, const Ocdelta *tree, const Ocdelta *twig)
{
    if (tree->left < tree->right || tree->realloc)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu.ssbo_tree[i]);
        if (tree->realloc)
            glBufferData(GL_SHADER_STORAGE_BUFFER, chunk[i].treestoragesize * sizeof(Octree), chunk[i].tree, GL_STATIC_DRAW); 
        else
        {
            size_t offset = tree->left * sizeof(Octree);
            size_t size = (tree->right - tree->left) * sizeof(Octree);
            void *pointer = &chunk[i].tree[tree->left];
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, pointer);
        }
    }

    if (twig->left < twig->right || twig->realloc)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu.ssbo_twig[i]);
        if (twig->realloc)
            glBufferData(GL_SHADER_STORAGE_BUFFER, chunk[i].twigstoragesize * sizeof(Octwig), chunk[i].twig, GL_STATIC_DRAW); 
        else
        {
            size_t offset = twig->left * sizeof(Octwig);
            size_t size = (twig->right - twig->left) * sizeof(Octwig);
            void *pointer = &chunk[i].twig[twig->left];
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, pointer);
        }
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

static int mod(int n, int m)
{
    return (n + (n / m + 1) * m) % m;
}

int World::index(int x, int z) const
{
    int i = (mod(z, depth) * width) + mod(x, width);
    assert(i >= 0 && i < width * depth);
    return i;
}

int World::index(int x, int y, int z) const
{
    int i = (mod(y, height) * width * depth) + (mod(z, depth) * width) + mod(x, width);
    assert(i >= 0 && i < width * height * depth);
    return i;
}

// Both of these currently leak memory
void World::g_pyramid(int x, int z)
{
    int i = index(x, z);
    float amplitude = 16;
    float period = 1.0f / PYRAMID_RESOLUTION;
    float xshift = (float)x * PYRAMID_RESOLUTION;
    float yshift = 16.0f;
    float zshift = (float)z * PYRAMID_RESOLUTION;
    heightmap[i] = BoundsPyramid();
    heightmap[i].init(PYRAMID_RESOLUTION, amplitude, period, xshift, yshift, zshift);
}

void World::g_chunk(int x, int y, int z)
{
    int i = index(x, y, z);
    int j = index(x, z);
    vec3 p = vec3(x * chunksize, (y - (height / 2)) * chunksize, z * chunksize);
    chunk[i] = Ocroot();
    grow(&chunk[i], p, chunksize, TREE_MAX_DEPTH, &heightmap[j]);
}

void World::shift(glm::ivec3 s)
{
    assert(glm::length(vec3(s)) == 1.0);

    ivec3 bounds(width, height, depth);

    for (size_t i = 0; i < 3; ++i)
        assert(bmax[i] - bmin[i] == bounds[i]);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int x2 = bmin.x + x;
            int y2 = bmin.y + y;
            int z = bmax.z;
            int z2 = z;
            g_pyramid(x2, z2);
            g_chunk(x2, y2, z2);
            Ocdelta d1(true), d2(true);
            modify(index(x2, y2, z2), &d1, &d2);
        }
    }
    bmin += ivec3(0, 0, 1);
    bmax += ivec3(0, 0, 1);
}
