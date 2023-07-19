
uniform samplerCube tex;

in vec3 uvw;

out vec4 color;

void main() {
    color = texture(tex, uvw).rgba;
}