 
uniform sampler2D tex;

in vec2 uv;

out vec4 fragcolor;

void main() {
    fragcolor = texture(tex, uv).rgba;
}
