
uniform mat4x4 mvp;
uniform mat4x4 model;

layout(location = 0) in vec3 vertexcoord;

void main() 
{
	gl_Position = mvp * model * vec4(vertexcoord, 1.0);
}