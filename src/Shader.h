#pragma once

#ifndef SHADER_H
#define SHADER_H

struct Shader
{
    unsigned int program = 0;

    Shader(unsigned int p) : program(p) {}

    Shader compile(const char *path, unsigned int type);
    Shader vertex(const char *path);
    Shader fragment(const char *path);
    unsigned int link();

    static void registerInclude(const char *path);
};

#endif