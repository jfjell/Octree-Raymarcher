#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/glew.h>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Draw.h"
#include "Util.h"
#include "Octree.h"

ParallaxDrawer::~ParallaxDrawer()
{
    glDeleteProgram(shader);
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
}

void ParallaxDrawer::loadTree(const Ocroot *root)
{
    using glm::vec3;
    // Load the tree into a mesh
    (void)root;
    vec3 pos = vec3(1.f, 1.f, 1.f) * 0.f;
    mesh.cubeface(pos, vec3(1, 1, 1));
}

void ParallaxDrawer::loadGL(const char *texture)
{
    vao = shader = tex = 0;

    // VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Shader
    this->shader = glCreateProgram();
    Shader::compileAttach("shaders/Parallax.Vertex.glsl", GL_VERTEX_SHADER, shader);
    Shader::compileAttach("shaders/Parallax.Fragment.glsl", GL_FRAGMENT_SHADER, shader);
    Shader::link(shader);

    this->mvp = glGetUniformLocation(shader, "mvp");
    assert(mvp != -1);
    this->sampler = glGetUniformLocation(shader, "sampler");
    assert(sampler != -1);
    this->eye = glGetUniformLocation(shader, "cameraOrigin");
    assert(eye != -1);

    // Vertices
    mesh.bind();

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

    SDL_FreeSurface(vt);
}

void ParallaxDrawer::draw(const glm::mat4 MVP, glm::vec3 position)
{
    glBindVertexArray(vao);
    glUseProgram(shader);

    glUniformMatrix4fv(mvp, 1, GL_FALSE, glm::value_ptr(MVP));
    glUniform1i(sampler, 0);
    glUniform3fv(eye, 1, glm::value_ptr(position));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, (void *)0);

    glUseProgram(0);
    glBindVertexArray(0);
}
