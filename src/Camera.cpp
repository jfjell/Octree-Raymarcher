#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Camera.h"

using glm::vec3;
using glm::mat4;
using glm::quat;
using glm::angleAxis;

static inline float reduce_angle_deg(float theta)
{
    float phi = fmod(theta, 360);
    phi = fmod(phi + 360, 360);
    if (phi > 180) phi -= 360;
    return phi;
}

mat4 PerspectiveCamera::view()
{
    yaw_deg = reduce_angle_deg(yaw_deg);
    pitch_deg = glm::clamp(pitch_deg, -90.0f, 90.0f);
    roll_deg = reduce_angle_deg(roll_deg);
    
    vec3 x(1, 0, 0), y(0, 1, 0), z(0, 0, 1);

    vec3 euler(pitch_deg, yaw_deg, roll_deg);
    quat q(glm::radians(euler));

    direction = q * z;
    right = q * -x;
    up = q * y;
    return glm::lookAt(position, position + direction, up);
}

mat4 PerspectiveCamera::proj()
{
    float aspect = (float)width / height;
    float half_fov_deg = fov_deg * 0.5;
    return glm::perspective(half_fov_deg, aspect, near, far);
}

mat4 OrthoCamera::view()
{
    return glm::lookAt(normalize(-direction), vec3(0), vec3(0, 1, 0));
}

mat4 OrthoCamera::proj()
{
    float w = (float)width * 0.5;
    float h = (float)height * 0.5;
    return glm::ortho(-w, w, -h, h, near, far);
}
