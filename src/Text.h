#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

struct Text
{
    TTF_Font *font;
    SDL_Surface *surface;
    char *line;
    const char *fontpath;
    unsigned int vao, vbo, ebo, tex, shader, sampler;
    int space, size, width, height, dx, dy, linecount;
    bool redraw;

    void init(const char *font, int size, int width, int height);
    void deinit();
    void printf(const char *format, ...);
    void draw();
    void clear();
};
