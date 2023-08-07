#pragma once

#ifndef ATLAS_H
#define ATLAS_H

struct TextureAtlas
{
    unsigned int diffuse, specular;

    void init();
    void release();
};

#endif