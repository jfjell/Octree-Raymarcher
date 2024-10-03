#pragma once

#ifndef BITSET_H
#define BITSET_H

#include <stdint.h>

class Bitset
{
private:
    uint64_t *word;
    size_t size;

    void grow(size_t i);
public:
    Bitset();
    ~Bitset();

    bool operator[](size_t bit);
    void operator+=(size_t bit);
    void operator-=(size_t bit);
    void operator|=(const Bitset& set);
    void operator&=(const Bitset& set);
};

#endif