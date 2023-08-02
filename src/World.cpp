#include <algorithm>
#include <GL/glew.h>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "World.h"
#include "Octree.h"
#include "BoundsPyramid.h"
#include "Shader.h"
#include "Traverse.h"

#define TREE_MAX_DEPTH 8
#define PYRAMID_RESOLUTION 256

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
    
    ivec3 bound = ivec3(width, height, depth);
    chunkcoordmin = vec3(0);

    heightmap = new BoundsPyramid[plane];
    for (int z = 0; z < depth; ++z)
        for (int x = 0; x < width; ++x)
            g_pyramid(chunkcoordmin.x+x, chunkcoordmin.z+z);

    chunk = new Ocroot[volume];
    for (int z = 0; z < depth; ++z)
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                g_chunk(chunkcoordmin.x+x, chunkcoordmin.y+y, chunkcoordmin.z+z);

    gcd = new GPUChunk[volume];
}

GPUChunk::GPUChunk(const Ocroot *r, RootAllocation a)
{
    bmin = r->position;
    tr_reg = a.tree.region;
    tr_off = a.tree.offset / sizeof(uint32_t);
    tw_reg = a.twig.region;
    tw_off = a.twig.offset / sizeof(uint32_t);
}

extern const float CUBE_VERTICES[8*3];
extern const unsigned short CUBE_INDICES[6*6];

void World::load_gpu()
{
    atlas.init();

    allocator.init(volume);
    for (int i = 0; i < volume; ++i)
        gcd[i] = GPUChunk(&chunk[i], allocator.alloc(i, &chunk[i]));

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE_VERTICES), CUBE_VERTICES, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(CUBE_INDICES), CUBE_INDICES, GL_STATIC_DRAW);

    glGenBuffers(1, &chunk_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, volume * sizeof(GPUChunk), gcd, GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    shader = Shader(glCreateProgram())
        .vertex("shaders/Parallax.Vertex.glsl")
        .fragment("shaders/ParallaxChunk.Frag.glsl")
        .link();

    chunkmin_ul = glGetUniformLocation(shader, "chunkmin");
    chunkmax_ul = glGetUniformLocation(shader, "chunkmax");
    chunksize_ul = glGetUniformLocation(shader, "chunksize");
    w_ul = glGetUniformLocation(shader, "csw");
    h_ul = glGetUniformLocation(shader, "csh");
    d_ul = glGetUniformLocation(shader, "csd");
    eye_ul = glGetUniformLocation(shader, "eye");
    model_ul = glGetUniformLocation(shader, "model");
    mvp_ul = glGetUniformLocation(shader, "mvp");
    sampler_ul = glGetUniformLocation(shader, "atlas");

    assert(chunkmin_ul != -1);
    assert(chunkmax_ul != -1);
    assert(chunksize_ul != -1);
    assert(w_ul != -1);
    assert(h_ul != -1);
    assert(d_ul != -1);
    assert(eye_ul != -1);
    assert(model_ul != -1);
    assert(mvp_ul != -1);
}

void World::deinit()
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &chunk_ssbo);
    glDeleteProgram(shader);

    for (int i = 0; i < volume; ++i)
    {
        free(chunk[i].tree);
        free(chunk[i].twig);
    }
    delete[] chunk;

    for (int i = 0; i < plane; ++i)
        heightmap[i].deinit();
    delete[] heightmap;

    delete[] gcd;

    allocator.release();
}

static mat4 srt(vec3 chunkmin, vec3 bounds)
{
    mat4 t = glm::translate(mat4(1.0), chunkmin);
    mat4 s = glm::scale(mat4(1.0), bounds);
    mat4 r = mat4(1.0);
    mat4 srt = t * r * s;
    return srt;
}

