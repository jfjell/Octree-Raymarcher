
uniform mat4 model;
uniform mat4 mvp;
uniform float t;

layout (location = 0) in vec3 vertexcoord;
layout (location = 1) in vec3 uvwcoord;

out vec3 uvw;

void main() {
    int cube = gl_VertexID / 8;
    float x = 0;
    float z = cos(t + cube) * 0.3;
    float y = sin(t + cube) * 0.5;
    gl_Position = mvp * model * vec4(vertexcoord + vec3(x, y, z), 1);
    uvw = uvwcoord;
}