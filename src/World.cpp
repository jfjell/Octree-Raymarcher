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
#define TREE_SIZE 128
#define PYRAMID_RESOLUTION 64

using glm::vec3;
using glm::mat4;

void World::init(int w, int h, int d)
{
    width  = w;
    height = h;
    depth  = d;
    plane  = width * depth;
    volume = plane * height;

    heightmap = new BoundsPyramid[plane];
    for (int z = 0; z < depth; ++z)
    {
        for (int x = 0; x < width; ++x)
        {
            int i = z * width + x;
            float amplitude = 16;
            float period = 1.0f / PYRAMID_RESOLUTION;
            float xshift = (float)x * PYRAMID_RESOLUTION;
            float yshift = 16.0f;
            float zshift = (float)z * PYRAMID_RESOLUTION;
            heightmap[i].init(PYRAMID_RESOLUTION, amplitude, period, xshift, yshift, zshift);
        }
    }

    chunk = new Ocroot[volume];
    for (int z = 0; z < depth; ++z)
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int i = z * width * height + y * width + x;
                int j = z * width + x;
                vec3 p = vec3(x * TREE_SIZE, (y - (height / 2)) * TREE_SIZE, z * TREE_SIZE);
                grow(&chunk[i], p, TREE_SIZE, TREE_MAX_DEPTH, &heightmap[j]);
            }
        }
    }

    order = new int[volume];
    for (int i = 0; i < volume; ++i)
        order[i] = i;
}

extern const float CUBE_VERTICES[8*3];
extern const unsigned short CUBE_INDICES[6*6];

void World::gpu()
{
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

    ssbotree = new unsigned int[volume];
    glGenBuffers(volume, ssbotree);
    for (int i = 0; i < volume; ++i)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbotree[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, chunk[i].trees * sizeof(Octree), chunk[i].tree, GL_STATIC_DRAW); 
        // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbotree[i]);
    }

    ssbotwig = new unsigned int[volume];
    glGenBuffers(volume, ssbotwig);
    for (int i = 0; i < volume; ++i)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbotwig[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, chunk[i].twigs * sizeof(Octwig), chunk[i].twig, GL_STATIC_DRAW); 
        // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbotwig[i]);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    shaderN = Shader(glCreateProgram())
        .vertex("shaders/Parallax.Vertex.glsl")
        .include("shaders/Raymarch.glsl")
        .fragment("shaders/ParallaxN.Fragment.glsl")
        .link();
    
    pmodel[0] = glGetUniformLocation(shaderN, "model");
    pmvp[0]   = glGetUniformLocation(shaderN, "mvp");
    peye[0]   = glGetUniformLocation(shaderN, "eye");
    pbmin[0]  = glGetUniformLocation(shaderN, "root.pos");
    psize[0]  = glGetUniformLocation(shaderN, "root.size");

    shaderF = Shader(glCreateProgram())
        .vertex("shaders/Parallax.Vertex.glsl")
        .include("shaders/Raymarch.glsl")
        .fragment("shaders/ParallaxF.Fragment.glsl")
        .link();

    pmodel[1] = glGetUniformLocation(shaderF, "model");
    pmvp[1]   = glGetUniformLocation(shaderF, "mvp");
    peye[1]   = glGetUniformLocation(shaderF, "eye");
    pbmin[1]  = glGetUniformLocation(shaderF, "root.pos");
    psize[1]  = glGetUniformLocation(shaderF, "root.size");
}

void World::deinit()
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(volume, ssbotree);
    glDeleteBuffers(volume, ssbotwig);
    glDeleteProgram(shaderN);
    glDeleteProgram(shaderF);

    for (int i = 0; i < plane; ++i)
        heightmap[i].deinit();

    delete[] ssbotree;
    delete[] ssbotwig;
    delete[] chunk;
    delete[] heightmap;
    delete[] order;
}

float cubeDF(vec3 p, vec3 bmin, vec3 bmax)
{
    vec3 bmid = (bmax + bmin) * 0.5f;
    return glm::distance(p, bmid);
}

float chunkDF(vec3 p, const Ocroot *chunk)
{
    return cubeDF(p, chunk->position, chunk->position + chunk->size);
}

glm::mat4 SRT(const Ocroot *chunk)
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

    glBindVertexArray(vao);

    // Near
    glUseProgram(shaderN);

    glUniformMatrix4fv(pmvp[0], 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3fv(peye[0], 1, glm::value_ptr(eye));

    for (int i = 0; i < midpoint; ++i)
    {
        int j = order[i];

        mat4 srt = SRT(&chunk[j]);
        glUniformMatrix4fv(pmodel[0], 1, GL_FALSE, glm::value_ptr(srt));
        glUniform3fv(pbmin[0], 1, glm::value_ptr(chunk[j].position));
        glUniform1f(psize[0], chunk[j].size);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbotree[j]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbotwig[j]);

        glDrawElements(GL_TRIANGLES, sizeof(CUBE_INDICES) / sizeof(unsigned short), GL_UNSIGNED_SHORT, (void *)0);
    }

    // Far
    glUseProgram(shaderF);

    glUniformMatrix4fv(pmvp[1], 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3fv(peye[1], 1, glm::value_ptr(eye));

    for (int i = midpoint; i < volume; ++i)
    {
        int j = order[i];

        mat4 srt = SRT(&chunk[j]);
        glUniformMatrix4fv(pmodel[1], 1, GL_FALSE, glm::value_ptr(srt));
        glUniform3fv(pbmin[1], 1, glm::value_ptr(chunk[j].position));
        glUniform1f(psize[1], chunk[j].size);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbotree[j]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbotwig[j]);

        glDrawElements(GL_TRIANGLES, sizeof(CUBE_INDICES) / sizeof(unsigned short), GL_UNSIGNED_SHORT, (void *)0);
    }

    glBindVertexArray(0);
    glUseProgram(0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void World::mod(int i)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbotree[i]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, chunk[i].trees * sizeof(Octree), chunk[i].tree, GL_STATIC_DRAW); 

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbotwig[i]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, chunk[i].twigs * sizeof(Octwig), chunk[i].twig, GL_STATIC_DRAW); 

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}