#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/glew.h>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Draw.h"
#include "Util.h"
#include "Octree.h"
#include "Shader.h"

const float CUBE_VERTICES[8*3] = {
    0., 0., 0., 
    1., 0., 0., 
    0., 0., 1., 
    1., 0., 1., 
    0., 1., 0., 
    1., 1., 0., 
    0., 1., 1., 
    1., 1., 1., 
};

const unsigned short CUBE_INDICES[6*6] = {
    4, 0, 5,
    0, 1, 5,
    6, 2, 4,
    2, 0, 4,
    7, 3, 6,
    3, 2, 6,
    5, 1, 7,
    1, 3, 7,
    0, 2, 1,
    2, 3, 1,
    5, 7, 4,
    7, 6, 4,
};

void ParallaxDrawer::destroy()
{
    glDeleteBuffers(1, &ssboTree);
    glDeleteBuffers(1, &ssboTwig);
    glDeleteBuffers(1, &this->vbo);
    glDeleteBuffers(1, &this->ebo);
    glDeleteProgram(shader);
    glDeleteVertexArrays(1, &vao);
}

void ParallaxDrawer::loadTree(const Ocroot *root)
{
    using glm::mat4;
    using glm::vec3;

    this->root = root;
    mat4 T = glm::translate(mat4(1.0), root->position);
    const mat4 S = glm::scale(mat4(1.0), vec3(root->size));
    const mat4 R = mat4(1.0);
    this->srt = T * R * S;
}

void ParallaxDrawer::loadGL(const char *texture)
{
    (void)texture;

    vao = shader = vbo = ebo = 0;

    // VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Shader
    this->shader = Shader(glCreateProgram())
        .vertex("shaders/Parallax.Vertex.glsl")
        .fragment("shaders/Parallax.Fragment.glsl")
        .link();

    glUseProgram(shader);

    this->model = glGetUniformLocation(shader, "model");
    assert(model != -1);

    this->mvp = glGetUniformLocation(shader, "mvp");
    assert(mvp != -1);

    this->eye = glGetUniformLocation(shader, "eye");
    assert(eye != -1);

    pos = glGetUniformLocation(shader, "root.pos");
    assert(pos != -1);
    size = glGetUniformLocation(shader, "root.size");
    assert(size != -1);

    glUseProgram(0);

    // Vertices
    glGenBuffers(1, &this->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE_VERTICES), CUBE_VERTICES, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &this->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(CUBE_INDICES), CUBE_INDICES, GL_STATIC_DRAW);

    glGenBuffers(1, &ssboTree);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTree);
    glBufferData(GL_SHADER_STORAGE_BUFFER, root->trees * sizeof(Octree), root->tree, GL_STATIC_DRAW); 
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboTree);

    glGenBuffers(1, &ssboTwig);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTwig);
    glBufferData(GL_SHADER_STORAGE_BUFFER, root->twigs * sizeof(Octwig), root->twig, GL_STATIC_DRAW); 
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssboTwig);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ParallaxDrawer::pre(const glm::mat4 MVP, glm::vec3 position)
{
    glUseProgram(shader);

    glUniformMatrix4fv(mvp, 1, GL_FALSE, glm::value_ptr(MVP));
    glUniform3fv(eye, 1, glm::value_ptr(position));
}

void ParallaxDrawer::draw()
{
    glBindVertexArray(vao);

    glUniform3fv(pos, 1, glm::value_ptr(root->position));
    glUniform1f(size, root->size);
    glUniformMatrix4fv(model, 1, GL_FALSE, glm::value_ptr(this->srt));

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboTree);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssboTwig);

    glDrawElements(GL_TRIANGLES, sizeof(CUBE_INDICES) / sizeof(short), GL_UNSIGNED_SHORT, (void *)0);
}

void ParallaxDrawer::post()
{
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
