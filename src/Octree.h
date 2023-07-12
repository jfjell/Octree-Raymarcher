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
    uint32_t value;

    Octree(uint32_t type, uint32_t offset);
    uint64_t offset();
    uint32_t type();

    static unsigned branch(bool xg, bool yg, bool zg);
    static void cut(unsigned i, bool *xg, bool *yg, bool *zg);
};

static_assert(sizeof(Octree) == sizeof(uint32_t));

#define TWIG_LEVELS 2
#define TWIG_SIZE 4
#define TWIG_WORDS (8*8)

struct Octwig
{
    uint16_t leaf[TWIG_WORDS];

    static unsigned word(unsigned x, unsigned y, unsigned z);
};

static_assert(sizeof(Octwig) == (1 << (TWIG_LEVELS * 3)) * sizeof(uint16_t));

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

// Generate
void grow(Ocroot *root, glm::vec3 position, float size, uint32_t depth, const BoundsPyramid *pyr);
// Write to a file
void pollinate(const Ocroot *root, const char *path); // Write to file
// Read from a file
void propagate(Ocroot *root, const char *path); 
// Remove the lowest level
void trim(Ocroot *root);
// Opposite of trim
void fertilize(Ocroot *root, const BoundsPyramid *pyr);

#endif