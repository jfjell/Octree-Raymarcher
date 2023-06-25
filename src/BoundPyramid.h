#pragma once

#ifndef BOUNDPYRAMID_H
#define BOUNDPYRAMID_H

class BoundPyramid
{
private:
    float ***bounds;
    float *base;
    int size;
    int levels;

    void compute(float ampl, float period, float xshift, float yshift, float shift);
    float bound(int x, int y, int b, int lv);

public:
    BoundPyramid(int size, float ampl, float period, float xshift, float yshift, float shift);
    ~BoundPyramid();

    float min(int x, int y, int lv);
    float max(int x, int y, int lv);
};

#endif