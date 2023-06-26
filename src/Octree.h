#pragma once

#ifndef OCTREE_H
#define OCTREE_H

#include <glm/vec3.hpp>

enum Octype
{
    EMPTY  = 0,
    LEAF   = 1,
    BRANCH = 2,
    TWIG   = 3,
};

struct Octree
{
    uint32_t type   : 2;
    uint32_t offset : 30;

    static unsigned branch(bool xg, bool yg, bool zg);
    static void cut(unsigned i, bool *xg, bool *yg, bool *zg);
};

static_assert(sizeof(Octree) == sizeof(uint32_t));

#define TWIG_LEVELS 2
#define TWIG_SIZE 4

struct Octwig
{
    uint64_t leafmap[1];

    static unsigned word(int x, int y, int z);
    static unsigned bit(int x, int y, int z);
};

static_assert(sizeof(Octwig) == (1 << (TWIG_LEVELS * 3)) / 8);

struct Ocroot
{
    glm::vec3 position;
    float     size;
    float     density;
    uint32_t  depth;
    uint64_t  trees;
    uint64_t  twigs;
    Octree   *tree;
    Octwig   *twig;
};

struct BoundsPyramid;

void grow(Ocroot *root, glm::vec3 position, float size, uint32_t depth, const BoundsPyramid *pyr);

#endif