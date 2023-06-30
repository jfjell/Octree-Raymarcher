#version 430 core

#extension AMD_gpu_shader_int16 : enable

#define EMPTY  0
#define LEAF   1
#define BRANCH 2
#define TWIG   3

#define TWIG_SIZE 4
#define TWIG_LEVELS 2
#define TWIG_WORDS 2

#define MAX_DEPTH 32
#define MAX_STEPS 512
#define MAX_TWIG_STEPS 24
#define MAX_DIST 256.0

const float EPS = 1.0 / 4096.0;

struct Root {
    vec3  pos;
    float size;
    uint  depth;
    uint  trees;
    uint  twigs;
};

struct Tree {
    vec3  pos;
    float size;
    uint  index;
};

layout(location = 0) in vec3 hitpos;
layout(location = 1) in vec2 uv;

uniform sampler2D sampler;
uniform vec3 eye;
uniform mat4x4 mvp;
uniform Root root;

layout(std430, binding = 2) restrict readonly buffer ssboTree {
    uint Tree_SSBO[];
};

layout(std430, binding = 3) restrict readonly buffer ssboTwig {
    uint Twig_SSBO[];
};

layout(std430, binding = 4) restrict readonly buffer ssboBark {
    uint16_t Bark_SSBO[];
};

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
    return Tree_offset(t) * TWIG_WORDS;
}

uint Twig_word(uint x, uint y, uint z) {
    return z / 2;
}

uint Twig_bit(uint x, uint y, uint z) {
    return (z / 2) * 16 + y * 4 + x;
}

uint Bark_offset(uint t) {
    if (Tree_type(t) == LEAF)
        return Bark_SSBO.length() - Tree_offset(t) - 1;
    else if (Tree_type(t) == TWIG)
        return Tree_offset(t);
}

bool isInsideCube(vec3 p, vec3 cmin, vec3 cmax) {
    bvec3 geq = greaterThanEqual(p, cmin);
    bvec3 leq = greaterThanEqual(cmax, p);
    return all(geq) && all(leq);
}

float cubeEscapeDistance(vec3 a, vec3 b, vec3 cmin, vec3 cmax) {
    vec3 tmin = (cmin - a) * (1.0 / b);
    vec3 tmax = (cmax - a) * (1.0 / b);
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

Tree enclosingTree(vec3 p) {
    Tree tree = Tree(root.pos, root.size, 0);
    for (int d = 0; d < MAX_DEPTH; ++d) {
        uint value = Tree_SSBO[tree.index];
        uint type = Tree_type(value);
        if (type != BRANCH) break;
        float halfsize = tree.size * 0.5;
        vec3 mid = tree.pos + halfsize;
        bvec3 geq = greaterThanEqual(p, mid);
        uint branch = Tree_branch(geq.x, geq.y, geq.z);
        vec3 nextpos = tree.pos + vec3(geq) * halfsize;
        tree = Tree(nextpos, halfsize, Tree_offset(value) + branch);
    }
    return tree;
}

bool raymarchTwig(uint index, vec3 a, vec3 b, vec3 cmin, float size, float leafsize, out float s, out Tree tree) {
    float halfsize = size * 0.5;
    float t = 0.0;
    for (int stw = 0; stw < MAX_TWIG_STEPS; ++stw) {
        vec3 p = a + b * t;
        ivec3 off = ivec3((vec3(p - cmin) * (1.0 / leafsize))); // Integer coordinates

        if (!isInsideCube(off, vec3(0), vec3(TWIG_SIZE-1))) break;

        uint word = Twig_word(off.x, off.y, off.z);
        uint bit  = Twig_bit(off.x, off.y, off.z);
        vec3 leafmin = cmin + vec3(off) * leafsize;
        vec3 leafmax = leafmin + leafsize;

        if ((Twig_SSBO[index + word] & (1 << bit)) != 0) {
            // Hit!
            s = t - EPS;
            tree = Tree(leafmin, leafsize, 0);
            return true;
        } else {
            // Missed, go next
            float stepsize = cubeEscapeDistance(p, b, leafmin, leafmax);
            t += stepsize + EPS;
        }
    }
    return false;
}

float raymarchTree(vec3 a, vec3 b, out Tree treeHit) {
    float t = 0.0;

    for (int tst = 0; tst < MAX_STEPS; ++tst) {
        vec3 p = a + b * t;
        if (!isInsideCube(p, root.pos, root.pos + root.size)) break;

        Tree tree = enclosingTree(p);
        uint value = Tree_SSBO[tree.index];
        uint type = Tree_type(value);
        if (type == EMPTY) {
            // Advance to the next subtree, or out of the tree
            float escape = cubeEscapeDistance(p, b, tree.pos, tree.pos + tree.size);
            t += escape + EPS;
        } else if (type == LEAF) {
            // Found a match
            treeHit = Tree(tree.pos, tree.size, tree.index);
            return t - EPS;
        } else if (type == TWIG) {
            float leafsize = tree.size / pow(2.0, float(TWIG_LEVELS));
            float s = 0.0;
            if (raymarchTwig(Twig_offset(value), p, b, tree.pos, tree.size, leafsize, s, treeHit)) {
                treeHit = Tree(treeHit.pos, leafsize, tree.index);
                return t + s;
            } else {
                float escape = cubeEscapeDistance(p, b, tree.pos, tree.pos + tree.size);
                t += escape + EPS;
            }
        }
    }
    return MAX_DIST;
}

#define MATERIALS 4

const vec4 materialLookup[MATERIALS] = {
    vec4(0.5, 0.5, 0.5, 1), // Stone
    vec4(0.7, 0.5, 0.3, 1), // Dirt
    vec4(0.7, 0.8, 0.5, 1), // Sand
    vec4(0.2, 0.6, 0.4, 1), // Grass
};

void main(void) {
    Tree tree;
    vec3 beta = normalize(hitpos - eye);
    // vec3 alpha = isInsideCube(eye, root.pos, root.pos + root.size) ? eye : hitpos + beta * EPS; 
    vec3 alpha = eye;
    if (!isInsideCube(alpha, root.pos, root.pos + root.size)) {
        // Fix the cube going invisible incase we hit the far vertex (if we happen to be very close to the edge)
        float t1 = cubeEscapeDistance(hitpos - beta * EPS, -beta, root.pos, root.pos + root.size);
        float t2 = cubeEscapeDistance(hitpos + beta * EPS, +beta, root.pos, root.pos + root.size);
        vec3 a1 = hitpos - beta * t1;
        vec3 a2 = hitpos + beta * t2;
        if (distance(alpha, a1) < distance(alpha, a2))
            alpha = a1;
        else
            alpha = a2;
    }
    if (!isInsideCube(alpha, root.pos, root.pos + root.size)) alpha = hitpos;
    float sigma = raymarchTree(alpha, beta, tree);

    if (sigma < MAX_DIST) {
        vec3 point = alpha + beta * sigma;
        // vec3 relative = (point - root.pos) / root.size;
        // vec3 texturecolor = texture(sampler, relative.xz).rgb;
        vec3 normal = cubeNormal(point, tree.pos, tree.pos + tree.size);
        // vec4 color = vec4(texturecolor + normal * 0.5, 1);
        uint material = Bark_SSBO[Bark_offset(Tree_SSBO[tree.index])];
        vec4 color = vec4(materialLookup[material].xyz + normal * 0.1, 1);
        gl_FragColor = color;
    } else {
        discard;
    }
}
