
uniform mat4x4 mvp;
uniform mat4x4 model;

layout(location = 0) in vec3 vertexcoord;

layout(location = 0) out vec3 hitpos;

void main() {
	gl_Position = mvp * model * vec4(vertexcoord, 1.0);
	hitpos = (model * vec4(vertexcoord, 1.0)).xyz;
}
