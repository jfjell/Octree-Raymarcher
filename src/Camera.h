#pragma once

#ifndef CAMERA_H
#define CAMERA_H

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

struct PerspectiveCamera
{
    using vec3 = glm::vec3;
    using mat4 = glm::mat4;

    vec3 position, direction, right, up;
    float near, far, fov_deg;
    int width, height;
    float yaw_deg, pitch_deg, roll_deg;

    mat4 view();
    mat4 proj();
};

struct OrthoCamera
{
    using vec3 = glm::vec3;
    using mat4 = glm::mat4;

    vec3 position, direction, up;
    int width, height;
    float near, far;

    mat4 view();
    mat4 proj();
};

#endif