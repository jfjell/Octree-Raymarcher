
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
#define MAX_TWIG_STEPS 64
#define MAX_DIST 4096.0

#define NEAR 0.125
#define FAR 8192

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

uniform vec3 rpos;
uniform float rsize;
uniform int width;
uniform int rdepth;
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

const vec4 materialLookup[] = {
    vec4(0, 0, 0, 0), // Empty
    vec4(0.5, 0.5, 0.5, 1), // Stone
    vec4(0.7, 0.5, 0.3, 1), // Dirt
    vec4(0.7, 0.8, 0.5, 1), // Sand
    vec4(0.2, 0.6, 0.4, 1), // Grass
    vec4(0.8, 0.1, 0.1, 1), // Custom
    vec4(0.1, 0.3, 0.8, 0.5)  // Water
};

vec4 Bark(uint i) {
    return materialLookup[i];
}


bool twigmarch(uint index, uint ignore,
    vec3 a, vec3 b, vec3 g, 
    vec3 cmin, float size, float leafsize, 
    out float s, out Leaf leaf) {

    vec3 cmax = cmin + size;
    vec3 imin = vec3(0);
    vec3 imax = vec3(TWIG_SIZE - 1);

    float t = 0.0;
    for (int _step = 0; _step < MAX_TWIG_STEPS; ++_step) {
        vec3 p = a + b * t;
        if (!isInsideCube(p, cmin, cmax)) break;

        ivec3 off = ivec3((vec3(p - cmin) * (1.0 / leafsize))); // Integer coordinates

        if (!isInsideCube(off, imin, imax)) break;

        vec3 leafmin = cmin + off * leafsize;
        vec3 leafmax = leafmin + leafsize;

        uint word = Twig_dword(off.x, off.y, off.z);
        uint shift = Twig_shift(word);
        uint mask = 0xffff;

        uint bark = (Twig(index + word) >> shift) & mask;
        if (bark != 0 && bark != ignore) {
            // Hit!
            s = t;
            leaf = Leaf(leafmin, leafsize, bark);
            return true;
        } else {
            // Missed, go next
            float escape = cubeEscapeDistance(p, g, leafmin, leafmax) + EPS;
            t += escape;
        }
    }
    s = t;
    return false;
}

float treemarch(vec3 a, vec3 b, vec3 g, uint ignore, out Leaf hit) {
    vec3 rmin = root.pos;
    vec3 rmax = root.pos + root.size;

    float t = 0.0;
    for (int _step = 0; _step < MAX_STEPS; ++_step) {
        vec3 p = a + b * t;
        if (!isInsideCube(p, rmin, rmax)) break;

        Leaf tree = traverse(p);
        float escape = cubeEscapeDistance(p, g, tree.pos, tree.pos + tree.size) + EPS;
        uint value = Tree(tree.index);
        uint type = Tree_type(value);
        if (type == EMPTY) {
            // Advance to the next subtree, or out of the tree
            t += escape;
        } else if (type == LEAF) {
            // Found a match
            uint bark = Tree_offset(value);
            if (bark != ignore) {
                hit = Leaf(tree.pos, tree.size, bark);
                return t;
            } else {
                t += escape;
            }
        } else if (type == TWIG) {
            float leafsize = tree.size / (1 << TWIG_LEVELS);
            float s = 0.0;
            if (twigmarch(Twig_offset(value), ignore, p, b, g, tree.pos, tree.size, leafsize, s, hit)) {
                return t + s;
            } else {
                t += escape;
            }
        }
    }
    return -1.0;
}

bool isTranslucent(vec4 c) {
    return c.a < 0.9;
}

layout (location = 0) in vec3 hitpos;

out vec4 fragcolor;

struct Stingray {
    float sigma; // Distance
    uint skin; // Material
};

layout (std430, binding = 4) restrict volatile buffer SSBO_STINGRAY {
    Stingray Ray[];
};

vec4 IntToVec4(uint rgba) {
    float r = float(rgba / 16777216) / 255;
    float g = float((rgba / 65536) % 256) / 255;
    float b = float((rgba / 256) % 256) / 255;
    float a = float((rgba % 256)) / 255;
    return vec4(r, g, b, a);
}

uint Vec4ToInt(vec4 rgba) {
    uint r = uint(rgba.r * 255) * 16777216;
    uint g = uint(rgba.g * 255) * 65536;
    uint b = uint(rgba.b * 255) * 256;
    uint a = uint(rgba.a * 255);
    return r | g | b | a;
}

