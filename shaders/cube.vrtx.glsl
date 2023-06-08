#version 330 core

uniform mat4x4 mvp;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 vertexUV;

out vec2 uv;

void main(void) {
	gl_Position = mvp * vec4(position, 1.0);
	uv = vertexUV;
}
