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
    this->shader = glCreateProgram();
    Shader::compileAttach("shaders/Parallax.Vertex.glsl", GL_VERTEX_SHADER, shader);
    Shader::compileAttach("shaders/Parallax.Fragment.glsl", GL_FRAGMENT_SHADER, shader);
    Shader::link(shader);

    glUseProgram(shader);

    this->mvp = glGetUniformLocation(shader, "mvp");
    assert(mvp != -1);

    this->eye = glGetUniformLocation(shader, "eye");
    assert(eye != -1);

    this->sampler = glGetUniformLocation(shader, "sampler");
    // assert(sampler != -1);
    if (sampler != -1) glUniform1i(sampler, 0);

    int pos   = glGetUniformLocation(shader, "root.pos");
    int size  = glGetUniformLocation(shader, "root.size");
    int depth = glGetUniformLocation(shader, "root.depth");
    int trees = glGetUniformLocation(shader, "root.trees");
    int twigs = glGetUniformLocation(shader, "root.twigs");
    if (pos != -1) glUniform3fv(pos, 1, glm::value_ptr(root->position));
    if (size != -1) glUniform1f(size, root->size);
    if (depth != -1) glUniform1ui(depth, root->depth);
    if (trees != -1) glUniform1ui(trees, root->trees);
    if (twigs != -1) glUniform1ui(twigs, root->twigs);

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

    glGenBuffers(1, &ssboBark);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboBark);
    glBufferData(GL_SHADER_STORAGE_BUFFER, root->barks * sizeof(Ocbark), root->bark, GL_STATIC_DRAW); 
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssboBark);

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
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    SDL_FreeSurface(vt);
}

void ParallaxDrawer::draw(const glm::mat4 MVP, glm::vec3 position)
{
    glBindVertexArray(vao);
    glUseProgram(shader);

    glUniformMatrix4fv(mvp, 1, GL_FALSE, glm::value_ptr(MVP));
    glUniform3fv(eye, 1, glm::value_ptr(position));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, (void *)0);

    glUseProgram(0);
    glBindVertexArray(0);
}
