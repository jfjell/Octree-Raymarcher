#include <assert.h>
#include <string.h>
#include <string>
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include "Light.h"
#include "Shader.h"

using std::string;
using glm::value_ptr;

void PointLight::bind(unsigned int shader, const char *var)
{
    string varname = var;

    int position_ul = glGetUniformLocation(shader, (varname + ".position").c_str());
    int ambient_ul = glGetUniformLocation(shader, (varname + ".ambient").c_str());
    int diffuse_ul = glGetUniformLocation(shader, (varname + ".diffuse").c_str());
    int specular_ul = glGetUniformLocation(shader, (varname + ".specular").c_str());
    int constant_ul = glGetUniformLocation(shader, (varname + ".constant").c_str());
    int linear_ul = glGetUniformLocation(shader, (varname + ".linear").c_str());
    int quadratic_ul = glGetUniformLocation(shader, (varname + ".quadratic").c_str());

    /*
    assert(position_ul != -1);
    assert(ambient_ul != -1);
    assert(diffuse_ul != -1);
    assert(specular_ul != -1);
    assert(constant_ul != -1);
    assert(linear_ul != -1);
    assert(quadratic_ul != -1);
    */

    glUniform3fv(ambient_ul, 1, glm::value_ptr(ambient));
    glUniform3fv(position_ul, 1, glm::value_ptr(position));
    glUniform3fv(diffuse_ul, 1, glm::value_ptr(diffuse));
    glUniform3fv(specular_ul, 1, glm::value_ptr(specular));
    glUniform1f(constant_ul, constant);
    glUniform1f(linear_ul, linear);
    glUniform1f(quadratic_ul, quadratic);
}

void DirectionalLight::bind(unsigned int shader, const char *var)
{
    string varname = var;

    int direction_ul = glGetUniformLocation(shader, (varname + ".direction").c_str());
    int ambient_ul = glGetUniformLocation(shader, (varname + ".ambient").c_str());
    int diffuse_ul = glGetUniformLocation(shader, (varname + ".diffuse").c_str());
    int specular_ul = glGetUniformLocation(shader, (varname + ".specular").c_str());

    /*
    assert(direction_ul != -1);
    assert(ambient_ul != -1);
    assert(diffuse_ul != -1);
    assert(specular_ul != -1);
    */

    glUniform3fv(direction_ul, 1, glm::value_ptr(direction));
    glUniform3fv(ambient_ul, 1, glm::value_ptr(ambient));
    glUniform3fv(diffuse_ul, 1, glm::value_ptr(diffuse));
    glUniform3fv(specular_ul, 1, glm::value_ptr(specular));
}

void Spotlight::bind(unsigned int shader, const char *var)
{
    string varname = var;

    int position_ul = glGetUniformLocation(shader, (varname + ".position").c_str());
    int direction_ul = glGetUniformLocation(shader, (varname + ".direction").c_str());
    int ambient_ul = glGetUniformLocation(shader, (varname + ".ambient").c_str());
    int diffuse_ul = glGetUniformLocation(shader, (varname + ".diffuse").c_str());
    int specular_ul = glGetUniformLocation(shader, (varname + ".specular").c_str());
    int cos_phi_ul = glGetUniformLocation(shader, (varname + ".cos_phi").c_str());
    int cos_gamma_ul = glGetUniformLocation(shader, (varname + ".cos_gamma").c_str());
    int constant_ul = glGetUniformLocation(shader, (varname + ".constant").c_str());
    int linear_ul = glGetUniformLocation(shader, (varname + ".linear").c_str());
    int quadratic_ul = glGetUniformLocation(shader, (varname + ".quadratic").c_str());

    /*
    assert(position_ul != -1);
    assert(direction_ul != -1);
    assert(ambient_ul != -1);
    assert(diffuse_ul != -1);
    assert(specular_ul != -1);
    assert(cos_phi_ul != -1);
    assert(cos_gamma_ul != -1);
    assert(constant_ul != -1);
    assert(linear_ul != -1);
    assert(quadratic_ul != -1);
    */

    glUniform3fv(position_ul, 1, glm::value_ptr(position));
    glUniform3fv(direction_ul, 1, glm::value_ptr(direction));
    glUniform3fv(ambient_ul, 1, glm::value_ptr(ambient));
    glUniform3fv(diffuse_ul, 1, glm::value_ptr(diffuse));
    glUniform3fv(specular_ul, 1, glm::value_ptr(specular));
    glUniform1f(cos_phi_ul, glm::cos(glm::radians(phi_deg)));
    glUniform1f(cos_gamma_ul, glm::cos(glm::radians(gamma_deg)));
    glUniform1f(constant_ul, constant);
    glUniform1f(linear_ul, linear);
    glUniform1f(quadratic_ul, quadratic);
}

extern const float CUBE_VERTICES[8*3];
extern const unsigned short CUBE_INDICES[6*6];

void PointLightContext::init()
{
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE_VERTICES), CUBE_VERTICES, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(CUBE_INDICES), CUBE_INDICES, GL_STATIC_DRAW);

    shader = Shader(glCreateProgram())
        .vertex("shaders/Light.Vertex.glsl")
        .fragment("shaders/Light.Fragment.glsl")
        .link();

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void PointLightContext::release()
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(shader);
}

void PointLightContext::draw(const glm::mat4& mvp, glm::vec3 p, glm::vec3 c)
{
    glBindVertexArray(vao);
    glUseProgram(shader);

    glm::mat4 model = glm::translate(glm::mat4(1), p);
    glUniform3fv(glGetUniformLocation(shader, "color"), 1, value_ptr(c));
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shader, "mvp"), 1, GL_FALSE, value_ptr(mvp));

    glDrawElements(GL_TRIANGLES, sizeof(CUBE_INDICES) / sizeof(unsigned short), GL_UNSIGNED_SHORT, (void *)0);

    glBindVertexArray(0);
    glUseProgram(0);
}

void Shadowmap::init(int w, int h)
{
    width = w;
    height = h;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &depth);
    glBindTexture(GL_TEXTURE_2D, depth);
    glTexImage2D(GL_TEXTURE_2D, 
        0, 
        GL_DEPTH_COMPONENT, 
        width, 
        height, 
        0, 
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glFramebufferTexture2D(GL_FRAMEBUFFER, 
        GL_DEPTH_ATTACHMENT, 
        GL_TEXTURE_2D, 
        depth, 
        0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Shadowmap::release()
{
    glDeleteTextures(1, &depth);
    glDeleteFramebuffers(1, &fbo);
}

void Shadowmap::enable()
{
    glGetIntegerv(GL_VIEWPORT, viewport);
    glViewport(0, 0, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void Shadowmap::disable()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}
