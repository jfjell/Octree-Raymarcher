#version 330 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 vertexUV;

out vec2 uv;

void main(void) {
	gl_Position = vec4(position.x, -position.y, -1.0, 1.0);
	uv = vertexUV;
}
