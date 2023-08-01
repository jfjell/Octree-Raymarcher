
#define MAX_ROOT_STEPS 256
#define MAX_TREE_STEPS 512
#define MAX_TWIG_STEPS 64
#define MAX_DEPTH 32

#define EMPTY  0
#define LEAF   1
#define BRANCH 2
#define TWIG   3

#define TWIG_SIZE 4
#define TWIG_DEPTH 2
#define TWIG_WORDS 64
#define TWIG_DWORDS 32

#define EPS (1.0 / 4096.0)
#define BIGEPS (1.0 / 16.0)

#define FAR 8192
#define NEAR 0.125

struct Root
{
    vec3 bmin;
    uint tr_region;
    uint tr_offset;
    uint tw_region;
    uint tw_offset;
    uint _align;
};

struct Leaf
{
    vec3 bmin;
    float size;
    uint offset;
};

uniform vec3 chunkmin, chunkmax;
uniform float chunksize;
uniform int csw, csh, csd;
uniform vec3 eye;

layout(std430, binding = 2) restrict readonly buffer CHUNK_SSBO
{
    Root Chunk[];
};

layout(std430, binding = 3) restrict readonly buffer BARK_SSBO
{
    vec4 Bark[];
};

layout(std430, binding = 4) restrict readonly buffer TREE_SSBO_ALPHA 
{
    uint Tree_Alpha[];
};

layout(std430, binding = 5) restrict readonly buffer TWIG_SSBO_ALPHA 
{
    uint Twig_Alpha[];
};

uint Tree(uint r, uint i)
{
    return Tree_Alpha[i];
}

uint Twig(uint r, uint i)
{
    return Twig_Alpha[i];
}

uint Tree_type(uint t)
{
    return t >> 30;
}

uint Twig_shift(uint t)
{
    return t % 2 ? 16 : 0;
}

uint Tree_offset(uint t)
{
    return t & ~(3 << 30);
}

uint Tree_branch(bool xg, bool yg, bool zg)
{
    return int(xg) + int(yg) * 2 + int(zg) * 4;
}

uint Twig_offset(uint t)
{
    return Tree_offset(t) * TWIG_DWORDS;
}

uint Twig_dword(uint x, uint y, uint z)
{
    return (z * 16 + y * 4 + x) / 2;
}

bool isInsideCube(vec3 p, vec3 cmin, vec3 cmax)
{
    bvec3 geq = greaterThanEqual(p, cmin);
    bvec3 leq = greaterThanEqual(cmax, p);
    return all(geq) && all(leq);
}

float cubeEscapeDistance(vec3 a, vec3 g, vec3 cmin, vec3 cmax)
{
    vec3 tmin = (cmin - a) * g;
    vec3 tmax = (cmax - a) * g;
    vec3 t = max(tmin, tmax);
    float d = min(t.x, min(t.y, t.z));
    return d < EPS ? BIGEPS : d;
}

float cubeEnterDistance(vec3 a, vec3 g, vec3 cmin, vec3 cmax, out bool intersect)
{
    vec3 tmin = (cmin - a) * g;
    vec3 tmax = (cmax - a) * g;
    vec3 t1 = min(tmin, tmax);
    vec3 t2 = max(tmin, tmax);
    float tnear = max(max(t1.x, t1.y), t1.z);
    float tfar = min(min(t2.x, t2.y), t2.z);
    intersect = tfar > tnear && tnear > 0;
    return tnear;
}

vec3 cubeNormal(vec3 p, vec3 cmin, vec3 cmax)
{
    if (abs(p.x - cmin.x) <= EPS) return vec3(-1, 0, 0);
    if (abs(p.x - cmax.x) <= EPS) return vec3(+1, 0, 0);
    if (abs(p.y - cmin.y) <= EPS) return vec3(0, -1, 0);
    if (abs(p.y - cmax.y) <= EPS) return vec3(0, +1, 0);
    if (abs(p.z - cmin.z) <= EPS) return vec3(0, 0, -1);
    if (abs(p.z - cmax.z) <= EPS) return vec3(0, 0, +1);
    return vec3(0, 0, 0);
}

int mod_s(int n, int m)
{
    return (m + (n % m)) % m;
}

int chunkIndex(vec3 p)
{
    vec3 q = p / chunksize;
    q -= vec3(lessThan(q, vec3(0)));

    ivec3 r = ivec3(q);
    int i = mod_s(r.x, csw);
    int j = mod_s(r.y, csh) * csw * csd;
    int k = mod_s(r.z, csd) * csw;

    return i + j + k;
}

Leaf descend(vec3 p, int root)
{
    Leaf leaf = Leaf(Chunk[root].bmin, chunksize, 0);
    uint reg = Chunk[root].tr_region;
    uint off = Chunk[root].tr_offset;
    for (int _step = 0; _step < MAX_DEPTH; ++_step)
    {
        uint value = Tree(reg, off + leaf.offset);
        uint type = Tree_type(value);
        if (type != BRANCH)
            break;
        float halfsize = leaf.size * 0.5;
        vec3 mid = leaf.bmin + halfsize;
        bvec3 geq = greaterThanEqual(p, mid);
        uint branch = Tree_branch(geq.x, geq.y, geq.z);
        vec3 nextpos = leaf.bmin + vec3(geq) * halfsize;
        leaf = Leaf(nextpos, halfsize, Tree_offset(value) + branch);
    }
    return leaf;
}