void main() {
    vec3 rmin = root.pos;
    vec3 rmax = rmin + root.size;

    vec3 beta = normalize(hitpos - eye);
    vec3 gamma = 1.0 / beta;
    vec3 alpha = eye;
    if (!isInsideCube(eye, rmin, rmax)) {
        // Fix the cube going invisible in case we hit the far vertex (if we happen to be very close to the edge)
        float t1 = cubeEscapeDistance(hitpos - beta * EPS, -gamma, rmin, rmax);
        float t2 = cubeEscapeDistance(hitpos + beta * EPS, +gamma, rmin, rmax);
        vec3 a1 = hitpos - beta * t1;
        vec3 a2 = hitpos + beta * t2;
        alpha = (distance(alpha, a1) < distance(alpha, a2)) ? a1 : a2;
    }

    /*
    vec3 Cd = vec3(0);
    float Ad = 0; 
    vec3 point = alpha;
    uint ignore = 0;
    while (Ad < 1.0 - EPS) {
        Leaf hit;
        float sigma = treemarch(point, beta, gamma, ignore, hit);
        vec3 nextpoint = point + sigma * beta;
        vec4 bark = Bark(hit.index);
    }
    */

    Leaf hit;
    float sigma = treemarch(alpha, beta, gamma, 0, hit);

    if (sigma < 0) discard; // Out of the tree

    vec3 point = alpha + sigma * beta;

    vec4 bark = Bark(hit.index);

    int pixelcoord = (int(gl_FragCoord.y) * width) + int(gl_FragCoord.x);
    Stingray prevray = Ray[pixelcoord];
    vec4 deadbark = Bark(prevray.skin);
    
    float leafsize = root.size / (1 << rdepth);
    vec4 color = vec4(0);
    if (!isTranslucent(bark)) {
        // Hit something solid, set the color to the material and add the normal as a small factor
        vec3 normal = cubeNormal(point, hit.pos, hit.pos + hit.size);
        color.rgb = (1.0 - deadbark.a * prevray.sigma) * (0.9 * bark.rgb + 0.1 * normal) + (deadbark.a * prevray.sigma * deadbark.rgb);
        color.a = 1;

        Ray[pixelcoord] = Stingray(0, 0);

        fragcolor = color;

        float inv_z = 1.0 / distance(point, eye);
        float inv_near = 1.0 / NEAR;
        float inv_far = 1.0 / FAR;
        float depth = (inv_z - inv_near) / (inv_far - inv_near);
        gl_FragDepth = depth;
    } else {
        // Hit something translucent, continue marching until we hit something solid, 
        uint ignore = hit.index;
        Leaf hit2;
        float sigma2 = treemarch(point, beta, gamma, ignore, hit2);

        if (sigma2 < 0) {
            // Exited out of the tree, add the distance to the exit
            float escape = cubeEscapeDistance(point, gamma, rmin, rmax);
            Ray[pixelcoord] = Stingray(escape + prevray.sigma, hit.index);
            discard;
        } else {
            vec3 point2 = point + beta * sigma2;
            vec4 bark2 = Bark(hit2.index);
            if (!isTranslucent(bark2)) {
                // Hit something solid
                vec3 normal2 = cubeNormal(point2, hit2.pos, hit2.pos + hit2.size);

                vec3 bottomcolor = 0.9 * bark2.rgb + 0.1 * normal2;
                vec3 liquidcolor = bark.rgb;
                float liquidscale = (prevray.sigma + sigma2) * bark.a;
                float bottomscale = 1.0 - liquidscale;
                color.rgb = bottomcolor * bottomscale + liquidcolor * liquidscale;
                color.a = 1;

                Ray[pixelcoord] = Stingray(0, 0);

                fragcolor = color;

                float inv_z = 1.0 / distance(point2, eye);
                float inv_near = 1.0 / NEAR;
                float inv_far = 1.0 / FAR;
                float depth = (inv_z - inv_near) / (inv_far - inv_near);
                gl_FragDepth = depth;
            } else {
                // Should not happen
                color = vec4(1, 0, 0, 1);
                fragcolor = color;

                float inv_z = 1.0 / distance(point2, eye);
                float inv_near = 1.0 / NEAR;
                float inv_far = 1.0 / FAR;
                float depth = (inv_z - inv_near) / (inv_far - inv_near);
                gl_FragDepth = depth;
            }
        }
    }
        
        /*
        //?
            color.rgb = (1.0 - deadbark.a) * prevray.sigma * (bark.a * sigma * bark.rgb);

        while (isTranslucent(color)) {
            // Step until the next material
            float tau = treemarch(point, beta, gamma, ignore, hit);

            if (tau < 0) {
                float a = (1.0 - color.a) * bark.a * escape / leafsize; 
                color.rgb += a * bark.rgb;
                color.a += a;
                break;
            }

            float a = (1.0 - color.a) * bark.a * tau / leafsize; 
            color.rgb += a * bark.rgb;
            color.a += a;

            vec4 bark = Bark(hit.index);

            if (!isTranslucent(bark)) {
                // Hit something solid
                vec3 normal = cubeNormal(point, hit.pos, hit.pos + hit.size);
                color.rgb += (1.0 - color.a) * (0.9 * bark.rgb + 0.1 * normal);
                color.a = 1;
                break;
            }
            point += tau * beta;
            ignore = hit.index;
        }
    }

    color.rgb = (1.0 - prevcolor.a) * color.a * color.rgb + (1.0 - color.a) * prevcolor.a * prevcolor.rgb;
    color.a += prevcolor.a;

    Opacity[coord] = RGBA8888Vec4ToInt(color);

    if (isTranslucent(color)) discard;
    */

}