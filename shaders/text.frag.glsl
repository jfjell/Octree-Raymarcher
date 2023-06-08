#version 330 core
 
uniform sampler2D samp;

in vec2 uv;

out vec4 color;
 
void main(void) {
    color = texture(samp, uv).rgba;
    if (color.a <= 0) discard;
}
