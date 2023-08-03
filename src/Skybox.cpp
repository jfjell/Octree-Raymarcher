#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include "Skybox.h"
#include "Util.h"
#include "Shader.h"

extern const float CUBE_VERTICES[8*3];
extern const unsigned short CUBE_INDICES[6*6];

void Skybox::init()
{
    static const char *FACES[6] =
    {
        "textures/skybox/right.jpg",
        "textures/skybox/left.jpg",
        "textures/skybox/top.jpg",
        "textures/skybox/bottom.jpg",
        "textures/skybox/front.jpg",
        "textures/skybox/back.jpg",
    };

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

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    for (int i = 0; i < 6; ++i)
    {
        SDL_Surface *fs = IMG_Load(FACES[i]);
        if (!fs) die("IMG_Load(\"%s\"): %s\n", FACES[i], IMG_GetError());
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, fs->w, fs->h, 0, GL_RGB, GL_UNSIGNED_BYTE, fs->pixels);
        SDL_FreeSurface(fs);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    shader = Shader(glCreateProgram())
        .vertex("shaders/Skybox.Vertex.glsl")
        .fragment("shaders/Skybox.Fragment.glsl")
        .link();

    tex_ul = glGetUniformLocation(shader, "skybox");
    assert(tex_ul != -1);
    vp_ul = glGetUniformLocation(shader, "vp");
    assert(vp_ul != -1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void Skybox::release()
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteTextures(1, &tex);
    glDeleteProgram(shader);
}

using glm::mat4;
using glm::mat3;
using glm::value_ptr;

void Skybox::draw(mat4 view, mat4 proj)
{
    mat4 v = mat4(mat3(view));
    mat4 vp = proj * v;

    glBindVertexArray(vao);
    glUseProgram(shader);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    glUniform1i(tex_ul, 1);
    glUniformMatrix4fv(vp_ul, 1, GL_FALSE, value_ptr(vp));

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glDrawElements(GL_TRIANGLES, sizeof(CUBE_INDICES) / sizeof(unsigned short), GL_UNSIGNED_SHORT, (void *)0);

    glBindVertexArray(0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}