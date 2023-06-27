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

out vec4 color;
 
#define MAX_STEPS 100
//#define MAX_DIST  100
#define SURF_DIST 1e-3

float cubeSDF(vec3 point, vec3 center, float radius) {
    vec3 q = abs(point - center) - vec3(radius); 
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

/*
float minDist(vec3 p) {
    return cubeSDF(p, vec3(0, 1, 0), 0.3);
}

#define EPS 1e-2

vec3 surfaceNormal(vec3 p) {
    vec2 e = vec2(EPS, 0);
    vec3 n = minDist(p) - vec3(minDist(p+e.xyy), minDist(p+e.yxy), minDist(p+e.yyx));
    return normalize(n);
}

float raymarch_(vec3 eye, vec3 look) {
    float dist = 0, left; // Distance from eye, distance from target

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 p = eye + look * dist;
        left = minDist(p);
        dist += left;
        if (dist < SURF_DIST || dist > MAX_DIST) break;
    }

    return dist;
}

void old() {
    color = vec4(0, 0, 0, 1);

    vec3 ro = (vec4(eye.xyz, 1.0)).xyz; 
    vec3 rd = normalize(hitpos - ro); 

    float dist = raymarch_(ro, rd);
    if (dist > MAX_DIST) discard;

    vec3 p = ro + rd * dist;
    vec3 n = surfaceNormal(p);
    color.rgb = n + vec3(0.5, 0.5, 0.5);
    
    if (twig.length() > 0)
        color.rgb += vec3(0.1, 0.1, 0.1);
}
*/

#define TREE_EPS 1.0 / (1 << root.depth)

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

bool isInside(vec3 p) {
    return (cubeSDF(p, root.pos, root.size) < 0.0);
}

uint branch(bool xg, bool yg, bool zg) {
    return int(xg) + int(yg) * 2 + int(zg) * 4;
}

/*
#define MAXSTEPS 100

float walk(vec3 point, vec3 direction, out bool found) {
    float size = root.size;
    vec3 lowpos = root.pos;
    uint t = tree[0];
    float s = 0.0;

    for (int step = 0; step < MAXSTEPS; ++step) {
        float halfsize = size * 0.5;
        vec3 midpos = lowpos + halfsize;
        if (type(t) == EMPTY) {
            // Walk forward until we pass over the wall and return false
            float d = 0.0;
            do { 
                d = cubeSDF(point + direction * s, midpos, halfsize);
                s += abs(d);
                ++step;
            } while (d < 0.0 && step < MAXSTEPS);

            found = false;
            return s;

        } else if (type(t) == LEAF) {
            // Found!
            found = true;
            return s;

        } else if (type(t) == BRANCH) {
            vec3 p = point + direction * s;
            uint next = offset(t) + branch(p.x > midpos.x, p.y > midpos.y, p.z > midpos.z);
            
            t = tree[next];
            size = halfsize;
        } else /* type(t) == TWIG */ //{
            /*found = true;
            return s;
        }
    }
    found = false;
    return s;
}
*/

float rootSDF(vec3 point) {
    return cubeSDF(point, root.pos + root.size * 0.5, root.size * 0.5);
}

/*
float treeSDF(vec3 p) {
    if (rootSDF(p) > 0.0) return rootSDF(p);

    vec3  corner   = root.pos;
    float halfsize = root.size * 0.5;
    uint  index    = 0;
    for ( ; ; ) {
        vec3 center = corner + halfsize;
        if (type(tree[index]) == EMPTY) return -cubeSDF(p, center, halfsize);
        if (type(tree[index]) == LEAF)  return cubeSDF(p, center, halfsize);
        if (type(tree[index]) == TWIG)  return cubeSDF(p, center, halfsize);

        bool xg = p.x > center.x;
        bool yg = p.y > center.y;
        bool zg = p.z > center.z;
        index = tree[offset(tree[index]) + branch(xg, yg, zg)];
        corner += vec3(int(xg), int(yg), int(zg)) * halfsize;
        halfsize *= 0.5;
    }
}
*/

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

/*
uint pointType(vec3 p) {
    vec3  corner   = root.pos;
    float halfsize = root.size * 0.5;
    uint  index    = 0;

    for ( ; ; ) {
        vec3 center = corner + halfsize;
        if (cubeSDF(p, center, halfsize) > 0)
            return 0;
        
        if (type(tree[index]) == EMPTY)
            return 1;
        if (type(tree[index]) == LEAF)
            return 2;
        if (type(tree[index]) == TWIG)
            return 3;
        
        if (type(tree[index]) != BRANCH) return 100;

        bool xg = p.x > center.x;
        bool yg = p.y > center.y;
        bool zg = p.z > center.z;
        index = tree[offset(tree[index]) + branch(xg, yg, zg)];
        corner += vec3(int(xg), int(yg), int(zg)) * halfsize;
        halfsize *= 0.5;
    }
}

void raymarch5(vec3 origin, vec3 direction) {
    float epsilon = 1.0 / (1 << root.depth);
    float dist = length(hitpos - origin) + epsilon;

    vec3 p = origin + direction * dist;

    uint pt = pointType(p);
    if (pt == 0)
        color = vec4(0, 0, 0, 1);
    if (pt == 100)
        color = vec4(1, 1, 1, 1) * 0.5;
    if (pt == 1)
        color = vec4(1, 0, 0, 1);
    if (pt == 2)
        color = vec4(0, 1, 0, 1);
    if (pt == 3)
        color = vec4(0, 0, 1, 1);
    return;

    if (rootSDF(p) <= 0) {
        color = vec4(1, 0, 0, 0);
        return;
    }
    else
        discard;

    bool found = false;
    for (int step = 0; step < MAXSTEPS && !found && rootSDF(p) < 0.0; ++step) {
        p += direction * epsilon;
        walk(p, direction, found);
    }
    if (found)
        color = vec4(1, 1, 1, 1);
    else
        color = vec4(0, 0, 0, 0);
}

float rm2(vec3 ro, vec3 rd) {
    float dist = 0, left; // Distance from eye, distance from target

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 p = ro + rd * dist;
        left = treeSDF(p);
        dist += left;
        if (dist < 0.0 || dist > MAX_DIST) break;
    }

    return dist;
}
*/

/*
float cubeInnerDistance(vec3 ro, vec3 rd, vec3 minpos, float size) {
    float eps = root.size / pow(2, root.depth);
    float halfsize = size * 0.5;
    vec3 midpos = minpos + halfsize;

    float s = 0.0, d = 0.0;
    do {
        d = cubeSDF(ro + rd * s, midpos, halfsize);
        s += abs(d) + eps;
    } while(d < 0.0);

    return s;
}
*/

/*
#define INF (1.0 / 0.0)

float cubeInnerDistance2(vec3 ro, vec3 rd, vec3 minpos, float size) {
    vec3 maxpos = minpos + size;
    float tmin = -INF, tmax = INF;
    for (int i = 0; i < 3; ++i) {
        float t1 = minpos[i] - (ro[i] / rd[i]);
        float t2 = maxpos[i] - (ro[i] / rd[i]);
        tmin = max(tmin, min(t1, t2));
        if (max(t1, t2) > 0.0) tmax = min(tmax, max(t1, t2));
    }
    return tmax;
}
*/

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

bool intersectsTwig(vec3 p, uint index, vec3 minpos, float leafsize) {
    vec3 i = p - minpos;
    uint x = uint(i.x / leafsize);
    uint y = uint(i.y / leafsize);
    uint z = uint(i.z / leafsize);
    uint w = word(x, y, z);
    uint b = bit(x, y, z);
    return (twig[index + w] & (1 << b)) != 0;
}

float raymarch(vec3 ro, vec3 rd) {
    float eps = root.size * 0.5 / pow(2, root.depth);
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
                break;
            } else if (species == TWIG) {
                float leafsize = t.size / pow(2, TWIG_LEVELS);
                if (intersectsTwig(p, TwigOffset(tree[t.index]), t.minpos, leafsize))
                    break;
                s += eps * 0.5;
            }
        } else {
            // Out of the root!
            return MAX_DIST;
        }
    }
    return s;
}

void main(void) {
    float eps = root.size / pow(2, root.depth);
    vec3 ro = eye;
    vec3 rd = normalize(hitpos - ro);
    vec3 init = rootSDF(ro) < 0.0 ? ro : hitpos + eps * rd;
    float dist = raymarch(init, rd);

    if (dist < MAX_DIST) {
        vec3 p = init + rd * dist;
        vec3 relative = (p - root.pos) / root.size;
        color = texture(sampler, relative.xz).rgba;
    } else {
        discard;
    }
        // color = vec4(0, 0, 0, 1);
    /*
    TreeBranch t;
    if (enclosingTree(init, t)) {
        uint species = type(tree[t.index]);
        if (species == EMPTY)
            color = vec4(cubeInnerDistance(init, rd, t.minpos, t.size), 0.2, 0.2, 1);
        else if (species == LEAF)
            color = vec4(0, 1, 0, 1);
        else if (species == TWIG)
            color = vec4(1, 1, 0, 1);
        else if (species == BRANCH)
            color = vec4(0, 0, 1, 1);
        else
            color = vec4(1, 1, 1, 1);
    } else {
        color = vec4(0, 0, 0, 1);
    }
    */
}
