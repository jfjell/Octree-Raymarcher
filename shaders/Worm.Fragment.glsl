
uniform samplerCube tex;

in vec3 uvw;

out vec4 fragcolor;

void main() {
    fragcolor = texture(tex, uvw).rgba;
}