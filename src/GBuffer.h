#pragma once

#ifndef GBUFFER_H
#define GBUFFER_H

struct GBuffer
{
    unsigned int fbo = 0, rbo = 0, ds = 0;
    unsigned int vao = 0, vbo = 0, ebo = 0, shader = 0;
    unsigned int color = 0, normal = 0;
    int tc, tn;
    int width, height;

    void init(int w, int h);
    void deinit();
    void enable();
    void disable();
    void draw();
};

#endif