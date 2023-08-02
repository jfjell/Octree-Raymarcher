#include <stddef.h>
#include <assert.h>
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "Atlas.h"
#include "Util.h"

void TextureAtlas::init()
{
    const char *path = "textures/atlas.png";
    SDL_Surface *ts = IMG_Load(path);
    if (!ts) die("IMG_Load(\"%s\"): %s\n", path, IMG_GetError());
    assert(ts->w == 2048);
    assert(ts->h == 2048);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ts->w, ts->h, 0, GL_RGB, GL_UNSIGNED_BYTE, ts->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    SDL_FreeSurface(ts);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void TextureAtlas::release()
{
    glDeleteTextures(1, &tex);
}