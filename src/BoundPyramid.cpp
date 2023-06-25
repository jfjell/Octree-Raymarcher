#include <assert.h>
#include <float.h>
#include <glm/vec2.hpp>
#include <glm/gtc/noise.hpp>
#include "BoundPyramid.h"

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

void BoundPyramid::compute(float ampl, float period, float xshift, float yshift, float shift)
{
    for (int y = 0; y < this->size; ++y)
    {
        for (int x = 0; x < this->size; ++x)
        {
            glm::vec2 p = glm::vec2((float)x + xshift, (float)y + yshift);
            float n = glm::simplex(p * period) * ampl + shift;

            int i = index(x, y, this->size);
            this->base[i] = n;

            // Walk up the pyramid, changing bound values if necessary
            for (int lv = this->levels - 1; lv >= 0; --lv)
            {
                int d = pow2(this->levels - lv);
                int j = index(x / d, y / d, this->size / d);
                if (n < this->bounds[lv][0][j]) this->bounds[lv][0][j] = n;
                if (n > this->bounds[lv][1][j]) this->bounds[lv][1][j] = n;
            }

        }
    }
}

BoundPyramid::BoundPyramid(int size, float ampl, float period, float xshift, float yshift, float shift)
{
    assert(ispowof2(size));

    int levels = log2(size);

    this->size = size;
    this->levels = levels;
    this->base = new float[size*size];

    // Allocate the bounds, and fill with initial values
    this->bounds = new float**[levels];
    for (int i = 0; i < levels; ++i)
    {
        this->bounds[i] = new float*[2];
        int s = pow2(i);

        this->bounds[i][0] = new float[s*s];
        for (int j = 0; j < s*s; ++j)
            this->bounds[i][0][j] = FLT_MAX;

        this->bounds[i][1] = new float[s*s];
        for (int j = 0; j < s*s; ++j)
            this->bounds[i][1][j] = FLT_MIN;
    }

    this->compute(ampl, period, xshift, yshift, shift);
}

BoundPyramid::~BoundPyramid()
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

float BoundPyramid::min(int x, int y, int lv)
{
    return this->bound(x, y, 0, lv);
}

float BoundPyramid::max(int x, int y, int lv)
{
    return this->bound(x, y, 1, lv);
}

float BoundPyramid::bound(int x, int y, int b, int lv)
{
    assert(b == 0 || b == 1);
    if (lv == this->levels)
        return this->base[index(x, y, this->size)];
    assert(lv < this->levels);

    int d = pow2(this->levels - lv);
    int j = index(x / d, y / d, this->size / d);
    return this->bounds[lv][b][j];
}
