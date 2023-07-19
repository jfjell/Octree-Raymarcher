#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <glm/gtc/type_ptr.hpp>
#include "Worm.h"
#include "Shader.h"

using glm::vec3;
using glm::mat4;

#define SEGMENTS 40

void Worm::init()
{
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    for (int i = 0; i < SEGMENTS; ++i)
        mesh.cubemap(vec3((float)i * 1.05f, 0, 0), vec3(1));
    mesh.bind();

    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    SDL_Surface *vt = IMG_Load("textures/quad.png");
    for (unsigned i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, vt->w, vt->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, vt->pixels);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    shader = Shader(glCreateProgram())
        .vertex("shaders/Worm.Vertex.glsl")
        .fragment("shaders/Worm.Fragment.glsl")
        .link();

    model = glGetUniformLocation(shader, "model");
    mvp = glGetUniformLocation(shader, "mvp");
    t = glGetUniformLocation(shader, "t");
    sampler = glGetUniformLocation(shader, "tex");

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    pos = vec3(0, 150, 0);
}

void Worm::draw(glm::mat4x4 mvp)
{
    glBindVertexArray(vao);
    glUseProgram(shader);

    mat4 model = glm::translate(mat4(1.0), pos);

    glUniformMatrix4fv(this->mvp, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(this->model, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1f(t, (float)clock() / CLOCKS_PER_SEC);
    glUniform1i(sampler, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, (void *)0);

    glUseProgram(0);
    glBindVertexArray(0);
}
