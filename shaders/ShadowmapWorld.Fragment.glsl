
uniform vec3 direction;
in vec3 hitpoint;

void main()
{
    vec3 alpha = eye;
    vec3 beta = direction; // normalize(hitpoint - eye);
    vec3 gamma = 1.0 / beta;

    float sigma; Leaf _hit; int _steps;
    if (rootmarch(alpha, beta, gamma, sigma, _hit, _steps))
    {
        vec3 point = alpha + beta * (sigma - EPS);

        float inv_z = 1.0 / distance(point, eye);
        float inv_near = 1.0 / NEAR;
        float inv_far = 1.0 / FAR;
        float depth = (inv_z - inv_near) / (inv_far - inv_near);
        gl_FragDepth = depth;
    }
    else
        discard;
}