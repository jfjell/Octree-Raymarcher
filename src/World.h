#pragma once

#ifndef WORLD_H
#define WORLD_H

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

struct Ocroot;
struct Ocaxe;
struct BoundsPyramid;

struct World
{
    Ocroot *chunk;
    BoundsPyramid *heightmap;
    int width, height, depth, plane, volume;
    unsigned int vao, vbo, ebo;
    unsigned int shaderN, shaderF;
    unsigned int *ssbotree, *ssbotwig;
    int *order;
    int pmodel[2], pmvp[2], pbmin[2], psize[2], peye[2];

    void init(int w, int h, int d);
    void deinit();
    void gpu();
    void mod(int i, const Ocaxe *tree, const Ocaxe *twig);
    void draw(glm::mat4 mvp, glm::vec3 eye);
};

#endif