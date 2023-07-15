#pragma once

#ifndef TRAVERSE_H
#define TRAVERSE_H

#include <glm/vec3.hpp>

struct Ocroot;
struct Octwig;

struct Tree
{
    glm::vec3 bmin;
    float     size;
    uint64_t  offset;

    Tree(glm::vec3 p, float s, uint64_t i): bmin(p), size(s), offset(i) {}
};

bool isInsideCube(glm::vec3 p, glm::vec3 cmin, glm::vec3 cmax);
float cubeEscapeDistance(glm::vec3 a, glm::vec3 b, glm::vec3 cmin, glm::vec3 cmax);
float intersectCube(glm::vec3 a, glm::vec3 b, glm::vec3 cmin, glm::vec3 cmax, bool *intersect);
bool cubesIntersect(glm::vec3 bmin0, glm::vec3 bmax0, glm::vec3 bmin1, glm::vec3 bmax1);
bool cubeIsInside(glm::vec3 omin, glm::vec3 omax, glm::vec3 imin, glm::vec3 imax);

Tree traverse(glm::vec3 p, const Ocroot *root);
bool twigmarch(glm::vec3 a, glm::vec3 b, glm::vec3 bmin, float size, float leafsize, const Octwig *twig, float *s);
bool treemarch(glm::vec3 a, glm::vec3 b, const Ocroot *root, float *s);
bool chunkmarch(glm::vec3 alpha, glm::vec3 beta, const Ocroot *chunk, size_t chunks, glm::vec3 index, glm::vec3 *sigma);

#endif