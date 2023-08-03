
uniform samplerCube skybox;

in vec3 uvw;

out vec4 color;

void main()
{
    color = texture(skybox, uvw);
}
