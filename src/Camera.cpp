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

mat4 PerspectiveCamera::view()
{
    yaw_deg = fmod(yaw_deg, 360.0);
    pitch_deg = glm::clamp(pitch_deg, -90.0f, 90.0f);
    roll_deg = fmod(roll_deg, 360.0);
    
    vec3 x(1, 0, 0), y(0, 1, 0), z(0, 0, 1);
    // quat q = angleAxis(pitch_deg, x) * angleAxis(yaw_deg, y) * angleAxis(roll_deg, z);

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
    return glm::lookAt(position, vec3(0), vec3(0, 1, 0));
}

mat4 OrthoCamera::proj()
{
    return glm::ortho(0.f, (float)width, 0.f, (float)height, near, far);
}
