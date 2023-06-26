#version 330 core
 
uniform samplerCube sampler;

in vec3 uvw;

out vec4 color;
 
void main(void) {
    color = texture(sampler, uvw).rgba;
}
