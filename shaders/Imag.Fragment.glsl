#version 430 core

#define EPS (1.0 / 64.0)

in vec3 coord;

bool isEdge(float p) {
    return p <= EPS || p >= 1.0 - EPS;
}

void main() {
    if (int(isEdge(coord.x)) + int(isEdge(coord.y)) + int(isEdge(coord.z)) >= 2)
        gl_FragColor = vec4(0, 0, 0, 1);
    else
        gl_FragColor = vec4(0.4, 0.4, 0.4, 0.2);
}
