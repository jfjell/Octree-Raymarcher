
layout (location = 0) in vec3 hitpos;

layout (depth_greater) out float gl_FragDepth;

void main() {
    Tree tree;
    vec3 beta = normalize(hitpos - eye);
    vec3 gamma = 1.0 / beta;

    // Fix the cube going invisible in case we hit the far vertex (if we happen to be very close to the edge)
    vec3 alpha = eye;
    float t1 = cubeEscapeDistance(hitpos - beta * EPS, -gamma, root.pos, root.pos + root.size);
    float t2 = cubeEscapeDistance(hitpos + beta * EPS, +gamma, root.pos, root.pos + root.size);
    vec3 a1 = hitpos - beta * t1;
    vec3 a2 = hitpos + beta * t2;
    alpha = (distance(alpha, a1) < distance(alpha, a2)) ? a1 : a2;

    float sigma = treemarch(alpha, beta, gamma, tree);

    if (sigma < MAX_DIST) {
        vec3 point = alpha + beta * (sigma + EPS);
        // vec3 relative = (point - root.pos) / root.size;
        // vec3 texturecolor = texture(sampler, relative.xz).rgb;
        vec3 normal = cubeNormal(point, tree.pos, tree.pos + tree.size);
        // vec4 color = vec4(texturecolor + normal * 0.5, 1);
        uint material = tree.index - 1;
        vec4 color = vec4(materialLookup[material].xyz + normal * 0.1, 1);
        gl_FragColor = color;

        float z = 1.0 / distance(point, eye);
        float near = 1.0 / 0.1;
        float far = 1.0 / 10000.0;
        float depth = (z - near) / (far - near);
        gl_FragDepth = depth;
    } else {
        gl_FragDepth = 1.0;
        discard;
    }
}