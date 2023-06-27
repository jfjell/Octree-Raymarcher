#version 430 core

uniform mat4x4 mvp;

layout(location = 0) in vec3 vertexcoord;
layout(location = 1) in vec2 uvcoord;

out vec3 hitpos;
out vec2 uv;

void main(void) {
	gl_Position = mvp * vec4(vertexcoord, 1.0);
	uv = vec2(uvcoord.x, 1.0 - uvcoord.y);
	hitpos = vertexcoord;
}
