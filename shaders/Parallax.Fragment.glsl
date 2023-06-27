#version 430 core
 
uniform vec3 eye;
uniform sampler2D sampler;

in vec3 hitpos;
in vec2 uv;
uniform mat4x4 mvp;

struct Root {
    vec3  pos;
    float size;
    uint  depth;
    uint  trees;
    uint  twigs;
};

uniform Root root;

layout(std430, binding = 2) buffer ssboTree {
    uint tree[];
};

layout(std430, binding = 3) buffer ssboTwig {
    uint twig[];
};

float cubeSDF(vec3 point, vec3 center, float radius) {
    vec3 q = abs(point - center) - vec3(radius); 
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

#define INF (1.0 / 0.0)

vec3 cubeInnerNormal(vec3 p, vec3 minpos, vec3 maxpos) {
    vec3 deltamin = minpos - p;
    vec3 deltamax = maxpos - p;
    vec3 delta = length(deltamin) < length(deltamax) ? deltamin : deltamax;
    vec3 norm = vec3(INF, INF, INF);
    for (int i = 0; i < 3; ++i) {
        if (abs(delta[i]) > length(norm)) continue;
        norm = vec3(0, 0, 0);
        norm[i] = delta[i];
    }
    vec3 midpos = minpos + (maxpos - minpos) * 0.5;
    float size = maxpos.x - minpos.x;
    if (cubeSDF(p, midpos, size * 0.5) <= 0.0) norm = -norm;
    return normalize(norm);
}

#define EMPTY  0
#define LEAF   1
#define BRANCH 2
#define TWIG   3

uint type(uint t) {
    return t >> 30;
}

uint offset(uint t) {
    return t & ~(3 << 30);
}

uint branch(bool xg, bool yg, bool zg) {
    return int(xg) + int(yg) * 2 + int(zg) * 4;
}

float rootSDF(vec3 point) {
    return cubeSDF(point, root.pos + root.size * 0.5, root.size * 0.5);
}

struct TreeBranch {
    vec3 minpos;
    float size;
    uint index;
};

bool enclosingTree(vec3 p, out TreeBranch intersect) {
    if (rootSDF(p) > 0.0) 
        return false;

    TreeBranch t = TreeBranch(root.pos, root.size, 0);
    for ( ; ; ) {
        uint species = type(tree[t.index]);
        if (species == EMPTY || species == LEAF || species == TWIG) 
            break;

        float halfsize = t.size * 0.5;
        vec3 c = t.minpos + halfsize;
        bool xg = p.x > c.x;
        bool yg = p.y > c.y;
        bool zg = p.z > c.z;
        uint subtree = branch(xg, yg, zg);
        vec3 nextpos = t.minpos + (vec3(float(xg), float(yg), float(zg)) * halfsize);
        t = TreeBranch(nextpos, halfsize, offset(tree[t.index]) + subtree);
    }

    intersect = t;
    return true;
}

#define TWIG_SIZE 4
#define TWIG_LEVELS 2

uint TreeOffset(uint t) {
    return offset(t);
}

uint TwigOffset(uint t) {
    return offset(t) * 2;
}

uint word(uint x, uint y, uint z) {
    return z / 2;
}

uint bit(uint x, uint y, uint z) {
    return (z / 2) * 16 + y * 4 + x;
}

#define MAX_DIST 65535

bool intersectsTwig(vec3 p, uint index, vec3 minpos, float leafsize, vec3 norm) {
    vec3 i = p - minpos;
    uint x = uint(i.x / leafsize);
    uint y = uint(i.y / leafsize);
    uint z = uint(i.z / leafsize);
    uint w = word(x, y, z);
    uint b = bit(x, y, z);
    bool t = (twig[index + w] & (1 << b)) != 0;
    norm = cubeInnerNormal(p, minpos + vec3(x, y, z) * leafsize, minpos + (vec3(x, y, z) + 1) * leafsize);
    return t;
}

float raymarch(vec3 ro, vec3 rd, out vec3 norm) {
    float eps = root.size / pow(2.0, float(root.depth) + TWIG_LEVELS);
    float s = 0.0;

    for ( ; ; ) {
        vec3 p = ro + rd * s;
        TreeBranch t;
        if (enclosingTree(p, t)) {
            float edge = -cubeSDF(p, t.minpos + t.size * 0.5, t.size * 0.5);

            uint species = type(tree[t.index]);
            if (species == EMPTY) {
                // Advance to the next subtree, or out of the tree
                s += edge + eps;
            } else if (species == LEAF) {
                // Found a match
                norm = cubeInnerNormal(p, t.minpos, t.minpos + t.size);
                return s;
            } else if (species == TWIG) {
                float leafsize = t.size / pow(2, TWIG_LEVELS);
                vec3 n;
                if (intersectsTwig(p, TwigOffset(tree[t.index]), t.minpos, leafsize, n)) {
                    norm = n;
                    return s;
                }
                s += eps;
            }
        } else {
            // Out of the root!
            return MAX_DIST;
        }
    }
}

void main(void) {
    float eps = root.size / pow(2, root.depth);
    vec3 ro = eye;
    vec3 rd = normalize(hitpos - ro);
    vec3 init = rootSDF(ro) < 0.0 ? ro : hitpos + eps * rd;
    vec3 normal;
    float dist = raymarch(init, rd, normal);

    if (dist < MAX_DIST) {
        vec3 p = init + rd * dist;
        vec3 relative = (p - root.pos) / root.size;
        vec3 tex = texture(sampler, relative.xz).rgb;
        gl_FragColor = vec4(tex + normal, 1);
    } else {
        discard;
    }
}
