#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Traverse.h"
#include "Octree.h"


#define EPS (1.0f / 8192.0f)

using glm::vec3;
using glm::bvec3;
using glm::ivec3;
using glm::greaterThanEqual;
using glm::all;
using glm::min;
using glm::max;

bool isInsideCube(vec3 p, vec3 cmin, vec3 cmax) 
{
    bvec3 geq = greaterThanEqual(p, cmin);
    bvec3 leq = greaterThanEqual(cmax, p);
    return all(geq) && all(leq);
}

float cubeEscapeDistance(vec3 a, vec3 b, vec3 cmin, vec3 cmax) 
{
    vec3 gamma = vec3(1.0 / b.x, 1.0 / b.y, 1.0 / b.z);
    vec3 tmin = (cmin - a) * gamma;
    vec3 tmax = (cmax - a) * gamma;
    vec3 t = max(tmin, tmax);
    return min(t.x, min(t.y, t.z));
}

Tree traverse(vec3 p, const Ocroot *root)
{
    Tree t = Tree(root->position, root->size, 0);
    for ( ; ; )
    {
        if (root->tree[t.offset].type() != BRANCH) return t;
        float halfsize = t.size * 0.5f;
        vec3 mid = t.bmin + halfsize;
        bvec3 ge = greaterThanEqual(p, mid);
        vec3 bmin = t.bmin + (vec3)ge * halfsize;
        uint64_t i = Octree::branch(ge.x, ge.y, ge.z);
        uint64_t next = root->tree[t.offset].offset() + i;
        t = Tree(bmin, halfsize, next);
    }
}

bool twigmarch(vec3 a, vec3 b, vec3 bmin, float size, float leafsize, const Octwig *twig, float *s)
{
    vec3 bmax = bmin + size;
    float t = 0.0;
    for (int c = 0; c < 1000; ++c)
    {
        vec3 p = a + b * t;
        if (!isInsideCube(p, bmin, bmax)) return false;
        ivec3 off = ivec3((p - bmin) / leafsize);
        if (!isInsideCube(off, vec3(0), vec3(TWIG_SIZE-1))) return false;
        uint32_t word = Octwig::word(off.x, off.y, off.z);
        if (twig->leaf[word] != 0)
        {
            *s = t;
            return true;
        }
        vec3 leafmin = bmin + vec3(off) * leafsize;
        vec3 leafmax = leafmin + leafsize;
        float escape = cubeEscapeDistance(p, b, leafmin, leafmax);
        t += escape + EPS;
    }
    return false;
}

bool treemarch(vec3 a, vec3 b, const Ocroot *root, float *s)
{
    vec3 rmin = root->position;
    vec3 rmax = root->position + root->size;
    float t = 0.0;
    for (int i = 0; i < 1000; ++i)
    {
        vec3 p = a + b * t;
        if (!isInsideCube(p, rmin, rmax)) return false;

        Tree tree = traverse(p, root);
        uint32_t type = root->tree[tree.offset].type();
        if (type == EMPTY)
        {
            float escape = cubeEscapeDistance(p, b, tree.bmin, tree.bmin + tree.size);
            t += escape + EPS;
        }
        else if (type == LEAF)
        {
            *s = t - EPS;
            return true;
        }
        else if (type == TWIG)
        {
            float leafsize = tree.size / (1 << TWIG_LEVELS);
            if (twigmarch(p, b, tree.bmin, tree.size, leafsize, &root->twig[root->tree[tree.offset].offset()], s))
            {
                *s += t;
                return true;
            }
            float escape = cubeEscapeDistance(p, b, tree.bmin, tree.bmin + tree.size);
            t += escape + EPS;
        }
        else
        {
            assert(false);
        }
    }
    return false;
}

float intersectCube(vec3 a, vec3 b, vec3 cmin, vec3 cmax, bool *intersect)
{
    vec3 tmin = (cmin - a) / b;
    vec3 tmax = (cmax - a) / b;
    vec3 t1 = min(tmin, tmax);
    vec3 t2 = max(tmin, tmax);
    float tnear = max(max(t1.x, t1.y), t1.z);
    float tfar = min(min(t2.x, t2.y), t2.z);
    *intersect = tfar > tnear;
    return tnear;
}

bool chunkmarch(vec3 alpha, vec3 beta, const Ocroot *chunk, size_t chunks, vec3 index, vec3 *sigma)
{
    float chsize = chunk[0].size;
    vec3 chmin = chunk[0].position;
    vec3 chmax = chunk[chunks-1].position + chsize;

    float t = 0.0f;
    bool intersect = true;
    if (!isInsideCube(alpha, chmin, chmax)) 
        t = intersectCube(alpha, beta, chmin, chmax, &intersect) + EPS;
    if (!intersect)
        return false;

    for (int c = 0; c < 1000; ++c)
    {
        vec3 p = alpha + beta * t;
        if (!isInsideCube(p, chmin, chmax)) 
            return false;

        ivec3 chi = (p - chmin) / chsize;
        int i = (int)glm::dot(vec3(chi), index);
        if (i < 0 || i >= (int)chunks) 
            return false;
        if (!isInsideCube(p, chunk[i].position, chunk[i].position + chsize))
            return false;

        float s = 0;
        if (treemarch(p, beta, &chunk[i], &s))
        {
            t += s;
            *sigma = alpha + beta * t;
            return true;
        }
        else
        {
            float escape = cubeEscapeDistance(p, beta, chunk[i].position, chunk[i].position + chsize);
            t += escape + EPS;
        }
    }
    return false;
}

bool cubesIntersect(vec3 bmin0, vec3 bmax0, vec3 bmin1, vec3 bmax1)
{
    using glm::all; 
    using glm::greaterThanEqual;
    return all(greaterThanEqual(bmax0, bmin1)) && all(greaterThanEqual(bmax1, bmin0));
}

bool cubeIsInside(glm::vec3 omin, glm::vec3 omax, glm::vec3 imin, glm::vec3 imax)
{
    using glm::all; 
    using glm::greaterThanEqual;
    return all(greaterThanEqual(imin, omin)) && all(greaterThanEqual(omax, imax));
}