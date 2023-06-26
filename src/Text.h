#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vector>

struct Text
{
    TTF_Font *font;
    SDL_Surface *surface;
    std::vector<char> buf;
    unsigned shader;
    const char *path;
    int space, size;
    int width, height;
    int xpos, ypos;
    bool redraw;
    unsigned vao, vbo[3] /* vbo, uv, ebo */, tex;

    Text(const char *font, int size, int width, int height);
    ~Text();
    void printf(const char *format, ...);
    void draw();
    void clear();
};
