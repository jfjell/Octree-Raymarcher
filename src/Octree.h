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

    Octwig() = default;
    explicit Octwig(uint16_t v);
};

static_assert(sizeof(Octwig) == (1 << (TWIG_LEVELS * 3)) * sizeof(uint16_t));

struct Ocdelta
{
    size_t left;
    size_t right;
    bool realloc;

    Ocdelta(bool a = false, size_t l = SIZE_MAX, size_t r = 0): left(l), right(r), realloc(a) {}
};

struct Ocroot
{
    glm::vec3 position;
    float     size;
    uint32_t  depth;
    uint64_t  trees;
    uint64_t  twigs;
    uint64_t  treestoragesize;
    uint64_t  twigstoragesize;
    bool      modified;
    Octree   *tree;
    Octwig   *twig;

    void write(const char *path);
    void read(const char *path);
    void destroy(glm::vec3 cmin, glm::vec3 cmax, Ocdelta *dtree, Ocdelta *dtwig);
    void build(glm::vec3 cmin, glm::vec3 cmax, uint16_t mat, Ocdelta *dtree, Ocdelta *dtwig);
    void replace(glm::vec3 cmin, glm::vec3 cmax, uint16_t mat, Ocdelta *dtree, Ocdelta *dtwig);
    Ocroot defragcopy();
    Ocroot lodmm();
};

struct BoundsPyramid;

// Generate
void grow(Ocroot *root, glm::vec3 position, float size, uint32_t depth, const BoundsPyramid *pyr);

#endif