#pragma once

#ifndef ATLAS_H
#define ATLAS_H

struct TextureAtlas
{
    unsigned int tex;

    void init();
    void release();
};

#endif