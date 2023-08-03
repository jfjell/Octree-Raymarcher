#pragma once

#ifndef SKYBOX_H
#define SKYBOX_H

#include <glm/mat4x4.hpp>

struct Skybox
{
    unsigned int vao, vbo, ebo, tex, shader;
    int tex_ul, vp_ul;

    void init();
    void release();
    void draw(glm::mat4 view, glm::mat4 proj);
};

#endif