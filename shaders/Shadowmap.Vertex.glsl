
layout (location = 0) in vec3 vertexClipPosition;

uniform mat4 shadowViewProj;
uniform mat4 model;

void main()
{
    gl_Position = shadowViewProj * model * vec4(vertexClipPosition, 1.0);
}