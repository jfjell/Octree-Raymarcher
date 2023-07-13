#include <GL/glew.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "ImaginaryCube.h"
#include "Shader.h"

using glm::vec3;
using glm::mat4;

extern const float CUBE_VERTICES[8*3];
extern const unsigned short CUBE_INDICES[6*6];

void ImaginaryCube::init(float scale)
{
    this->bmin = vec3(0.0);
    this->scale = vec3(scale);
    this->real = true;

    this->vao = this->vbo = this->ebo = this->shader = 0;

    glGenVertexArrays(1, &this->vao);
    glBindVertexArray(this->vao);

    this->shader = Shader(glCreateProgram())
        .vertex("shaders/Imag.Vertex.glsl")
        .fragment("shaders/Imag.Fragment.glsl")
        .link();

    this->model = glGetUniformLocation(shader, "model");
    assert(model != -1);

    this->mvp = glGetUniformLocation(shader, "mvp");
    assert(mvp != -1);

    glGenBuffers(1, &this->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE_VERTICES), CUBE_VERTICES, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &this->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(CUBE_INDICES), CUBE_INDICES, GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ImaginaryCube::deinit()
{
    glDeleteVertexArrays(1, &this->vao);
    glDeleteBuffers(1, &this->vbo);
    glDeleteBuffers(1, &this->ebo);
    glDeleteProgram(this->shader);
}

void ImaginaryCube::position(vec3 bmid)
{
    this->bmin = bmid - (this->scale * 0.5f);
}

void ImaginaryCube::draw(mat4 mvp)
{
    const mat4 T = glm::translate(mat4(1.0), this->bmin);
    const mat4 S = glm::scale(mat4(1.0), vec3(this->scale));
    const mat4 R = mat4(1.0);
    const mat4 SRT = T * R * S;

    glBindVertexArray(this->vao);
    glUseProgram(this->shader);

    glUniformMatrix4fv(this->model, 1, GL_FALSE, glm::value_ptr(SRT));
    glUniformMatrix4fv(this->mvp, 1, GL_FALSE, glm::value_ptr(mvp));

    glDrawElements(GL_TRIANGLES, sizeof(CUBE_INDICES) / sizeof(short), GL_UNSIGNED_SHORT, (void *)0);

    glBindVertexArray(0);
    glUseProgram(0);
}