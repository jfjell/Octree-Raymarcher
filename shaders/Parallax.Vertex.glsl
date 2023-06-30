#version 430 core

uniform mat4x4 mvp;

layout(location = 0) in vec3 vertexcoord;
layout(location = 1) in vec2 uvcoord;

layout(location = 0) out vec3 hitpos;

void main(void) {
	gl_Position = mvp * vec4(vertexcoord, 1.0);
	hitpos = vertexcoord;
}
