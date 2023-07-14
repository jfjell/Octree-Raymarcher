#pragma once

#ifndef SHADER_H
#define SHADER_H

#include <vector>
#include <string>

struct Shader
{
    unsigned int program = 0;
    std::vector<std::string> includes = {};

    Shader(unsigned int p) : program(p) {}

    Shader& include(const char *path);
    Shader& compile(const char *path, unsigned int type);
    Shader& vertex(const char *path);
    Shader& fragment(const char *path);
    unsigned int link();
};

#endif