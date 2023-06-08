#include <stdio.h>
#include <stdarg.h>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <GL/glew.h>
#include "Text.h"

static const float vertices[] = {
    -1.0, -1.0,
    1.0, -1.0,
    1.0,  1.0,
    -1.0,  1.0,
};

static const float uv[] = {
    0.0, 0.0,
    1.0, 0.0,
    1.0, 1.0,
    0.0, 1.0,
};

static const unsigned indices[] = {
    3, 0, 1,
    3, 2, 1,
};

Text::Text(const char *path, int size, int width, int height)
{
    this->path = path;
    this->size = size;
    this->width = width;
    this->height = height;
    this->font = TTF_OpenFont(path, size);

    this->redraw = true;
    this->space = 2;
    this->xpos = space;
    this->ypos = space;
    this->surface = SDL_CreateRGBSurface(0, width, height, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);

    shader.compile("shaders/text.vrtx.glsl", GL_VERTEX_SHADER);
    shader.compile("shaders/text.frag.glsl", GL_FRAGMENT_SHADER);
    shader.link();

    glGenVertexArrays(1, &this->vao);
    glBindVertexArray(this->vao);

    glGenBuffers(3, this->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, this->vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uv), uv, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vbo[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glGenTextures(1, &this->tex);
    glBindTexture(GL_TEXTURE_2D, this->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Text::~Text()
{
    TTF_CloseFont(this->font);
    glDeleteVertexArrays(1, &this->vao);
    glDeleteBuffers(3, this->vbo);
    glDeleteTextures(1, &this->tex);
    SDL_FreeSurface(this->surface);
    this->font = NULL;
    this->vao = this->vbo[0] = this->vbo[1] = this->vbo[2] = this->tex = 0;
    this->surface = NULL;
}

void Text::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    int count = vsnprintf(NULL, 0, format, args);
    this->buf.reserve(count+1);
    vsnprintf(this->buf.data(), count+1, format, args);

    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Surface *surf = TTF_RenderText_Blended(this->font, this->buf.data(), white);

    SDL_Rect coords = { xpos, ypos, surf->w, surf->h };
    SDL_BlitSurface(surf, NULL, this->surface, &coords);
    this->ypos += surf->h + space;

    this->redraw = true;
    SDL_FreeSurface(surf);
}

void Text::clear()
{
    SDL_FillRect(this->surface, NULL, 0x00000000);
    this->ypos = space;
    this->redraw = true;
}

void Text::draw()
{
    // Copy texture data and draw
    glUseProgram(this->shader.program);
    glBindVertexArray(this->vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->tex);
    if (this->redraw)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    int samplerIndex = glGetUniformLocation(this->shader.program, "samp");
    glUniform1i(samplerIndex, 0);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *)0);

    glUseProgram(0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}