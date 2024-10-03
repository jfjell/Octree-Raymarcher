
uniform mat4x4 mvp;
uniform mat4x4 shadowVP;
uniform mat4x4 model;

layout(location = 0) in vec3 vertexcoord;

out vec3 hitpoint;
out vec4 shadowHitpoint;

void main() 
{
	gl_Position = mvp * model * vec4(vertexcoord, 1.0);
	hitpoint = vec3(model * vec4(vertexcoord, 1.0));
	shadowHitpoint = shadowVP * vec4(hitpoint, 1.0);
}
