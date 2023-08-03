
layout (location = 0) in vec3 vertex;

out vec3 uvw;

uniform mat4 vp;

void main()
{
    vec3 v = (vertex - 0.5) * 2.0;
    uvw = normalize(v);
    vec4 position = vp * vec4(v, 1.0);
    gl_Position = position.xyww;
}