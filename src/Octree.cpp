#include <math.h>
#include <stdlib.h>
#include <queue>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Octree.h"
#include "BoundsPyramid.h"

using glm::vec3;

unsigned Octwig::word(int x, int y, int z)
{
    (void)x;
    (void)y;
    (void)z;
    return 0;
}

unsigned Octwig::bit(int x, int y, int z)
{
    return x * 4 * 4 + y * 4 + z;
}

unsigned Octree::branch(bool xg, bool yg, bool zg)
{
    return (zg << 2) | (yg << 1) | xg;
}

void Octree::cut(unsigned i, bool *xg, bool *yg, bool *zg)
{
    *xg = i & 1;
    *yg = i & 2;
    *zg = i & 4;
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
    size_t allocatedTwigs = 16;
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
        Ocentry e = q.front();
        q.pop();

        vec3 p = (e.pos - position) / size;
        float low = pyr->min(p.x, p.z, e.depth);
        float high = pyr->max(p.x, p.z, e.depth);

        if(high < e.pos.y)
        {
            // Maximum height in this quadrant is lower => empty
            root->tree[e.offset].type   = EMPTY;
            root->tree[e.offset].offset = ~(uint32_t)0;
        }
        else if (low > e.pos.y + e.size)
        {
            // Minimum height in this quadrant is higher => leaf/solid
            root->tree[e.offset].type   = LEAF;
            root->tree[e.offset].offset = ~(uint32_t)0;
        }
        else if (e.depth == root->depth - TWIG_LEVELS)
        {
            assert(high >= e.pos.y && low <= e.pos.y + e.size);

            // printf("\nMaking a twig where depth=%d, low=%f, high=%f, xyz=%f,%f,%f\n", e.depth, low, high, e.pos.x, e.pos.y, e.pos.z);
            // Reached maximum depth => twig/brick
            float twigLeafSize = e.size / (1 << TWIG_LEVELS);
            Octwig twig;
            for (int y = 0; y < TWIG_SIZE; ++y)
            {
                for (int z = 0; z < TWIG_SIZE; ++z)
                {
                    for (int x = 0; x < TWIG_SIZE; ++x)
                    {
                        float dx = ((float)x * twigLeafSize) / size;
                        float dz = ((float)z * twigLeafSize) / size;
                        float l = pyr->min(p.x + dx, p.z + dz, e.depth + TWIG_LEVELS);
                        float h = pyr->max(p.x + dx, p.z + dz, e.depth + TWIG_LEVELS);

                        // printf("\txyz=%f,%f, low=%f, high=%f\n", e.pos.x + (float)x * twigLeafSize, e.pos.z + (float)z * twigLeafSize, l, h);

                        /*
                        // Same logic as above
                        if (h < e.pos.y + y * twigLeafSize) // Empty
                            twig.leafmap[Octwig::word(x, y, z)] &= ~(1 << Octwig::bit(x, y, z));
                        if (l > e.pos.y + y * twigLeafSize) // Leaf
                            twig.leafmap[Octwig::word(x, y, z)] |=  (1 << Octwig::bit(x, y, z));
                        */
                       if (h >= e.pos.y + y *twigLeafSize) // Leaf
                            twig.leafmap[Octwig::word(x, y, z)] |=  (1 << Octwig::bit(x, y, z));
                        else // Empty
                            twig.leafmap[Octwig::word(x, y, z)] &= ~(1 << Octwig::bit(x, y, z));

                        (void)l;
                    }
                }
            }
            if (root->twigs >= allocatedTwigs)
                root->twig = (Octwig *)realloc(root->twig, (allocatedTwigs *= 2) * sizeof(Octwig));

            uint32_t offset = root->twigs++;
            root->tree[e.offset].type   = TWIG;
            root->tree[e.offset].offset = offset;
            root->twig[offset]          = twig;

            // printf("%016llx\n", root->twig[offset].leafmap[0]);
        }
        else
        {
            // Must be a branch => make 8 children and add them to the queue
            assert(high >= e.pos.y && low <= e.pos.y + e.size);

            if (root->trees >= allocatedTrees + 8)
                root->tree = (Octree *)realloc(root->tree, (allocatedTrees *= 2) * sizeof(Octree));
            
            uint32_t offset = root->trees;

            for (int i = 0; i < 8; ++i)
            {
                bool xg, yg, zg;
                Octree::cut(i, &xg, &yg, &zg);
                q.push({ e.pos + vec3(xg, yg, zg) * (e.size / 2), e.size / 2, e.depth + 1, offset + i });
            }

            root->trees += 8;
            root->tree[e.offset].type   = BRANCH;
            root->tree[e.offset].offset = offset;
        }
    }

    root->tree = (Octree *)realloc(root->tree, root->trees * sizeof(Octree));
    root->twig = (Octwig *)realloc(root->twig, root->twigs * sizeof(Octwig));
}
