#include <string.h>
#include "Bitset.h"

#define MASK 0x3f
#define SHIFT 6

Bitset::Bitset()
{
    size = 64;
    word = new uint64_t[size];
    memset(word, 0, size * sizeof(uint64_t));
}

Bitset::~Bitset()
{
    delete[] word;
}

void Bitset::grow(size_t i)
{
    size_t s = size;
    while (i >= s) 
        s <<= 1;
    if (s == size)
        return;
    uint64_t *w = new uint64_t[s];
    memcpy(w, word, size * sizeof(uint64_t));
    memset(w + size, 0, (s - size) * sizeof(uint64_t));
    delete[] word;
    size = s;
    word = w;
}

bool Bitset::operator[](size_t bit)
{
    size_t i = bit >> SHIFT, j = bit & MASK;
    if (i >= size) [[unlikely]]
        return false;
    return (word[i] & (1ull << j)) != 0;
}

void Bitset::operator+=(size_t bit)
{
    size_t i = bit >> SHIFT, j = bit & MASK;
    if (i >= size) [[unlikely]]
        grow(i);
    word[i] |= (1ull << j);
}

void Bitset::operator-=(size_t bit)
{
    size_t i = bit >> SHIFT, j = bit & MASK;
    if (i >= size) [[unlikely]]
        return;
    word[i] &= ~(1ull << j);
}

void Bitset::operator|=(const Bitset& set)
{
    if (set.size > size) [[unlikely]]
        grow(set.size);
    for (size_t i = 0; i < set.size; ++i)
        word[i] |= set.word[i];
}

void Bitset::operator&=(const Bitset& set)
{
    if (set.size > size) [[unlikely]]
        grow(set.size);
    for (size_t i = 0; i < set.size; ++i)
        word[i] &= set.word[i];
}