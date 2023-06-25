#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "Mesh.h"

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
};

static_assert(sizeof(Octree) == sizeof(uint32_t));

#define TWIG_LEVELS 2
#define TWIG_SIZE 4

struct Octwig
{
    uint64_t leafmap[1];
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
