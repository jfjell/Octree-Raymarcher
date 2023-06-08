#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "Mesh.h"

struct Octree
{
    using vec3 = glm::vec3;
    vec3    position;
    vec3    size;
    bool    leaf;
    size_t  depth;
    Octree *parent;
    Octree *branches[8];
};

Octree * generateWorldDumb(glm::vec3 pos, size_t depth, float size, float scale, float amp);
Octree * generateWorld(glm::vec3 pos, size_t depth, int size, float amplitude, float phase, float shift);
void plantMesh(const Octree *tree, Mesh& mesh);
void plantMeshDumb(const Octree *tree, Mesh& mesh);
