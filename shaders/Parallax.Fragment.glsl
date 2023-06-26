#version 330 core
 
uniform vec3 cameraOrigin;
uniform sampler2D sampler;

in vec3 vtxhitpos;
in vec2 uv;
uniform mat4x4 mvp;

out vec4 color;
 
#define MAX_STEPS 100
#define MAX_DIST  100
#define SURF_DIST 1e-3

float cubeSDF(vec3 p, vec3 c, float r) {
    return 0;
}

float sphereSDF2(vec3 p, vec3 s, float r) {
    return length(s - p) - r;
}

float sphereSDF(vec3 p) {
    float d = length(p) - .5;
    return d;
}

float torusSDF(vec3 p) {
    float d = length(vec2(length(p.xz) - .5, p.y)) - .1;
    return d;
}

float minDist(vec3 p) {
    return sphereSDF2(p, vec3(3, 0, 0), 1);
}

#define EPS 1e-2

vec3 surfaceNormal(vec3 p) {
    vec2 e = vec2(EPS, 0);
    vec3 n = minDist(p) - vec3(minDist(p+e.xyy), minDist(p+e.yxy), minDist(p+e.yyx));
    return normalize(n);
}

float raymarch(vec3 eye, vec3 look) {
    float dist = 0, left; // Distance from eye, distance from target

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 p = eye + look * dist;
        left = minDist(p);
        dist += left;
        if (dist < SURF_DIST || dist > MAX_DIST) break;
    }

    return dist;
}

void main(void) {
    color = texture(sampler, uv).rgba;
    color = vec4(0, 0, 0, 1);
    vec3 eye = (vec4(cameraOrigin.xyz, 1.0)).xyz; // vec3(0, 0, -3);
    vec3 look = normalize(vtxhitpos - eye); // normalize(vec3(uv.xy - .5, 1));
    float d = raymarch(eye, look);
    if (d < MAX_DIST) {
        vec3 p = eye + look * d;
        vec3 n = surfaceNormal(p);
        color.rgb = n;
    }
}
