#include <assert.h>
#include <float.h>
#include <glm/vec2.hpp>
#include <glm/common.hpp>
#include <glm/gtc/noise.hpp>
#include "BoundsPyramid.h"

static inline bool ispowof2(size_t i)
{
    return !(i & (i - 1));
}

#ifdef _MSC_VER
# include <intrin.h>
#endif
static inline size_t log2(size_t i)
{
#ifdef _MSC_VER
    return _tzcnt_u64(i);
#else
    return __builtin_ctz(i);
#endif
}

static inline size_t pow2(size_t i)
{
    return (size_t)1 << i;
}

static inline size_t index(size_t x, size_t z, size_t s)
{
    return z * s + x;
}

static inline float lerp(float x0, float x1, float t)
{
    return (float)(x1 * t + (1.0 - t) * x0);
}

static inline float blerp(float yx00, float yx01, float yx10, float yx11, float t, float s)
{
    float y0 = lerp(yx00, yx01, t);
    float y1 = lerp(yx10, yx11, t);
    return lerp(y0, y1, s);
}

void BoundsPyramid::init(size_t size, float ampl, float period, float xshift, float yshift, float zshift)
{
    assert(ispowof2(size));

    this->size = size;
    this->levels = log2(size);
    this->amplitude = ampl;
    this->shift = yshift;

    this->basequad = new half[this->size*this->size];
    this->minquad = new half*[this->levels+1];
    this->maxquad = new half*[this->levels+1];

    for (size_t i = 0, s = 1; i < levels; ++i, s *= 2)
    {
        this->minquad[i] = new half[s*s];
        for (size_t j = 0; j < s*s; ++j)
            this->minquad[i][j] = 1.0;

        this->maxquad[i] = new half[s*s];
        for (size_t j = 0; j < s*s; ++j)
            this->maxquad[i][j] = -1.0;
    }
    this->minquad[this->levels] = this->maxquad[this->levels] = this->basequad;

    this->computeBase(period, xshift, zshift);

    for (size_t i = 0; i < levels; ++i)
    {
        this->computeBoundsAbove(levels - i);
    }
}

void BoundsPyramid::deinit()
{
    delete[] this->basequad;
    for (size_t i = 0; i < levels; ++i)
    {
        delete[] this->minquad[i];
        delete[] this->maxquad[i];
    }
    delete[] this->minquad;
    delete[] this->maxquad;
}

void BoundsPyramid::computeBase(float period, float xshift, float zshift)
{
    for (size_t z = 0, i = 0; z < this->size; ++z)
    {
        for (size_t x = 0; x < this->size; ++x, ++i)
        {
            auto point = glm::vec2((float)x + xshift, (float)z + zshift) * period;
            float noise = glm::simplex(point);
            assert(noise >= -1.0 && noise <= +1.0);
            this->basequad[i] = noise;
        }
    }
}

void BoundsPyramid::computeBoundsAbove(size_t lv)
{
    using glm::min;
    using glm::max;

    assert(0 < lv && lv <= this->levels);
    size_t above = lv-1;
    size_t s = pow2(lv);

    for (size_t z = 0; z < s; ++z)
    {
        for (size_t x = 0; x < s; x += 2)
        {
            size_t i = index(x+0, z, s);
            size_t j = index(x+1, z, s);
            size_t k = index(x / 2, z / 2, s / 2);

            float min0 = this->minquad[above][k];
            float min1 = this->minquad[lv][i];
            float min2 = this->minquad[lv][j];

            float max0 = this->maxquad[above][k];
            float max1 = this->maxquad[lv][i];
            float max2 = this->maxquad[lv][j];

            this->minquad[above][k] = glm::min(min0, glm::min(min1, min2));
            this->maxquad[above][k] = glm::max(max0, glm::max(max1, max2));
        }
    }
}
float BoundsPyramid::min(float x, float z, size_t lv) const
{
    return this->bound(x, z, lv, this->minquad[lv]);
}

float BoundsPyramid::max(float x, float z, size_t lv) const
{
    return this->bound(x, z, lv, this->maxquad[lv]);
}

float BoundsPyramid::bound(float x, float z, size_t lv, const half *q) const
{
    assert(0 <= x && x <= 1.0);
    assert(0 <= z && z <= 1.0);
    
    // Base integer coordinates
    size_t a = (size_t)(x * this->size);
    size_t b = (size_t)(z * this->size);

    if (lv <= this->levels)
    {
        // Within bounds => simply compute the index for the level and return
        size_t d = pow2(this->levels - lv);
        size_t i = index(a / d, b / d, this->size / d);
        return q[i] * this->amplitude + this->shift;
    }

    // Out of bounds => interpolate values from the base
    size_t mask = this->size - 1;
    size_t a0 = a, a1 = (a0 + 1) & mask;
    size_t b0 = b, b1 = (b0 + 1) & mask;
    float t = (float)(x * this->size) - a0;
    float s = (float)(z * this->size) - b0;
    float ba00 = this->basequad[index(a0, b0, this->size)];
    float ba01 = this->basequad[index(a1, b0, this->size)];
    float ba10 = this->basequad[index(a0, b1, this->size)];
    float ba11 = this->basequad[index(a1, b1, this->size)];
    return blerp(ba00, ba01, ba10, ba11, t, s) * this->amplitude + this->shift;
}
