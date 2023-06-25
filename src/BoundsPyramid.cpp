#include <assert.h>
#include <float.h>
#include <glm/vec2.hpp>
#include <glm/common.hpp>
#include <glm/gtc/noise.hpp>
#define private public
#include "BoundsPyramid.h"

static inline bool ispowof2(int i)
{
    return !(i & (i - 1));
}

static inline int log2(int i)
{
    return __builtin_ctz(i);
}

static inline int pow2(int i)
{
    return 1 << i;
}

static inline int index(int x, int y, int s)
{
    return y * s + x;
}

// Compute and store the bounds at the 4 points in the rectangle (x0, y0) -> (x1, y1) at level lv
void BoundsPyramid::computeBounds(int lv, int x0, int y0, int x1, int y1, float *lo, float *hi)
{
    using glm::min;
    using glm::max;

    int size = this->size / pow2(this->levels - lv);
    int nw = index(x0, y0, size);
    int ne = index(x1, y0, size);
    int sw = index(x0, y1, size);
    int se = index(x1, y1, size);

    if (lv == this->levels)
    {
        *lo = min(min(base[nw], base[ne]), min(base[sw], base[se]));
        *hi = max(max(base[nw], base[ne]), max(base[sw], base[se]));
    }
    else
    {
        assert(lv < this->levels);

        float *ll = bounds[lv][0];
        float *hl = bounds[lv][1];
        computeBounds(lv+1, x0*2, y0*2, x0*2+1, y0*2+1, &ll[nw], &hl[nw]);
        computeBounds(lv+1, x1*2, y0*2, x1*2+1, y0*2+1, &ll[ne], &hl[ne]);
        computeBounds(lv+1, x0*2, y1*2, x0*2+1, y1*2+1, &ll[sw], &hl[sw]);
        computeBounds(lv+1, x1*2, y1*2, x1*2+1, y1*2+1, &ll[se], &hl[se]);

        if (lo) *lo = min(min(ll[nw], ll[ne]), min(ll[sw], ll[se]));
        if (hi) *hi = max(max(hl[nw], hl[ne]), max(hl[sw], hl[se]));
    }
}

void BoundsPyramid::computeBase(float ampl, float period, float xshift, float yshift, float zshift)
{
    using glm::vec2;

    for (int y = 0; y < this->size; ++y)
    {
        for (int x = 0; x < this->size; ++x)
        {
            vec2 p = vec2((float)x + xshift, (float)y + yshift);
            float n = glm::simplex(p * period) * ampl + zshift;
            int i = index(x, y, this->size);
            this->base[i] = n;
        }
    }
}

BoundsPyramid::BoundsPyramid(int size, float ampl, float period, float xshift, float yshift, float zshift)
{
    assert(ispowof2(size));

    int levels = log2(size);

    this->size = size;
    this->levels = levels;
    this->base = new float[size*size];
    this->bounds = new float**[levels];
    for (int i = 0; i < levels; ++i)
    {
        int s = pow2(i);
        this->bounds[i] = new float*[2];
        this->bounds[i][0] = new float[s*s];
        this->bounds[i][1] = new float[s*s];
    }

    this->computeBase(ampl, period, xshift, yshift, zshift);
    this->computeBounds(0, 0, 0, 0, 0, NULL, NULL);
}

BoundsPyramid::~BoundsPyramid()
{
    for (int i = 0; i < levels; ++i)
    {
        for (int j = 0; j < 2; ++j)
            delete[] this->bounds[i][j];
        delete[] this->bounds[i];
    }
    delete[] this->bounds;
    delete[] this->base;
        
}

float BoundsPyramid::min(float xf, float yf, int lv) const
{
    return this->bound(xf, yf, 0, lv);
}

float BoundsPyramid::max(float xf, float yf, int lv) const
{
    return this->bound(xf, yf, 1, lv);
}

float BoundsPyramid::bound(float xf, float yf, int b, int lv) const
{
    assert(b == 0 || b == 1);
    assert(xf >= 0 && xf <= 1);
    assert(yf >= 0 && yf <= 1);

    int x = xf * this->size, y = yf * this->size;

    if (lv == this->levels)
        return this->base[index(x, y, this->size)];
    assert(lv < this->levels);

    int d = pow2(this->levels - lv);
    int j = index(x / d, y / d, this->size / d);
    return this->bounds[lv][b][j];
}
