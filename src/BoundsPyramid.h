#pragma once

#ifndef BOUNDPYRAMID_H
#define BOUNDPYRAMID_H

struct BoundsPyramid
{
    static constexpr int MIN = 0, MAX = 1;

    float ***bounds;
    float *base;
    int size;
    int levels;

    void computeBase(float ampl, float period, float xshift, float yshift, float zshift);
    void computeBounds(int lv, int x0, int y0, int x1, int y1, float *lo, float *hi);
    float bound(float xf, float yf, int b, int lv) const;

    BoundsPyramid(const BoundsPyramid&) = delete;
    BoundsPyramid operator=(const BoundsPyramid&) = delete;

    BoundsPyramid(int size, float ampl, float period, float xshift, float yshift, float zshift);
    ~BoundsPyramid();

    float min(float xf, float yf, int lv) const;
    float max(float xf, float yf, int lv) const;
};

#endif