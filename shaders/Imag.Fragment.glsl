
#define EPS (1.0 / 64.0)

in vec3 coord;

out vec4 fragcolor;

bool isEdge(float p) {
    return p <= EPS || p >= 1.0 - EPS;
}


void main() {
    if (int(isEdge(coord.x)) + int(isEdge(coord.y)) + int(isEdge(coord.z)) >= 2)
        fragcolor = vec4(0, 0, 0, 1);
    else
        fragcolor = vec4(0.8, 0.8, 0.8, 0.2);
}
