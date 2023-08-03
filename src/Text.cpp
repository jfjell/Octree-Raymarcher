#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <GL/glew.h>
#include "Text.h"
#include "Shader.h"

extern const float QUAD_VERTICES[8];
extern const unsigned short QUAD_INDICES[6];

const float QUAD_VERTICES[8] = {
    -1.0, +1.0, // SW
    +1.0, +1.0, // SE
    +1.0, -1.0, // NE
    -1.0, -1.0, // NW
};

const unsigned short QUAD_INDICES[6] = {
    0, 1, 2, // /.
    2, 3, 0, // ./
};

void Text::init(const char *fontpath, int size, int width, int height)
{
    this->fontpath = fontpath;
    this->size = size;
    this->width = width;
    this->height = height;
    this->space = 2;
    this->dx = space;
    this->dy = space;
    this->redraw = true;

    this->font = TTF_OpenFont(fontpath, size);
    assert(this->font != NULL);

    uint32_t alpha = 0xffu << 24, red = 0xffu << 16, green = 0xffu << 8, blue = 0xffu;
    this->surface = SDL_CreateRGBSurface(0, width, height, 32, alpha, red, green, blue);

    this->linecount = 32;
    this->line = new char[this->linecount];

    this->shader = Shader(glCreateProgram())
        .vertex("shaders/Text.Vertex.glsl")
        .fragment("shaders/Text.Fragment.glsl")
        .link();

    glUseProgram(shader);
    this->sampler = glGetUniformLocation(this->shader, "tex");
    glUniform1i(this->sampler, 0);
    glUseProgram(0);

    glGenVertexArrays(1, &this->vao);
    glBindVertexArray(this->vao);

    glGenBuffers(1, &this->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &this->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(QUAD_INDICES), QUAD_INDICES, GL_STATIC_DRAW);

    glGenTextures(1, &this->tex);
    glBindTexture(GL_TEXTURE_2D, this->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Text::deinit()
{
    delete[] this->line;

    glDeleteVertexArrays(1, &this->vao);
    glDeleteBuffers(1, &this->vbo);
    glDeleteBuffers(1, &this->ebo);
    glDeleteTextures(1, &this->tex);
    glDeleteProgram(this->shader);

    TTF_CloseFont(this->font);
    SDL_FreeSurface(this->surface);
}

void Text::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    int count = vsnprintf(NULL, 0, format, args);
    if (count+1 >= this->linecount)
    {
        this->linecount = count+1;
        delete[] this->line;
        line = new char[this->linecount];
    }
    vsnprintf(this->line, count+1, format, args);

    SDL_Color w = { 255, 255, 255, 255 };
    SDL_Surface *s = TTF_RenderUTF8_Blended(this->font, this->line, w);
    assert(s != NULL);

    SDL_Rect coords = { this->dx, this->dy, s->w, s->h };
    SDL_BlitSurface(s, NULL, this->surface, &coords);
    this->dy += s->h + space;

    this->redraw = true;
    SDL_FreeSurface(s);
}

void Text::clear()
{
    SDL_FillRect(this->surface, NULL, 0x00000000);
    this->dy = space;
    this->redraw = true;
}

void Text::draw()
{
    glUseProgram(this->shader);
    glBindVertexArray(this->vao);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->tex);
    if (this->redraw) 
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 
            0, 0, surface->w, surface->h, 
            GL_RGBA, GL_UNSIGNED_BYTE, 
            surface->pixels);
    }

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (void *)0);

    glUseProgram(0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}