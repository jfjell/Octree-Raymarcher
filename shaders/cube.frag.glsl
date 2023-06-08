#version 330 core
 
uniform sampler2D samp;

in vec2 uv;

out vec4 color;
 
float eps = 0.005;

void main(void) {
    if (uv.x >= 1.0 - eps || uv.x <= eps || uv.y >= 1.0 - eps|| uv.y <= eps || abs(uv.x - uv.y) <= eps)
        color = vec4(0, 0, 0, 1);
    else
        color = texture(samp, uv).rgba;
}
