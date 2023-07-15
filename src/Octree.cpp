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
#include "Traverse.h"

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

Octwig::Octwig(uint16_t v)
{
    for (size_t i = 0; i < TWIG_WORDS; ++i)
        leaf[i] = v;
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
    root->size     = size;
    root->depth    = depth;

    root->treestoragesize = 16; 
    root->trees = 1; 
    root->tree  = (Octree *)malloc(root->treestoragesize * sizeof(Octree));

    root->twigstoragesize = 16;
    root->twigs = 0;
    root->twig  = (Octwig *)malloc(root->twigstoragesize * sizeof(Octwig));

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
            if (root->twigs >= root->twigstoragesize)
                root->twig = (Octwig *)realloc(root->twig, (root->twigstoragesize *= 2) * sizeof(Octwig));
            uint64_t offset = root->twigs++;
            root->tree[t.offset] = Octree(TWIG, (uint32_t)offset);
            root->twig[offset] = twig;
        }
        else
        {
            // Must be a branch => make 8 children and add them to the queue
            assert(high >= t.pos.y && low <= t.pos.y + t.size);

            if (root->trees+8 >= root->treestoragesize)
                root->tree = (Octree *)realloc(root->tree, (root->treestoragesize *= 2) * sizeof(Octree));
            
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
}

#define TREE_STRUCT_SIZE (sizeof(Ocroot) - sizeof(void *) * 2)

void Ocroot::write(const char *path)
{
    FILE *fp = fopen(path, "wb");
    fwrite(&position, 1, TREE_STRUCT_SIZE, fp);
    fwrite(tree, sizeof(Octree), trees, fp);
    fwrite(twig, sizeof(Octwig), twigs, fp);
    fclose(fp);
}

void Ocroot::read(const char *path)
{
    FILE *fp = fopen(path, "rb");
    fread(&position, 1, TREE_STRUCT_SIZE, fp);

    tree = (Octree *)malloc(treestoragesize * sizeof(Octree));
    fread(tree, sizeof(Octree), trees, fp);

    twig = (Octwig *)malloc(twigstoragesize * sizeof(Octwig));
    fread(twig, sizeof(Octwig), twigs, fp);

    fclose(fp);
}


static void destroyCube(Ocroot *root, 
    uint64_t offset, 
    vec3 bmin, 
    float size, 
    size_t depth, 
    vec3 cmin, 
    vec3 cmax,
    Ocdelta *tree,
    Ocdelta *twig)
{
    using glm::min;
    using glm::max;

    vec3 bmax = bmin + size;

    if (!cubesIntersect(bmin, bmax, cmin, cmax))
        return;

    Octree t = root->tree[offset];
    if (t.type() == EMPTY)
    {
        return;
    }
    else if (cubeIsInside(cmin, cmax, bmin, bmax))
    {
        tree->left = min(tree->left, offset);
        tree->right = max(tree->right, offset+1);
        root->tree[offset] = Octree(EMPTY, 0);
    }
    else if (t.type() == LEAF)
    {
        if (depth == root->depth - TWIG_LEVELS)
        {
            // At max depth => make a twig
            if (root->twigs >= root->twigstoragesize)
            {
                root->twig = (Octwig *)realloc(root->twig, (root->twigstoragesize *= 2) * sizeof(Octwig));
                twig->realloc = true;
            }

            size_t pos = root->twigs++; 
            
            twig->left = min(twig->left, pos);
            twig->right = max(twig->right, pos+1);
            root->twig[pos] = Octwig((uint16_t)t.offset());

            tree->left = min(tree->left, offset);
            tree->right = max(tree->right, offset+1);
            root->tree[offset] = Octree(TWIG, pos);

            destroyCube(root, offset, bmin, size, depth, cmin, cmax, tree, twig);
        }
        else
        {
            // Make 8 leaf children and check recursively
            if (root->trees+8 >= root->treestoragesize)
            {
                root->tree = (Octree *)realloc(root->tree, (root->treestoragesize *= 2) * sizeof(Octree));
                tree->realloc = true;
            }

            size_t pos = root->trees; 
            tree->left = min(tree->left, offset);
            root->tree[offset] = Octree(BRANCH, pos);

            tree->right = max(tree->right, pos + 7 + 1);
            for (size_t i = 0; i < 8; ++i)
                root->tree[pos + i] = Octree(LEAF, t.offset());

            root->trees += 8;

            destroyCube(root, offset, bmin, size, depth, cmin, cmax, tree, twig);
        }
    }
    else if (t.type() == TWIG)
    {
        float leafsize = size / (1 << TWIG_LEVELS);
        twig->left = min(twig->left, t.offset());
        twig->right = max(twig->right, t.offset()+1);
        for (size_t z = 0; z < TWIG_SIZE; ++z)
        {
            for (size_t y = 0; y < TWIG_SIZE; ++y)
            {
                for (size_t x = 0; x < TWIG_SIZE; ++x)
                {
                    size_t i = Octwig::word(x, y, z);
                    vec3 leafmin = bmin + vec3(x, y, z) * leafsize;
                    vec3 leafmax = leafmin + leafsize;
                    if (cubesIntersect(leafmin, leafmax, cmin, cmax))
                        root->twig[t.offset()].leaf[i] = 0;
                }
            }
        }
    }
    else if (t.type() == BRANCH)
    {
        float halfsize = size * 0.5;
        for (size_t i = 0; i < 8; ++i)
        {
            bool xg, yg, zg;
            Octree::cut(i, &xg, &yg, &zg);
            vec3 nextmin = bmin + vec3(xg, yg, zg) * halfsize;
            destroyCube(root, t.offset() + i, nextmin, halfsize, depth+1, cmin, cmax, tree, twig);
        }
    }
    else
    {
        assert(false);
    }
}

