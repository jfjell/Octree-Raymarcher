#version 330 core
 
uniform sampler2D tex;

in vec2 uv;

void main(void) {
    gl_FragColor = texture(tex, uv).rgba;
}