void World::draw(mat4 mvp, vec3 eye)
{
    vec3 bounds = vec3(width, height, depth);
    vec3 chunkmin = chunkcoordmin * chunksize;
    vec3 chunkmax = chunkmin + bounds * (float)chunksize;
    mat4 model = srt(chunkmin, bounds * (float)chunksize);

    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glBindVertexArray(vao);
    glUseProgram(shader);

    glUniform3fv(chunkmin_ul, 1, glm::value_ptr(chunkmin));
    glUniform3fv(chunkmax_ul, 1, glm::value_ptr(chunkmax));
    glUniform1f(chunksize_ul, (float)chunksize);
    glUniform1i(w_ul, width);
    glUniform1i(h_ul, height);
    glUniform1i(d_ul, depth);
    glUniform3fv(eye_ul, 1, glm::value_ptr(eye));
    glUniformMatrix4fv(model_ul, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(mvp_ul, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform1i(sampler_ul, 0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, chunk_ssbo);
    allocator.bind();

    glBindTexture(GL_TEXTURE_2D, atlas.tex);
    glActiveTexture(GL_TEXTURE0);

    glDrawElements(GL_TRIANGLES, sizeof(CUBE_INDICES) / sizeof(unsigned short), GL_UNSIGNED_SHORT, (void *)0);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

void World::modify(int i, const Ocdelta *tree, const Ocdelta *twig)
{
    gcd[i] = GPUChunk(&chunk[i], allocator.subst(i, &chunk[i], tree, twig));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, i * sizeof(GPUChunk), sizeof(GPUChunk), &gcd[i]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

static int modulo(int n, int m)
{
    return (m + (n % m)) % m;
}

int World::index(int x, int z) const
{
    int i = (modulo(z, depth) * width) + modulo(x, width);
    assert(i >= 0 && i < plane);
    return i;
}

int World::index(int x, int y, int z) const
{
    int i = (modulo(y, height) * width * depth) + (modulo(z, depth) * width) + modulo(x, width);
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
    vec3 p = vec3(x, y, z) * (float)chunksize;
    chunk[i] = Ocroot();
    grow(&chunk[i], p, (float)chunksize, TREE_MAX_DEPTH, &heightmap[j]);

    // Add water at y=6
    vec3 watermin = chunk[i].position;
    vec3 watermax = vec3(chunk[i].position.x + chunk[i].size, 6, chunk[i].position.z + chunk[i].size);
    Ocdelta d;
    chunk[i].build(watermin, watermax, 6, &d, &d);
}

glm::ivec3 World::index_float(glm::vec3 p) const
{
    using glm::vec3;
    using glm::ivec3;

    vec3 q = p / (float)chunksize;
    for (int i = 0; i < 3; ++i) 
        if (q[i] < 0.0) q[i] -= 1.0;
    return ivec3(q); 
}

void World::shift(glm::ivec3 offset)
{
    using glm::vec3;
    using glm::ivec3;
    using glm::ivec2;
    using glm::length;
    using glm::abs;

    assert(glm::length(vec3(offset)) == 1.0);

    static const ivec3 axis[] = { ivec3(1, 0, 0), ivec3(0, 1, 0), ivec3(0, 0, 1) };

    ivec3 bounds(width, height, depth);
    vec3 chunkcoordmax = chunkcoordmin + bounds;
    int index = offset.x ? 0 : offset.y ? 1 : /* offset.z ? */ 2;
    ivec2 inv_index = offset.x ? ivec2(2, 1) : offset.y ? ivec2(2, 0) : /* offset.z ? */ ivec2(1, 0);

    assert(inv_index[0] != index && inv_index[1] != index);

    int sign = offset.x + offset.y + offset.z;
    ivec3 u = axis[index] * ivec3(sign < 0 ? chunkcoordmin[index] - 1 : chunkcoordmax[index]);
    ivec3 prev = ivec3(0); // Previous pyramid point
    for (int i = 0; i < bounds[inv_index[0]]; ++i)
    {
        ivec3 s = axis[inv_index[0]] * (chunkcoordmin[inv_index[0]] + i);
        for (int j = 0; j < bounds[inv_index[1]]; ++j)
        {
            ivec3 t = axis[inv_index[1]] * (chunkcoordmin[inv_index[1]] + j);
            ivec3 p = s + t + u;

            if (!offset.y && (j == 0 || p.x != prev.x || p.z != prev.z)) // First iteration or a new point?
            {
                g_pyramid(p.x, p.z);
                prev = p;
            }

            g_chunk(p.x, p.y, p.z);

            Ocdelta d(true);
            modify(this->index(p.x, p.y, p.z), &d, &d);
        }
    }

    chunkcoordmin += offset;
}
