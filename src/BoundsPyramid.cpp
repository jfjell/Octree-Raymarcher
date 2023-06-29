#include <assert.h>
#include <float.h>
#include <glm/vec2.hpp>
#include <glm/common.hpp>
#include <glm/gtc/noise.hpp>
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

        float *ll = bounds[lv][MIN];
        float *hl = bounds[lv][MAX];
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
        this->bounds[i][MIN] = new float[s*s];
        this->bounds[i][MAX] = new float[s*s];
    }

    this->computeBase(ampl, period, xshift, yshift, zshift);
    this->computeBounds(0, 0, 0, 0, 0, NULL, NULL);
}

BoundsPyramid::~BoundsPyramid()
{
    for (int i = 0; i < levels; ++i)
    {
        delete[] this->bounds[i][MIN];
        delete[] this->bounds[i][MAX];
        delete[] this->bounds[i];
    }
    delete[] this->bounds;
    delete[] this->base;
        
}

float BoundsPyramid::min(float xf, float yf, int lv) const
{
    return this->bound(xf, yf, MIN, lv);
}

float BoundsPyramid::max(float xf, float yf, int lv) const
{
    return this->bound(xf, yf, MAX, lv);
}

static float lerp(float x0, float x1, float t)
{
    return x1 * t + (1.0 - t) * x0;
}

static float blerp(float yx00, float yx01, float yx10, float yx11, float t, float s)
{
    float y0 = lerp(yx00, yx01, t);
    float y1 = lerp(yx10, yx11, t);
    return lerp(y0, y1, s);
}

float BoundsPyramid::bound(float xf, float yf, int b, int lv) const
{
    assert(b == MIN || b == MAX);
    assert(xf >= 0 && xf <= 1);
    assert(yf >= 0 && yf <= 1);

    int x = xf * this->size, y = yf * this->size;
    if (lv < this->levels)
    {
        int d = pow2(this->levels - lv);
        int j = index(x / d, y / d, this->size / d);
        return this->bounds[lv][b][j];
    }
    else if (lv == this->levels)
    {
        return this->base[index(x, y, this->size)];
    }
    else
    {
        int x0 = x, y0 = y;
        int x1 = (x0 + 1) % this->size, y1 = (y0 + 1) % this->size;
        float t = (double)(xf * this->size) - x0;
        float s = (double)(yf * this->size) - y0;
        float yx00 = this->base[index(x0, y0, this->size)];
        float yx01 = this->base[index(x1, y0, this->size)];
        float yx10 = this->base[index(x0, y1, this->size)];
        float yx11 = this->base[index(x1, y1, this->size)];
        return blerp(yx00, yx01, yx10, yx11, t, s);
    }
}
