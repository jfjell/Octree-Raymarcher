#pragma once

#include <GL/glew.h>

struct Shader
{
    Shader();
    ~Shader();
    void compile(const char *path, GLenum type);
    unsigned link();

    unsigned program;
};
