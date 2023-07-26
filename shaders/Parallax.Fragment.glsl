
#define EMPTY  0
#define LEAF   1
#define BRANCH 2
#define TWIG   3

#define TWIG_SIZE 4
#define TWIG_LEVELS 2
#define TWIG_WORDS 64
#define TWIG_DWORDS 32

#define MAX_DEPTH 32
#define MAX_STEPS 512
#define MAX_TWIG_STEPS 24
#define MAX_DIST 4096.0

const float EPS = 1.0 / 4096.0;

struct Root {
    vec3  pos;
    float size;
};

struct Leaf {
    vec3  pos;
    float size;
    uint  index;
};

uniform vec3 eye;
uniform mat4x4 mvp;
uniform Root root;

layout(std430, binding = 2) restrict readonly buffer ssboTree {
    uint Tree_SSBO[];
};

layout(std430, binding = 3) restrict readonly buffer ssboTwig {
    uint Twig_SSBO[];
};

uint Tree(uint i) {
    return Tree_SSBO[i];
}

uint Twig(uint i) {
    return Twig_SSBO[i];
}

uint Tree_type(uint t) {
    return t >> 30;
}

uint Tree_offset(uint t) {
    return t & ~(3 << 30);
}

uint Tree_branch(bool xg, bool yg, bool zg) {
    return int(xg) + int(yg) * 2 + int(zg) * 4;
}

uint Twig_offset(uint t) {
    return Tree_offset(t) * TWIG_DWORDS;
}

uint Twig_dword(uint x, uint y, uint z) {
    return (z * 16 + y * 4 + x) / 2;
}

uint Twig_shift(uint i) {
    return i % 2 != 0 ? 16 : 0;
}

bool isInsideCube(vec3 p, vec3 cmin, vec3 cmax) {
    bvec3 geq = greaterThanEqual(p, cmin);
    bvec3 leq = greaterThanEqual(cmax, p);
    return all(geq) && all(leq);
}

float cubeEscapeDistance(vec3 a, vec3 g, vec3 cmin, vec3 cmax) {
    vec3 tmin = (cmin - a) * g;
    vec3 tmax = (cmax - a) * g;
    vec3 t = max(tmin, tmax);
    return min(t.x, min(t.y, t.z));
}

vec3 cubeNormal(vec3 p, vec3 cmin, vec3 cmax) {
    vec3 normal = vec3(0);
    if (abs(p.x - cmin.x) <= EPS) normal += vec3(-1, 0, 0);
    if (abs(p.x - cmax.x) <= EPS) normal += vec3(+1, 0, 0);
    if (abs(p.y - cmin.y) <= EPS) normal += vec3(0, -1, 0);
    if (abs(p.y - cmax.y) <= EPS) normal += vec3(0, +1, 0);
    if (abs(p.z - cmin.z) <= EPS) normal += vec3(0, 0, -1);
    if (abs(p.z - cmax.z) <= EPS) normal += vec3(0, 0, +1);
    return normalize(normal);
}

Leaf traverse(vec3 p) {
    Leaf tree = Leaf(root.pos, root.size, 0);
    for (int d = 0; d < MAX_DEPTH; ++d) {
        uint value = Tree(tree.index);
        uint type = Tree_type(value);
        if (type != BRANCH) break;
        float halfsize = tree.size * 0.5;
        vec3 mid = tree.pos + halfsize;
        bvec3 geq = greaterThanEqual(p, mid);
        uint branch = Tree_branch(geq.x, geq.y, geq.z);
        vec3 nextpos = tree.pos + vec3(geq) * halfsize;
        tree = Leaf(nextpos, halfsize, Tree_offset(value) + branch);
    }
    return tree;
}