bool twigmarch(uint i, uint ignore,
    vec3 a, vec3 b, vec3 g,
    vec3 cmin, float size, float leafsize, int root,
    out float s, out Leaf hitleaf, inout int steps)
{
    vec3 cmax = cmin + size;
    vec3 imin = vec3(0);
    vec3 imax = vec3(TWIG_SIZE - 1);

    float inv_leafsize = 1.0 / leafsize;

    uint reg = Chunk[root].tw_region;
    uint off = Chunk[root].tw_offset;

    float t = 0;
    int _step;
    for (_step = 0; _step < MAX_TWIG_STEPS; ++_step)
    {
        vec3 p = a + b * t;
        if (!isInsideCube(p, cmin, cmax))
            break;

        ivec3 offset = ivec3((p - cmin) * inv_leafsize);

        if (!isInsideCube(offset, imin, imax))
            break;

        vec3 leafmin = cmin + offset * leafsize;
        vec3 leafmax = leafmin + leafsize;

        uint word = Twig_dword(offset.x, offset.y, offset.z);
        uint shift = Twig_shift(word);
        uint mask = 0xffff;

        uint bark = (Twig(reg, off + i + word) >> shift) & mask;
        if (bark != 0)
        {
            s = t;
            hitleaf = Leaf(leafmin, leafsize, bark);
            steps += _step;
            return true;
        }
        float escape = cubeEscapeDistance(p, g, leafmin, leafmax) + EPS;
        t += escape;
    }
    steps += _step;
    s = t;
    return false;
}

bool treemarch(vec3 a, vec3 b, vec3 g,
    uint ignore, int root,
    out float s, out Leaf hitleaf, inout int steps)
{
    vec3 rmin = Chunk[root].bmin;
    vec3 rmax = rmin + chunksize;
    uint reg = Chunk[root].tr_region;
    uint off = Chunk[root].tr_offset;

    float t = 0;
    int _step;
    for (_step = 0; _step < MAX_TREE_STEPS; ++_step)
    {
        vec3 p = a + b * t;
        if (!isInsideCube(p, rmin, rmax))
            break;

        Leaf leaf = descend(p, root);
        vec3 leafmin = leaf.bmin;
        vec3 leafmax = leafmin + leaf.size;
        uint value = Tree(reg, off + leaf.offset);
        uint type = Tree_type(value);

        if (type == LEAF)
        {
            hitleaf = Leaf(leafmin, leaf.size, Tree_offset(value));
            s = t;
            steps += _step;
            return true;
        }
        else
        {
            float escape = cubeEscapeDistance(p, g, leafmin, leafmax) + EPS;
            if (type == EMPTY)
                t += escape;
            else
            {
                float leafsize = leaf.size / (1 << TWIG_DEPTH);

                float u = 0;
                if (twigmarch(Twig_offset(value), ignore,
                    p, b, g,
                    leafmin, leaf.size, leafsize, root,
                    u, hitleaf, steps))
                {
                    s = t + u;
                    steps += _step;
                    return true;
                }
                t += escape;
            }
        }
    }
    steps += _step;
    return false;
}

bool rootmarch(vec3 a, vec3 b, vec3 g, out float s, out Leaf hitleaf, inout int steps)
{
    float t = 0;
    bool enter = true;
    if (!isInsideCube(a, chunkmin, chunkmax))
        t = cubeEnterDistance(a, g, chunkmin, chunkmax, enter) + EPS;
    
    if (!enter)
        return false;

    int _step = 0;
    for ( ; _step < MAX_ROOT_STEPS; ++_step)
    {
        vec3 p = a + b * t;
        if (!isInsideCube(p, chunkmin, chunkmax))
            break;
        int r = chunkIndex(p);

        vec3 rmin = Chunk[r].bmin;
        vec3 rmax = rmin + chunksize;

        float u = 0;
        if (treemarch(p, b, g, 0, r, u, hitleaf, steps))
        {
            s = t + u;
            steps += _step;
            return true;
        }
        float escape = cubeEscapeDistance(p, g, rmin, rmax) + EPS;
        t += escape;
    }
    steps += _step;
    return false;
}

in vec3 hit;

out vec4 Color;

void main()
{
    vec3 alpha = eye;
    vec3 beta = normalize(hit - eye);
    vec3 gamma = 1.0 / beta;

    Leaf hitleaf;
    float sigma;
    int steps = 0;
    if (rootmarch(alpha, beta, gamma, sigma, hitleaf, steps))
    {
        vec3 point = alpha + beta * (sigma - EPS);
        vec3 normal = cubeNormal(point, hitleaf.bmin, hitleaf.bmin + hitleaf.size);
        vec3 color = 0.9 * vec3(0.5, 1, 0.2) + 0.1 * normal;

        Color = vec4(color, 1);

        float inv_z = 1.0 / distance(point, eye);
        float inv_near = 1.0 / NEAR;
        float inv_far = 1.0 / FAR;
        float depth = (inv_z - inv_near) / (inv_far - inv_near);
        gl_FragDepth = depth;
    }
    else
    {
        discard;
    }

}