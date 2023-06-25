#include <math.h>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Octree.h"
#include "Mesh.h"

using glm::vec3;
using glm::vec2;

inline int branch(bool x, bool y, bool z)
{
    assert(x == 1 || x == 0);
    assert(y == 1 || y == 0);
    assert(z == 1 || z == 0);
    int i = x + y * 2 + z * 4;
    assert(i >= 0 && i < 8);
    return i;
}

inline void cut(int branch, float &x, float &y, float &z)
{
    x = (float)!!(branch & 1);
    y = (float)!!(branch & 2);
    z = (float)!!(branch & 4);
}

Octree * growTreeRec(vec3 pos, size_t depth, float size, float scale, float amp)
{

    float half = size / 2;
    float n = glm::perlin(vec2(pos.x, pos.z) * scale) * amp;
    float left = glm::perlin(vec2(pos.x - size, pos.z) * scale) * amp;
    float right = glm::perlin(vec2(pos.x + size, pos.z) * scale) * amp;
    float front = glm::perlin(vec2(pos.x, pos.z - size) * scale) * amp;
    float back = glm::perlin(vec2(pos.x, pos.z + size) * scale) * amp;
    float neighbour[4] = { left, right, front, back };

    if (depth == 0)
    {
        float delta = 0;
        for (int i = 0; i < 4; ++i)
            delta = glm::max(delta, n - neighbour[i]);

        if ((n >= pos.y && n <= pos.y + size) || (pos.y <= n && delta >= size && pos.y >= n - delta))
        {
            Octree *t = new Octree();
            t->position = pos;
            t->depth = depth;
            t->size = vec3(size, size, size);
            t->leaf = true;

            for (int i = 0; i < 8; ++i)
                t->branches[i] = nullptr;

            return t;
        }
        else
        {
            return nullptr;
        }
    }
    else
    {
        Octree *t = nullptr;
        for (int i = 0; i < 8; ++i)
        {
            float x, y, z;
            cut(i, x, y, z);
            Octree *b = growTreeRec(pos + vec3(x, y, z) * half, depth - 1, half, scale, amp);
            if (b)
            {
                if (!t)
                {
                    t = new Octree();
                    memset(t->branches, 0, sizeof(t->branches));
                    t->branches[i] = b;
                    t->position = pos;
                    t->depth = depth;
                    t->size = vec3(size, size, size);
                    t->leaf = false;
                }
                t->branches[i] = b;
            }
        }
        return t;
    }
}

void addLeafAt(Octree *t, vec3 pos)
{
    assert(pos.x >= t->position.x && pos.x <= t->position.x + t->size.x);
    assert(pos.y >= t->position.y && pos.y <= t->position.y + t->size.y);
    assert(pos.z >= t->position.z && pos.z <= t->position.z + t->size.z);
    assert(t != nullptr);

    if (t->leaf)
    {
        assert(t->depth == 0);
    }
    else
    {
        float half = t->size.x / 2;
        bool xg = pos.x >= t->position.x + half;
        bool yg = pos.y >= t->position.y + half;
        bool zg = pos.z >= t->position.z + half;
        int i = branch(xg, yg, zg);
        if (!t->branches[i])
        {
            Octree *b = new Octree();
            memset(b->branches, 0, sizeof(b->branches));
            b->parent = t;
            b->position = t->position + vec3(xg, yg, zg) * half;
            b->depth = t->depth - 1;
            b->size = vec3(half, half, half);
            b->leaf = b->depth == 0;
            t->branches[i] = b;
        }
        addLeafAt(t->branches[i], pos);
    }
}

