#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <queue>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Octree.h"
#include "BoundsPyramid.h"

using glm::vec3;

unsigned Octwig::word(unsigned x, unsigned y, unsigned z)
{
    assert(x < TWIG_SIZE);
    assert(y < TWIG_SIZE);
    assert(z < TWIG_SIZE);
    unsigned i = (z * TWIG_SIZE * TWIG_SIZE) + (y * TWIG_SIZE) + x;
    assert(i < TWIG_WORDS);
    return i;
}

Octree::Octree(uint32_t type, uint32_t offset)
{
    assert(type <= 3);
    assert(offset < (uint32_t)1 << 31);
    value = (type << 30) | (offset & ~((uint32_t)3 << 30));
}

uint64_t Octree::offset()
{
    return value & ~((uint32_t)3 << 30);
}

uint32_t Octree::type()
{
    return value >> 30;
}

unsigned Octree::branch(bool xg, bool yg, bool zg)
{
    return (int)xg + (int)yg * 2 + (int)zg * 4;
}

void Octree::cut(unsigned i, bool *xg, bool *yg, bool *zg)
{
    *xg = i & 1;
    *yg = i & 2;
    *zg = i & 4;
}

#define INVALID_OFFSET (~((uint32_t)3 << 30))

uint16_t heightMaterial(float y)
{
    return (uint16_t)glm::clamp(y / 0.03, 1.0, 4.0);
}

void grow(Ocroot *root, vec3 position, float size, uint32_t depth, const BoundsPyramid *pyr)
{
    using std::queue;

    root->position = position;
    root->density  = 0;
    root->size     = size;
    root->depth    = depth;

    uint64_t allocatedTrees = 16; 
    root->trees = 1; 
    root->tree  = (Octree *)malloc(allocatedTrees * sizeof(Octree));

    uint64_t allocatedTwigs = 16;
    root->twigs = 0;
    root->twig  = (Octwig *)malloc(allocatedTwigs * sizeof(Octwig));

    struct Ocentry
    {
        vec3     pos;
        float    size;
        uint32_t depth;
        uint32_t offset;
    };

    auto q = queue<Ocentry>();
    q.push({ position, size, 0, 0 }); // Push root

    while (!q.empty())
    {
        Ocentry t = q.front();
        q.pop();

        vec3 p = (t.pos - position) / size;
        float low = pyr->min(p.x, p.z, t.depth);
        float high = pyr->max(p.x, p.z, t.depth);

        if(high < t.pos.y)
        {
            // Maximum height in this quadrant is lower => empty
            root->tree[t.offset] = Octree(EMPTY, INVALID_OFFSET);
        }
        else if (low > t.pos.y + t.size)
        {
            // Minimum height in this quadrant is higher => leaf/solid
            root->tree[t.offset] = Octree(LEAF, heightMaterial(p.y));
        }
        else if (t.depth == root->depth - TWIG_LEVELS)
        {
            assert(high >= t.pos.y && low <= t.pos.y + t.size);

            // Reached maximum depth => twig/brick
            float twigLeafSize = t.size / (1 << TWIG_LEVELS);
            Octwig twig;
            for (int y = 0; y < TWIG_SIZE; ++y)
            {
                for (int z = 0; z < TWIG_SIZE; ++z)
                {
                    for (int x = 0; x < TWIG_SIZE; ++x)
                    {
                        float dx = ((float)x * twigLeafSize) / size;
                        float dz = ((float)z * twigLeafSize) / size;
                        float l = pyr->min(p.x + dx, p.z + dz, t.depth + TWIG_LEVELS);
                        float h = pyr->max(p.x + dx, p.z + dz, t.depth + TWIG_LEVELS);

                        (void)l;

                        // Similar logic to above
                        uint32_t w = Octwig::word(x, y, z);
                       if (h >= t.pos.y + y *twigLeafSize) // Leaf
                            twig.leaf[w] = heightMaterial(p.y);
                        else // Empty
                            twig.leaf[w] = 0;
                    }
                }
            }
            if (root->twigs >= allocatedTwigs)
                root->twig = (Octwig *)realloc(root->twig, (allocatedTwigs *= 2) * sizeof(Octwig));
            uint64_t offset = root->twigs++;
            root->tree[t.offset] = Octree(TWIG, (uint32_t)offset);
            root->twig[offset] = twig;
        }
        else
        {
            // Must be a branch => make 8 children and add them to the queue
            assert(high >= t.pos.y && low <= t.pos.y + t.size);

            if (root->trees >= allocatedTrees + 8)
                root->tree = (Octree *)realloc(root->tree, (allocatedTrees *= 2) * sizeof(Octree));
            
            uint64_t offset = root->trees;

            for (int i = 0; i < 8; ++i)
            {
                bool xg, yg, zg;
                Octree::cut(i, &xg, &yg, &zg);
                q.push({ t.pos + vec3(xg, yg, zg) * (t.size / 2), t.size / 2, t.depth + 1, (uint32_t)offset + i });
            }

            root->trees += 8;
            root->tree[t.offset] = Octree(BRANCH, (uint32_t)offset);
        }
    }

    root->tree = (Octree *)realloc(root->tree, root->trees * sizeof(Octree));
    root->twig = (Octwig *)realloc(root->twig, root->twigs * sizeof(Octwig));
}

#define TREE_STRUCT_SIZE (sizeof(Ocroot) - sizeof(void *) * 2)

void pollinate(const Ocroot *root, const char *path)
{
    FILE *fp = fopen(path, "wb");
    fwrite(&root->position, 1, TREE_STRUCT_SIZE, fp);
    fwrite(root->tree, sizeof(Octree), root->trees, fp);
    fwrite(root->twig, sizeof(Octwig), root->twigs, fp);
    fclose(fp);
}

void propagate(Ocroot *root, const char *path)
{
    FILE *fp = fopen(path, "rb");
    fread(&root->position, 1, TREE_STRUCT_SIZE, fp);
    root->tree = (Octree *)malloc(root->trees * sizeof(Octree));
    fread(root->tree, sizeof(Octree), root->trees, fp);
    root->twig = (Octwig *)malloc(root->twigs * sizeof(Octwig));
    fread(root->twig, sizeof(Octwig), root->twigs, fp);
    fclose(fp);
}

/*
void trim(Ocroot *root)
{

}

void fertilize(Ocroot *root, const BoundsPyramid *pyr)
{

}
*/