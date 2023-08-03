#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#define TWIG_BITS 12
#define TWIG_WORD_BITS 32
#define TWIG_DEPTH 2
#define TWIG_SIZE 4
#define TWIG_SIZE2 (TWIG_SIZE*TWIG_SIZE)
#define TWIG_WORDS (((1 << (TWIG_DEPTH * 3)) * TWIG_BITS) / TWIG_WORD_BITS)
#define TWIG_LEAF_MAX (1 << TWIG_BITS)
#define TWIG_LEAF_MASK (TWIG_LEAF_MAX - 1)

int index(int x, int y, int z)
{
    return z * TWIG_SIZE2 + y * TWIG_SIZE + x;
}

int lw(int x, int y, int z)
{
    return index(x, y, z) * TWIG_BITS / TWIG_WORD_BITS;
}

int lwlb(int x, int y, int z)
{
    return index(x, y, z) * TWIG_BITS % TWIG_WORD_BITS;
}

int hw(int x, int y, int z)
{
    return (index(x, y, z) * TWIG_BITS + TWIG_BITS - 1) / TWIG_WORD_BITS;
}

int hwhb(int x, int y, int z)
{
    return (index(x, y, z) * TWIG_BITS + TWIG_BITS - 1) % TWIG_WORD_BITS;
}

struct Twig
{
    uint32_t leaf[TWIG_WORDS];

    void index(int x, int y, int z, int *lw, int *lb, int *hw, int *hb);
    void set(unsigned v, int x, int y, int z);
    unsigned get(int x, int y, int z);
};

void Twig::index(int x, int y, int z, int *loword, int *lobit, int *hiword, int *hibit)
{
    assert(x >= 0 && x < TWIG_SIZE);
    assert(y >= 0 && y < TWIG_SIZE);
    assert(z >= 0 && z < TWIG_SIZE);
    int i1 = (z * TWIG_SIZE2 + y * TWIG_SIZE + x) * TWIG_BITS;
    int i2 = i1 + TWIG_BITS - 1;
    int w1 = i1 / TWIG_WORD_BITS;
    int w2 = i2 / TWIG_WORD_BITS;
    int b1 = i1 % TWIG_WORD_BITS;
    int b2 = i2 % TWIG_WORD_BITS;
    *loword = w1;
    *lobit = b1;
    *hiword = w2;
    *hibit = b2;
}

void Twig::set(unsigned v, int x, int y, int z)
{
    assert(v <= TWIG_LEAF_MAX);
    int w1, w2, b1, b2;
    index(x, y, z, &w1, &b1, &w2, &b2);
    if (w1 == w2)
        leaf[w1] = (leaf[w1] & ~(v << b1)) | (v << b1);
    else
    {
        uint32_t v1 = v << b1;
        uint32_t v2 = v >> (TWIG_WORD_BITS - b1);
        leaf[w1] = (leaf[w1] & ~v1) | v1;
        leaf[w2] = (leaf[w2] & ~v2) | v2;
    }
}

unsigned Twig::get(int x, int y, int z)
{
    int w1, w2, b1, b2;
    index(x, y, z, &w1, &b1, &w2, &b2);
    if (w1 == w2)
        return (leaf[w1] >> b1) & TWIG_LEAF_MASK;
    else
        return ((leaf[w1] >> b1) | (leaf[w2] << b1)) & TWIG_LEAF_MASK;
}

int main()
{
    printf("depth %d\n", TWIG_DEPTH);
    printf("size %d, size2 %d\n", TWIG_SIZE, TWIG_SIZE2);
    printf("dwords %d\n", TWIG_WORDS);

    printf("x,y,z | lw:lb | hw:hb\n");
    for (int z = 0; z < TWIG_SIZE; ++z)
        for (int y = 0; y < TWIG_SIZE; ++y)
            for (int x = 0; x < TWIG_SIZE; ++x)
                printf("%d,%d,%d | %d:%d | %d:%d\n",
                    x, y, z,
                    lw(x, y, z),
                    lwlb(x, y, z),
                    hw(x, y, z),
                    hwhb(x, y, z));

    return 0;
}
