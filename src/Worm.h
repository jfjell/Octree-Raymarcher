#pragma once

#ifndef WORM_H
#define WORM_H

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include "Draw.h"

struct Worm
{
    glm::vec3 pos;
    unsigned int vao, shader, tex;
    int model, mvp, t, sampler;
    Mesh mesh;

    void init();
    void draw(glm::mat4 mvp);
};

#endif