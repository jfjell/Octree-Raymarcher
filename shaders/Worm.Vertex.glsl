
uniform mat4 model;
uniform mat4 mvp;
uniform float t;

layout (location = 0) in vec3 vertexcoord;
layout (location = 1) in vec3 uvwcoord;

out vec3 uvw;

void main() {
    int cube = gl_VertexID / 8;
    float x = 0; // cos(t + cube) * 0.3;
    float y = sin(t + cube) * 1.3;
    float z = 0;
    gl_Position = mvp * model * vec4(vertexcoord, 1) + vec4(x, y, z, 0);
    uvw = uvwcoord;
}