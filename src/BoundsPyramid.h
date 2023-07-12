#pragma once

#ifndef BOUNDSPYRAMID_H
#define BOUNDSPYRAMID_H

struct BoundsPyramid
{
    // using half = __fp16;
    using half = float;

    half *basequad, **minquad, **maxquad;
    size_t size, levels;
    float amplitude, shift;

    void init(size_t size, float ampl, float period, float xshift, float yshift, float zshift);
    void deinit();
    void computeBase(float period, float xshift, float zshift);
    void computeBoundsAbove(size_t lv);
    float min(float x, float z, size_t lv) const;
    float max(float x, float z, size_t lv) const;
    float bound(float x, float z, size_t lv, const half *q) const;
};


#endif