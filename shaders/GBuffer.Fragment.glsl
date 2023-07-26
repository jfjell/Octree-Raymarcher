 
uniform sampler2D colors;
// uniform sampler2D normals;

in vec2 uv;

out vec4 fragcolor;

void main() {
    fragcolor = vec4(texture(colors, uv).rgb /* + texture(normals, uv).rgb * 0.1 */, 1);
}
