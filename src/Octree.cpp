#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <queue>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Octree.h"
#include "MisraGries.h"
#include "BoundsPyramid.h"
#include "Traverse.h"

using glm::vec3;
using glm::ivec3;
using glm::bvec3;

using glm::all;
using glm::greaterThanEqual;

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

#define INVALID_OFFSET 0

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
            root->tree[offset] = Octree(TWIG, (uint32_t)pos);

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
            root->tree[offset] = Octree(BRANCH, (uint32_t)pos);

            tree->right = max(tree->right, pos + 7 + 1);
            for (size_t i = 0; i < 8; ++i)
                root->tree[pos + i] = Octree(LEAF, (uint32_t)t.offset());

            root->trees += 8;

            destroyCube(root, offset, bmin, size, depth, cmin, cmax, tree, twig);
        }
    }
    else if (t.type() == TWIG)
    {
        float leafsize = size / (1 << TWIG_LEVELS);
        twig->left = min(twig->left, t.offset());
        twig->right = max(twig->right, t.offset()+1);
        for (unsigned z = 0; z < TWIG_SIZE; ++z)
        {
            for (unsigned y = 0; y < TWIG_SIZE; ++y)
            {
                for (unsigned x = 0; x < TWIG_SIZE; ++x)
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
        float halfsize = size * 0.5f;
        for (unsigned i = 0; i < 8; ++i)
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
    *dtree = *dtwig = Ocdelta();
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
            root->tree[offset] = Octree(TWIG, (uint32_t)pos);

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
            root->tree[offset] = Octree(BRANCH, (uint32_t)pos);

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
        for (unsigned z = 0; z < TWIG_SIZE; ++z)
        {
            for (unsigned y = 0; y < TWIG_SIZE; ++y)
            {
                for (unsigned x = 0; x < TWIG_SIZE; ++x)
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
        float halfsize = size * 0.5f;
        for (unsigned i = 0; i < 8; ++i)
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
    *dtree = *dtwig = Ocdelta();
    buildCube(this, 0, position, size, 0, cmin, cmax, mat, dtree, dtwig);
}

void Ocroot::replace(glm::vec3 cmin, glm::vec3 cmax, uint16_t mat, Ocdelta *dtree, Ocdelta *dtwig)
{
    *dtree = *dtwig = Ocdelta();
    destroyCube(this, 0, position, size, 0, cmin, cmax, dtree, dtwig);
    buildCube(this, 0, position, size, 0, cmin, cmax, mat, dtree, dtwig);
}

// Returns -1 if more than one material is found, otherwise returns the material
int is_monotwig(const Octwig *twig)
{
    uint16_t x = twig->leaf[0];
    for (int i = 1; i < TWIG_WORDS; ++i)
        if (twig->leaf[i] != x)
            return -1;
    return x;
}

// Returns -1 on false, otherwise returns the type
int is_monobranch(const Ocroot *root, uint32_t offset)
{
    Octree t = root->tree[offset];
    assert(t.type() == BRANCH);
    uint32_t x = root->tree[t.offset()].type();
    for (int i = 1; i < 8; ++i)
        if (root->tree[t.offset() + i].type() != x)
            return -1;
    return x;
}

uint16_t descend(const Ocroot *root, uint32_t offset, vec3 cmin, float size, vec3 p)
{
    vec3 cmax = cmin + size;

    assert(isInsideCube(p, cmin, cmax));

    Octree t = root->tree[offset];
    if (t.type() == EMPTY)
        return 0;
    else if (t.type() == LEAF)
        return (uint16_t)t.offset();
    else if (t.type() == TWIG)
    {
        float leafsize = size / (1 << TWIG_DEPTH);
        ivec3 i = ivec3((p - cmin) / leafsize);
        assert(all(greaterThanEqual(i, ivec3(0))) && all(greaterThanEqual(ivec3(TWIG_SIZE - 1), i)));
        unsigned w = Octwig::word(i.x, i.y, i.z);
        return root->twig[t.offset()].leaf[w];
    }
    else
    {
        float halfsize = size * 0.5f;
        vec3 cmid = cmin + halfsize;
        bvec3 geq = greaterThanEqual(p, cmid);
        unsigned i = Octree::branch(geq.x, geq.y, geq.z);
        return descend(root, (uint32_t)t.offset() + i, cmin + vec3(geq) * halfsize, halfsize, p);
    }
}

// Returns the depth
int defragcopy(const Ocroot *from, Ocroot *to, uint32_t f, uint32_t t)
{
    auto twig = Octwig();
    Octree tree = from->tree[f];
    if (tree.type() == EMPTY)
    {
        // Empty nodes and leaves are copied as is
        to->tree[t] = Octree(EMPTY, 0);
        return 1;
    }
    else if (tree.type() == LEAF)
    {
        to->tree[t] = Octree(LEAF, (uint32_t)tree.offset());
        return 1;
    }
    else if (tree.type() == TWIG)
    {
        twig = from->twig[tree.offset()];

        makeTwig: 

        int x = is_monotwig(&twig);
        if (x != -1)
        {
            // Twig is only one material => make it an empty branch or a leaf
            to->tree[t] = Octree(!x ? EMPTY : LEAF, x);

            return 1;
        }
        else
        {
            // Copy the twig as is
            if (to->twigs >= to->twigstoragesize)
                to->twig = (Octwig *)realloc(to->twig, (to->twigstoragesize *= 2) * sizeof(Octwig));

            uint32_t i = (uint32_t)to->twigs++;
            to->tree[t] = Octree(TWIG, i);
            to->twig[i] = twig;

            return TWIG_DEPTH + 1;
        }
    }
    else // if (tree.type() == BRANCH)
    {
        uint64_t trees = to->trees;
        uint64_t twigs = to->twigs;

        if (to->trees + 8 >= to->treestoragesize)
            to->tree = (Octree *)realloc(to->tree, (to->treestoragesize *= 2) * sizeof(Octree));

        uint32_t i = (uint32_t)to->trees;
        to->tree[t] = Octree(BRANCH, i);
        to->trees += 8;

        int maxdepth = 0;
        for (int j = 0; j < 8; ++j)
        {
            int d = defragcopy(from, to, (uint32_t)tree.offset() + j, i + j);
            maxdepth = d > maxdepth ? d : maxdepth;
        }

        if (maxdepth <= TWIG_DEPTH)
        {
            // Made a branch when a twig or leaf/empty would suffice!
            // Reset the tree to its original state and make a twig
            to->trees = trees;
            to->twigs = twigs;

            float leafsize = 1.0 / (1 << TWIG_DEPTH);
            float halfleafsize = leafsize * 0.5f;
            for (unsigned z = 0; z < TWIG_SIZE; ++z)
            {
                for (unsigned y = 0; y < TWIG_SIZE; ++y)
                {
                    for (unsigned x = 0; x < TWIG_SIZE; ++x)
                    {
                        vec3 p = vec3(x, y, z) * leafsize + halfleafsize;
                        unsigned w = Octwig::word(x, y, z);
                        twig.leaf[w] = descend(to, t, vec3(0), 1.0, p);
                    }
                }
            }

            goto makeTwig;
        }
        return maxdepth + 1;
    }
}

void defragcopy(const Ocroot *from, Ocroot *to)
{
    to->position = from->position;
    to->size = from->size;
    to->depth = from->depth;
    to->trees = 1;
    to->twigs = 0;
    to->treestoragesize = 16;
    to->twigstoragesize = 16;
    to->tree = (Octree *)malloc(to->treestoragesize * sizeof(Octree));
    to->twig = (Octwig *)malloc(to->twigstoragesize * sizeof(Octwig));
    
    ::defragcopy(from, to, 0, 0);

    // Did we allocate too much memory?
    while (to->trees > 0 && to->treestoragesize > 16 && to->treestoragesize / to->trees >= 4)
        to->tree = (Octree *)realloc(to->tree, (to->treestoragesize /= 2) * sizeof(Octree));

    while (to->twigs > 0 && to->twigstoragesize > 16 && to->twigstoragesize / to->twigs >= 4)
        to->twig = (Octwig *)realloc(to->twig, (to->twigstoragesize /= 2) * sizeof(Octwig));
}


using glm::bvec3;
using glm::uvec3;

// Returns the depth
template <int K> 
static int density(const Ocroot *root, uint32_t offset, vec3 bmin, float size, vec3 cmin, vec3 cmax, MisraGriesCounter<K> *c, unsigned int n)
{
    Octree t = root->tree[offset];
    if (t.type() == EMPTY)
    {
        c->count(0, n);
        return 1;
    }
    else if (t.type() == LEAF)
    {
        c->count(t.offset(), n);
        return 1;
    }
    else if (t.type() == TWIG)
    {
        float leafsize = size / (1 << TWIG_DEPTH);
        vec3 subtwigmin = glm::clamp((cmin - bmin) / leafsize, 0.0f, (float)TWIG_SIZE);
        vec3 subtwigmax = glm::clamp((cmax - bmin) / leafsize, 0.0f, (float)TWIG_SIZE);

        unsigned int m = n / (1 << (TWIG_DEPTH*3));
        for (unsigned z = (unsigned)subtwigmin.z; (float)z < subtwigmax.z; ++z)
        {
            for (unsigned y = (unsigned)subtwigmin.y; (float)y < subtwigmax.y; ++y)
            {
                for (unsigned x = (unsigned)subtwigmin.x; (float)x < subtwigmax.x; ++x)
                {
                    unsigned w = Octwig::word(x, y, z);
                    c->count(root->twig[t.offset()].leaf[w], m);
                }
            }
        }

        return TWIG_DEPTH;
    }
    else // if (t.type() == BRANCH)
    {
        float halfsize = size * 0.5f;
        int d = 0;
        for (unsigned i = 0; i < 8; ++i)
        {
            bool gx, gy, gz;
            Octree::cut(i, &gx, &gy, &gz);
            vec3 nextmin = bmin + vec3(gx, gy, gz) * halfsize;
            vec3 nextmax = nextmin + halfsize;
            if (cubesIntersect(cmin, cmax, nextmin, nextmax))
                d = glm::max(d, density(root, (uint32_t)t.offset() + i, nextmin, halfsize, cmin, cmax, c, n / 8));
        }
        return d + 1;
    }
}

static void lodmm(const Ocroot *from, Ocroot *to, uint32_t f, uint32_t t, size_t depth)
{
    using glm::uvec3;

    Octree tree = from->tree[f];

    if (tree.type() != BRANCH)
    {
        defragcopy(from, to, f, t);
    }
    else
    {
        assert(tree.type() == BRANCH);

        if (depth == to->depth - TWIG_LEVELS)
        {
            // Make a new twig, sampling the average material of the twigs below
            const float EPS = 1.0 / 256.0;

            if (to->twigs >= to->twigstoragesize)
                to->twig = (Octwig *)realloc(to->twig, (to->twigstoragesize *= 2) * sizeof(Octwig));

            uint32_t i = (uint32_t)to->twigs++;
            to->tree[t] = Octree(TWIG, i);
            to->twig[i] = Octwig();

            float leafsize = 1.0 / (1 << TWIG_LEVELS);
            MisraGriesCounter<8> counter;
            for (unsigned z = 0; z < TWIG_SIZE; ++z)
            {
                for (unsigned y = 0; y < TWIG_SIZE; ++y)
                {
                    for (unsigned x = 0; x < TWIG_SIZE; ++x)
                    {
                        unsigned w = Octwig::word(x, y, z);

                        vec3 leafmin = vec3(x, y, z) * leafsize;
                        vec3 leafmax = leafmin + leafsize;

                        vec3 adjleafmin = leafmin + EPS;
                        vec3 adjleafmax = leafmax - EPS;

                        counter.empty();
                        unsigned int n = (1 << ((TWIG_DEPTH+1)*3)); // Initial multiplier
                        assert(density(from, f, vec3(0), 1.0, adjleafmin, adjleafmax, &counter, n) <= (int)(from->depth - depth));

                        to->twig[i].leaf[w] = (uint16_t)counter.majority();
                    }
                }
            }
        }
        else
        {
            // Same as copy algorithm, only replacing the recursive call
            if (to->trees + 8 >= to->treestoragesize)
                to->tree = (Octree *)realloc(to->tree, (to->treestoragesize *= 2) * sizeof(Octree));

            uint32_t pos = (uint32_t)to->trees;
            to->tree[t] = Octree(BRANCH, pos);
            to->trees += 8;

            for (int i = 0; i < 8; ++i)
                lodmm(from, to, (uint32_t)tree.offset() + i, pos + i, depth + 1);
        }
    }
}

Ocroot Ocroot::lodmm()
{
    assert(depth > 1);

    Ocroot to;
    to.position = position;
    to.size = size;
    to.depth = depth - 1;
    to.trees = 1;
    to.twigs = 0;
    to.treestoragesize = 16;
    to.twigstoragesize = 16;
    to.tree = (Octree *)malloc(to.treestoragesize * sizeof(Octree));
    to.twig = (Octwig *)malloc(to.twigstoragesize * sizeof(Octwig));

    ::lodmm(this, &to, 0, 0, 0);

    return to;
}
