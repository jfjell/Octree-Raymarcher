#pragma once

#ifndef IMAGINARYCUBE_H
#define IMAGINARYCUBE_H

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

struct ImaginaryCube
{
    unsigned int vao, vbo, ebo, shader;
    int model, mvp;
    glm::vec3 bmin, scale;
    bool real;

    void init(glm::vec3 scale);
    void deinit();
    void position(glm::vec3 bmid);
    void draw(glm::mat4 mvp);
};

typedef ImaginaryCube ImagCube;

#endif