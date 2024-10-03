#pragma once

#ifndef LIGHT_H
#define LIGHT_H

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

struct PointLight
{
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float constant;
    float linear;
    float quadratic;

    void bind(unsigned int shader, const char *var);
};

typedef PointLight PLight;

struct DirectionalLight
{
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    void bind(unsigned int shader, const char *var);
};

typedef DirectionalLight DLight;

struct Spotlight
{
    using vec3 = glm::vec3;

    vec3 position;
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float phi_deg; // Inner cone cutoff angle
    float gamma_deg; // Outer cone cutoff angle
    float constant;
    float linear;
    float quadratic;

    void bind(unsigned int shader, const char *var);
};

typedef Spotlight SLight;

struct PointLightContext
{
    unsigned int vao, vbo, ebo, shader;

    void init();
    void release();
    void draw(const glm::mat4& mvp, glm::vec3 p, glm::vec3 c);
};

struct Shadowmap
{
    unsigned int fbo, depth; 
    int width, height;
    int viewport[4];

    void init(int w, int h);
    void release();
    void enable();
    void disable();
};

struct Material
{
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;
};

#endif