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
    unsigned i = z / 2;
    assert(i < TWIG_WORDS);
    (void)x;
    (void)y;
    return i;
}

unsigned Octwig::bit(unsigned x, unsigned y, unsigned z)
{
    assert(x < TWIG_SIZE);
    assert(y < TWIG_SIZE);
    assert(z < TWIG_SIZE);
    unsigned i = (z / 2) * 16 + y * 4 + x;
    assert(i < TWIG_BITS);
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

#define MATERIALS 8

uint16_t heightMaterial(float y)
{
    return (int)glm::clamp(y / 0.05, 0.0, 3.0);
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

    uint64_t allocatedLeftBarks = 16; 
    size_t leftBarkIndex = 0;
    Ocbark *leftBark = (Ocbark *)malloc(allocatedLeftBarks * sizeof(Ocbark));
    
    size_t allocatedRightBarks = 16;
    size_t rightBarkIndex = 0;
    Ocbark *rightBark = (Ocbark *)malloc(allocatedRightBarks * sizeof(Ocbark));

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
            uint32_t offset = rightBarkIndex++;
            root->tree[t.offset] = Octree(LEAF, offset);

            if (rightBarkIndex >= allocatedRightBarks)
                rightBark = (Ocbark *)realloc(rightBark, (allocatedRightBarks *= 2) * sizeof(Ocbark));
            rightBark[offset].material = heightMaterial(p.y);
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
                       if (h >= t.pos.y + y *twigLeafSize) // Leaf
                            twig.leafmap[Octwig::word(x, y, z)] |=  (1 << Octwig::bit(x, y, z));
                        else // Empty
                            twig.leafmap[Octwig::word(x, y, z)] &= ~(1 << Octwig::bit(x, y, z));

                    }
                }
            }
            if (root->twigs >= allocatedTwigs)
                root->twig = (Octwig *)realloc(root->twig, (allocatedTwigs *= 2) * sizeof(Octwig));
            uint32_t offset = root->twigs++;
            root->tree[t.offset] = Octree(TWIG, offset);
            root->twig[offset] = twig;

            if (leftBarkIndex >= allocatedLeftBarks)
                leftBark = (Ocbark *)realloc(leftBark, (allocatedLeftBarks *= 2) * sizeof(Ocbark));
            leftBark[leftBarkIndex++].material = heightMaterial(p.y);
        }
        else
        {
            // Must be a branch => make 8 children and add them to the queue
            assert(high >= t.pos.y && low <= t.pos.y + t.size);

            if (root->trees >= allocatedTrees + 8)
                root->tree = (Octree *)realloc(root->tree, (allocatedTrees *= 2) * sizeof(Octree));
            
            uint32_t offset = root->trees;

            for (int i = 0; i < 8; ++i)
            {
                bool xg, yg, zg;
                Octree::cut(i, &xg, &yg, &zg);
                q.push({ t.pos + vec3(xg, yg, zg) * (t.size / 2), t.size / 2, t.depth + 1, offset + i });
            }

            root->trees += 8;
            root->tree[t.offset] = Octree(BRANCH, offset);
        }
    }

    root->tree = (Octree *)realloc(root->tree, root->trees * sizeof(Octree));
    root->twig = (Octwig *)realloc(root->twig, root->twigs * sizeof(Octwig));

    assert(leftBarkIndex == root->twigs);
    root->barks = leftBarkIndex + rightBarkIndex;
    root->bark = (Ocbark *)malloc(root->barks * sizeof(Ocbark));
    for (size_t i = 0; i < leftBarkIndex; ++i)
        root->bark[i] = leftBark[i];
    for (size_t i = 0; i < rightBarkIndex; ++i)
        root->bark[root->barks - 1 - i] = rightBark[i];

    free(leftBark);
    free(rightBark);
}

#define TREE_STRUCT_SIZE (12 + 4 + 4 + 4 + 8 + 8 + 8)

void pollinate(const Ocroot *root, const char *path)
{
    FILE *fp = fopen(path, "wb");
    fwrite(&root->position, 1, TREE_STRUCT_SIZE, fp);
    fwrite(root->tree, sizeof(Octree), root->trees, fp);
    fwrite(root->twig, sizeof(Octwig), root->twigs, fp);
    fwrite(root->bark, sizeof(Ocbark), root->barks, fp);
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
    root->bark = (Ocbark *)malloc(root->barks * sizeof(Ocbark));
    fread(root->bark, sizeof(Ocbark), root->barks, fp);
    fclose(fp);
}
