#version 330 core

uniform mat4x4 mvp;

layout(location = 0) in vec3 vertexcoord;
layout(location = 1) in vec3 uvwcoord;

out vec3 uvw;

void main(void) {
	gl_Position = mvp * vec4(vertexcoord, 1.0);
	uvw = uvwcoord;
}