bool twigmarch(uint index, vec3 a, vec3 b, vec3 g, vec3 cmin, float size, float leafsize, out float s, out Leaf tree) {
    float t = 0.0;
    for (int stw = 0; stw < MAX_TWIG_STEPS; ++stw) {
        vec3 p = a + b * t;
        if (!isInsideCube(p, cmin, cmin + size)) break;

        ivec3 off = ivec3((vec3(p - cmin) * (1.0 / leafsize))); // Integer coordinates

        if (!isInsideCube(off, vec3(0), vec3(TWIG_SIZE-1))) break;

        vec3 leafmin = cmin + off * leafsize;
        vec3 leafmax = leafmin + leafsize;

        uint word = Twig_dword(off.x, off.y, off.z);
        uint shift = Twig_shift(word);
        uint mask = 0xffff;

        uint bark = (Twig(index + word) >> shift) & mask;
        if (bark != 0) {
            // Hit!
            s = t - EPS;
            tree = Leaf(leafmin, leafsize, bark);
            return true;
        } else {
            // Missed, go next
            float stepsize = cubeEscapeDistance(p, g, leafmin, leafmax);
            t += stepsize + EPS;
        }
    }
    s = t;
    return false;
}

float treemarch(vec3 a, vec3 b, vec3 g, out Leaf treeHit) {
    vec3 rmin = root.pos;
    vec3 rmax = root.pos + root.size;

    float t = 0.0;

    for (int tst = 0; tst < MAX_STEPS; ++tst) {
        vec3 p = a + b * t;
        if (!isInsideCube(p, rmin, rmax)) break;

        Leaf tree = traverse(p);
        uint value = Tree(tree.index);
        uint type = Tree_type(value);
        if (type == EMPTY) {
            // Advance to the next subtree, or out of the tree
            float escape = cubeEscapeDistance(p, g, tree.pos, tree.pos + tree.size);
            t += escape + EPS;
        } else if (type == LEAF) {
            // Found a match
            uint material = Tree_offset(value);
            treeHit = Leaf(tree.pos, tree.size, material);
            return t - EPS;
        } else if (type == TWIG) {
            float leafsize = tree.size / (1 << TWIG_LEVELS);
            float s = 0.0;
            if (twigmarch(Twig_offset(value), p, b, g, tree.pos, tree.size, leafsize, s, treeHit)) {
                return t + s;
            } else {
                t += s;
            }
        }
    }
    return MAX_DIST;
}

#define MATERIALS 6

const vec4 materialLookup[MATERIALS] = {
    vec4(0.5, 0.5, 0.5, 1), // Stone
    vec4(0.7, 0.5, 0.3, 1), // Dirt
    vec4(0.7, 0.8, 0.5, 1), // Sand
    vec4(0.2, 0.6, 0.4, 1), // Grass
    vec4(0.8, 0.1, 0.1, 1), // Custom
    vec4(0.1, 0.3, 0.8, 0.5)  // Water
};

layout (location = 0) in vec3 hitpos;

out vec4 fragcolor;

void main() {
    Leaf tree;
    vec3 beta = normalize(hitpos - eye);
    vec3 gamma = 1.0 / beta;
    vec3 alpha = eye;
    if (!isInsideCube(eye, root.pos, root.pos + root.size)) {
        // Fix the cube going invisible in case we hit the far vertex (if we happen to be very close to the edge)
        float t1 = cubeEscapeDistance(hitpos - beta * EPS, -gamma, root.pos, root.pos + root.size);
        float t2 = cubeEscapeDistance(hitpos + beta * EPS, +gamma, root.pos, root.pos + root.size);
        vec3 a1 = hitpos - beta * t1;
        vec3 a2 = hitpos + beta * t2;
        alpha = (distance(alpha, a1) < distance(alpha, a2)) ? a1 : a2;
    }

    float sigma = treemarch(alpha, beta, gamma, tree);

    if (sigma < MAX_DIST) {
        vec3 point = alpha + beta * (sigma + EPS);
        // vec3 relative = (point - root.pos) / root.size;
        // vec3 texturecolor = texture(sampler, relative.xz).rgb;
        vec3 normal = cubeNormal(point, tree.pos, tree.pos + tree.size);
        // vec4 color = vec4(texturecolor + normal * 0.5, 1);
        uint material = tree.index - 1;
        vec4 color = vec4(materialLookup[material].xyz + normal * 0.1, 1);
        fragcolor = color;

        float z = 1.0 / distance(point, eye);
        float near = 1.0 / 0.1;
        float far = 1.0 / 10000.0;
        float depth = (z - near) / (far - near);
        gl_FragDepth = depth;
    } else {
        gl_FragDepth = 1.0;
        discard;
    }
}