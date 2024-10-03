
uniform sampler2D Diffuse, Specular;
uniform sampler2D ShadowDepthMap;

vec2 leafUV(vec3 p, Leaf leaf)
{
    vec3 leafmin = leaf.bmin;
    vec3 leafmax = leafmin + leaf.size;
    uint m = leaf.offset;
    vec2 iuv = cubeUV(p, leafmin, leafmax);
    iuv += (vec2(lessThan(iuv, vec2(0.125))) - vec2(greaterThan(iuv, vec2(0.125)))) * EPS * 2;
    uint x = m & 0xff, y = (m >> 8) & 0xff;
    vec2 uv = (vec2(x, y) + iuv) / 256.0;
    return uv;
}

struct PointLight
{
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

struct DirectionalLight
{
    vec3 position;
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct Spotlight
{
    vec3 position;
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float cos_phi;
    float cos_gamma;
    float constant;
    float linear;
    float quadratic;
};

uniform PointLight pointLight;
uniform DirectionalLight directionalLight;
uniform Spotlight spotlight;

struct Material
{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

Material ML[8] =
{
    Material(vec3(0), vec3(0), vec3(0), 0), // void
    Material(vec3(0.8), vec3(0.8), vec3(0.5), 8), // stone
    Material(vec3(0.8), vec3(0.6), vec3(0.1), 16), // dirt
    Material(vec3(0.8), vec3(0.7), vec3(0.15), 32), // sand
    Material(vec3(0.8), vec3(0.9), vec3(0.7), 10000), // grass
    Material(vec3(0.8), vec3(0.5), vec3(0), 0), // shroom
    Material(vec3(0.8), vec3(0.4), vec3(1.0), 100), // water
    Material(vec3(0), vec3(0), vec3(0), 0), // void
};

float attenuation(float Kc, float Kl, float Kq, float d)
{
    return 1.0 / (Kc + Kl * d + Kq * d * d);
}

vec3 computePointLight_BlinnPhong(PointLight light, vec3 n, vec3 p, vec3 e, vec3 diffuse, vec3 specular, float shininess, float shadow)
{
    vec3 l = normalize(light.position - p);
    vec3 v = normalize(e - p);
    // vec3 r = reflect(-l, n);
    vec3 h = normalize(l + v);

    float d = max(dot(n, l), 0.0);
    float s = pow(max(dot(v, h), 0.0), shininess);

    float dist = length(p - light.position);
    float att = attenuation(light.constant, light.linear, light.quadratic, dist);

    vec3 amb = light.ambient * diffuse;
    vec3 diff = light.diffuse * d * diffuse * (1.0 - shadow);
    vec3 spec = light.specular * s * specular * (1.0 - shadow);
    return (amb + diff + spec) * att;
}

vec3 computeDirectionalLight_BlinnPhong(DirectionalLight light, vec3 n, vec3 p, vec3 e, vec3 diffuse, vec3 specular, float shininess, float shadow)
{
    vec3 l = normalize(-light.direction);
    vec3 v = normalize(e - p);
    // vec3 r = reflect(-l, n);
    vec3 h = normalize(l + v);

    float d = max(dot(n, l), 0.0);
    float s = pow(max(dot(v, h), 0.0), shininess);

    vec3 amb = light.ambient * diffuse;
    vec3 diff = light.diffuse * d * diffuse * (1.0 - shadow);
    vec3 spec = light.specular * s * specular * (1.0 - shadow);

    return amb + diff + spec;
}

vec3 computeSpotlight_BlinnPhong(Spotlight light, vec3 n, vec3 p, vec3 e, vec3 diffuse, vec3 specular, float shininess, float shadow)
{
    vec3 l = normalize(light.position - p);
    vec3 v = normalize(e - p);
    // vec3 r = reflect(-l, n);
    vec3 h = normalize(l + v);

    float d = max(dot(n, l), 0.0);
    float s = pow(max(dot(v, h), 0.0), shininess);

    float dist = length(p - light.position);
    float att = attenuation(light.constant, light.linear, light.quadratic, dist);

    float theta = dot(l, normalize(-light.direction));
    float delta = spotlight.cos_phi - spotlight.cos_gamma;
    float intensity = clamp((theta - spotlight.cos_gamma) / delta, 0.0, 1.0);

    vec3 amb = light.ambient * diffuse;
    vec3 diff = light.diffuse * d * diffuse * (1.0 - shadow);
    vec3 spec = light.specular * s * specular * (1.0 - shadow);

    return (amb + (diff + spec) * intensity) * att;
}

uniform mat4 shadowVP;

float computeShadow(vec3 point)
{
    vec4 lightspace = shadowVP * vec4(point, 1);
    vec3 proj = lightspace.xyz / lightspace.w;
    proj = proj * 0.5 + 0.5;

    float pixelDepth = texture(ShadowDepthMap, proj.xy).r;

    float inv_near = 1.0 / NEAR;
    float inv_far = 1.0 / FAR;
    float pointDepth = ((1.0 / distance(point, directionalLight.position)) - inv_near) / (inv_far - inv_near);

    return pointDepth > pixelDepth ? 1.0 : 0.0;
}

in vec3 hitpoint;
in vec4 shadowHitpoint;

out vec4 Color;

void main()
{
    vec3 alpha = eye;
    vec3 beta = normalize(hitpoint - eye);
    vec3 gamma = 1.0 / beta;
    
    float sigma; Leaf hit; int _steps;
    if (rootmarch(alpha, beta, gamma, sigma, hit, _steps))
    {
        vec3 leafmin = hit.bmin;
        vec3 leafmax = leafmin + hit.size;

        vec3 point = alpha + beta * (sigma - EPS);
        vec3 normal = cubeNormal(point, leafmin, leafmax);

        uint b = hit.offset;
        vec2 uv = leafUV(point, hit);

        float mgamma = 2.2;
        vec3 diffuse = pow(texture(Diffuse, uv).xyz, vec3(mgamma));
        vec3 specular = pow(texture(Specular, uv).xyz, vec3(mgamma));

        Material material = ML[b];

        float shadow = computeShadow(point);
        Color.rgb = vec3(0);
        Color.rgb += computePointLight_BlinnPhong(pointLight, normal, point, eye, diffuse, specular, material.shininess, shadow);
        Color.rgb += computeDirectionalLight_BlinnPhong(directionalLight, normal, point, eye, diffuse, specular, material.shininess, shadow);
        Color.rgb += computeSpotlight_BlinnPhong(spotlight, normal, point, eye, diffuse, specular, material.shininess, shadow);
        Color.a = 1;

        float inv_z = 1.0 / distance(point, eye);
        float inv_near = 1.0 / NEAR;
        float inv_far = 1.0 / FAR;
        float depth = (inv_z - inv_near) / (inv_far - inv_near);
        gl_FragDepth = depth;
    }
    else
    {
        discard;
    }
}