void Ocroot::destroy(glm::vec3 cmin, glm::vec3 cmax, Ocdelta *dtree, Ocdelta *dtwig)
{
    *dtree = Ocdelta(SIZE_MAX, 0, false);
    *dtwig = Ocdelta(SIZE_MAX, 0, false);
    destroyCube(this, 0, position, size, 0, cmin, cmax, dtree, dtwig);
}

static void buildCube(Ocroot *root, 
    uint64_t offset, 
    vec3 bmin, 
    float size, 
    size_t depth, 
    vec3 cmin, 
    vec3 cmax,
    uint16_t material,
    Ocdelta *tree,
    Ocdelta *twig)
{
    using glm::min;
    using glm::max;

    vec3 bmax = bmin + size;

    if (!cubesIntersect(bmin, bmax, cmin, cmax))
        return;

    Octree t = root->tree[offset];
    if (t.type() == EMPTY)
    {
        if (cubeIsInside(cmin, cmax, bmin, bmax))
        {
            tree->left = min(tree->left, offset);
            tree->right = max(tree->right, offset+1);
            root->tree[offset] = Octree(LEAF, material);
        }
        else if (depth == root->depth - TWIG_LEVELS)
        {
            // At max depth => make a twig
            if (root->twigs >= root->twigstoragesize)
            {
                root->twig = (Octwig *)realloc(root->twig, (root->twigstoragesize *= 2) * sizeof(Octwig));
                twig->realloc = true;
            }

            size_t pos = root->twigs++; 
            
            twig->left = min(twig->left, pos);
            twig->right = max(twig->right, pos+1);
            root->twig[pos] = Octwig(0);

            tree->left = min(tree->left, offset);
            tree->right = max(tree->right, offset+1);
            root->tree[offset] = Octree(TWIG, pos);

            buildCube(root, offset, bmin, size, depth, cmin, cmax, material, tree, twig);
        }
        else
        {
            // Make 8 leaf children and check recursively
            if (root->trees+8 >= root->treestoragesize)
            {
                root->tree = (Octree *)realloc(root->tree, (root->treestoragesize *= 2) * sizeof(Octree));
                tree->realloc = true;
            }

            size_t pos = root->trees; 
            tree->left = min(tree->left, offset);
            root->tree[offset] = Octree(BRANCH, pos);

            tree->right = max(tree->right, pos + 7 + 1);
            for (size_t i = 0; i < 8; ++i)
                root->tree[pos + i] = Octree(EMPTY, 0);

            root->trees += 8;

            buildCube(root, offset, bmin, size, depth, cmin, cmax, material, tree, twig);
        }
    }
    else if (t.type() == LEAF)
    {
        return;
    }
    else if (t.type() == TWIG)
    {
        float leafsize = size / (1 << TWIG_LEVELS);
        twig->left = min(twig->left, t.offset());
        twig->right = max(twig->right, t.offset()+1);
        for (size_t z = 0; z < TWIG_SIZE; ++z)
        {
            for (size_t y = 0; y < TWIG_SIZE; ++y)
            {
                for (size_t x = 0; x < TWIG_SIZE; ++x)
                {
                    size_t i = Octwig::word(x, y, z);
                    vec3 leafmin = bmin + vec3(x, y, z) * leafsize;
                    vec3 leafmax = leafmin + leafsize;
                    if (root->twig[t.offset()].leaf[i] == 0 && cubesIntersect(leafmin, leafmax, cmin, cmax))
                        root->twig[t.offset()].leaf[i] = material;
                }
            }
        }
    }
    else if (t.type() == BRANCH)
    {
        float halfsize = size * 0.5;
        for (size_t i = 0; i < 8; ++i)
        {
            bool xg, yg, zg;
            Octree::cut(i, &xg, &yg, &zg);
            vec3 nextmin = bmin + vec3(xg, yg, zg) * halfsize;
            buildCube(root, t.offset() + i, nextmin, halfsize, depth+1, cmin, cmax, material, tree, twig);
        }
    }
    else
    {
        assert(false);
    }
}

void Ocroot::build(glm::vec3 cmin, glm::vec3 cmax, uint16_t mat, Ocdelta *dtree, Ocdelta *dtwig)
{
    *dtree = Ocdelta(SIZE_MAX, 0, false);
    *dtwig = Ocdelta(SIZE_MAX, 0, false);
    buildCube(this, 0, position, size, 0, cmin, cmax, mat, dtree, dtwig);
}

void Ocroot::replace(glm::vec3 cmin, glm::vec3 cmax, uint16_t mat, Ocdelta *dtree, Ocdelta *dtwig)
{
    *dtree = Ocdelta(SIZE_MAX, 0, false);
    *dtwig = Ocdelta(SIZE_MAX, 0, false);
    destroyCube(this, 0, position, size, 0, cmin, cmax, dtree, dtwig);
    buildCube(this, 0, position, size, 0, cmin, cmax, mat, dtree, dtwig);
}

void Ocroot::incLOD()
{

}

void Ocroot::decLOD()
{

}