/* O(2^2d)*/
Octree * growTree(vec3 pos, size_t depth, int size, float amplitude, float phase, float shift)
{
    using namespace glm;

    assert(depth > 0);

    assert((int)pos.x == pos.x);
    assert((int)pos.y == pos.y);
    assert((int)pos.z == pos.z);

    Octree *root = new Octree();
    memset(root->branches, 0, sizeof(root->branches));
    root->parent   = nullptr;
    root->position = pos;
    root->depth    = depth;
    root->size     = vec3(size, size, size);
    root->leaf     = false;

    size_t leaves    = 1 << depth;
    float  smallsize = (float)size / leaves;

    for (size_t i = 0; i < leaves; ++i)
    {
        for (size_t k = 0; k < leaves; ++k)
        {
            vec2 mid = vec2(pos.x + smallsize * ((float)i + 0.5), pos.z + smallsize * ((float)k + 0.5));

            float noise = perlin(mid * phase) * amplitude + shift;
            float smallhalf = smallsize / 2;
            noise = noise - fmod(noise, smallsize) + smallhalf;

            float x = mid.x;
            float y = noise;
            float z = mid.y;
            if (x < pos.x || x > pos.x + size) continue;
            if (z < pos.z || z > pos.z + size) continue;

            if (y >= pos.y && y <= pos.y + size)
                addLeafAt(root, vec3(x, y, z));

            // Check neighbours height -- may need to add more leaves in order to prevent holes.
            vec2 left  = vec2(pos.x + smallsize * ((float)i - 0.5), pos.z + smallsize * ((float)k + 0.5));
            vec2 right = vec2(pos.x + smallsize * ((float)i + 1.5), pos.z + smallsize * ((float)k + 0.5));
            vec2 back  = vec2(pos.x + smallsize * ((float)i + 0.5), pos.z + smallsize * ((float)k + 1.5));
            vec2 front = vec2(pos.x + smallsize * ((float)i + 0.5), pos.z + smallsize * ((float)k - 0.5));
            float lr  = min(perlin(left * phase), perlin(right * phase));
            float bf  = min(perlin(back * phase), perlin(front * phase));
            float low = min(lr, bf) * amplitude + shift;
            low = low - fmod(low, smallsize);

            if (low > y) continue;

            for (size_t j = 0; j < (y - low - smallsize) / smallsize; ++j)
            {
                float w = low + smallsize * (j + 1);
                if (w >= pos.y && w <= pos.y + size)
                    addLeafAt(root, vec3(x, w, z));
            }
        }
    }

    return root;
}

void plantCubes(const Octree *tree, Mesh& mesh)
{
    if (tree == nullptr)
        return;
    else if (tree->leaf)
        addCube(mesh, tree->position, tree->size);
    else
        for (int i = 0; i < 8; ++i)
            plantCubes(tree->branches[i], mesh);
}

int pbr(const Octree *t, vec3 p)
{
    bool xg = p.x > t->position.x + t->size.x / 2;
    bool yg = p.y > t->position.y + t->size.y / 2;
    bool zg = p.z > t->position.z + t->size.z / 2;
    return branch(xg, yg, zg);
}

bool within(const Octree *t, vec3 p)
{
    bool xw = p.x >= t->position.x && p.x <= t->position.x + t->size.x;
    bool yw = p.y >= t->position.y && p.y <= t->position.y + t->size.y;
    bool zw = p.z >= t->position.z && p.z <= t->position.z + t->size.z;
    return xw && yw && zw;
}

bool isLeafAt(const Octree *t, vec3 p)
{
    if (!t) return false;
    if (!within(t, p)) return false;
    if (t->leaf) return true;
    return isLeafAt(t->branches[pbr(t, p)], p);
}

void plantLeaf(const Octree *root, const Octree *tree, Mesh& mesh)
{
    using glm::value_ptr;

    if (!tree)
        return;

    if (!tree->leaf)
    {
        for (int i = 0; i < 8; ++i)
            plantLeaf(root, tree->branches[i], mesh);
        return;
    } 

    float size = tree->size.x;
    float eps = size / 2;
    vec3 left  = tree->position + vec3(-eps, 0, 0);
    vec3 right = tree->position + vec3(size + eps, 0, 0);
    vec3 back  = tree->position + vec3(0, 0, size + eps);
    vec3 front = tree->position + vec3(0, 0, -eps);
    vec3 above = tree->position + vec3(0, size + eps, 0);
    vec3 below = tree->position + vec3(0, -eps, 0);

    vec3 rs = tree->position + vec3(tree->size.x, 0., 0.);
    vec3 as = tree->position + vec3(0., tree->size.y, 0.);
    vec3 bs = tree->position + vec3(0., 0., tree->size.z);

    if (!isLeafAt(root, left))
        mesh.add(zQuadVertices, 4, value_ptr(tree->position), value_ptr(tree->size), quadUV, quadIndices, 6);
    if (!isLeafAt(root, right))
        mesh.add(zQuadVertices, 4, value_ptr(rs), value_ptr(tree->size), quadUV, quadIndices, 6);

    if (!isLeafAt(root, front))
        mesh.add(xQuadVertices, 4, value_ptr(tree->position), value_ptr(tree->size), quadUV, quadIndices, 6);
    if (!isLeafAt(root, back))
        mesh.add(xQuadVertices, 4, value_ptr(bs), value_ptr(tree->size), quadUV, quadIndices, 6);

    if (!isLeafAt(root, below))
        mesh.add(yQuadVertices, 4, value_ptr(tree->position), value_ptr(tree->size), quadUV, quadIndices, 6);
    if (!isLeafAt(root, above))
        mesh.add(yQuadVertices, 4, value_ptr(as), value_ptr(tree->size), quadUV, quadIndices, 6);
}

void plantFaces(const Octree *tree, Mesh& mesh)
{
    plantLeaf(tree, tree, mesh);
}
