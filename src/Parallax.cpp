#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/glew.h>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Draw.h"
#include "Util.h"
#include "Octree.h"
#include "Shader.h"

void ParallaxDrawer::destroy()
{
    glDeleteBuffers(1, &ssboTree);
    glDeleteBuffers(1, &ssboTwig);
    glDeleteProgram(shader);
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
}

void ParallaxDrawer::loadTree(const Ocroot *root)
{
    mesh.cubeface(root->position, glm::vec3(1., 1., 1.) * root->size);
    this->root = root;
}

void ParallaxDrawer::loadGL(const char *texture)
{
    vao = shader = tex = 0;

    // VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Shader
    this->shader = Shader(glCreateProgram())
        .vertex("shaders/Parallax.Vertex.glsl")
        .fragment("shaders/Parallax.Fragment.glsl")
        .link();

    glUseProgram(shader);

    this->mvp = glGetUniformLocation(shader, "mvp");
    assert(mvp != -1);

    this->eye = glGetUniformLocation(shader, "eye");
    assert(eye != -1);

    this->sampler = glGetUniformLocation(shader, "sampler");
    // assert(sampler != -1);
    if (sampler != -1) glUniform1i(sampler, 0);

    pos = glGetUniformLocation(shader, "root.pos");
    assert(pos != -1);
    size = glGetUniformLocation(shader, "root.size");
    assert(size != -1);

    glUseProgram(0);

    // Vertices
    mesh.bind();

    glGenBuffers(1, &ssboTree);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTree);
    glBufferData(GL_SHADER_STORAGE_BUFFER, root->trees * sizeof(Octree), root->tree, GL_STATIC_DRAW); 
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboTree);

    glGenBuffers(1, &ssboTwig);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTwig);
    glBufferData(GL_SHADER_STORAGE_BUFFER, root->twigs * sizeof(Octwig), root->twig, GL_STATIC_DRAW); 
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssboTwig);

    // Texture
    glGenTextures(1, &this->tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->tex);

    SDL_Surface *vt = IMG_Load(texture);
    if (!vt) die("IMG_Load: %s", IMG_GetError());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vt->w, vt->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, vt->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    SDL_FreeSurface(vt);
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

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboTree);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssboTwig);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glDrawElements(GL_TRIANGLES, (unsigned int)mesh.indices.size(), GL_UNSIGNED_INT, (void *)0);
}

void ParallaxDrawer::post()
{
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
