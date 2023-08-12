#pragma once

#ifndef MISRAGRIES_H
#define MISRAGRIES_H

#include <assert.h>

template <int K>
struct MisraGriesCounter
{
    static_assert(K > 0);

    int A[K];
    int keys[K];

    MisraGriesCounter();
    void empty();
    int count(int i);
    int count(int i, unsigned n);
    int majority() const;
};

template <int K>
MisraGriesCounter<K>::MisraGriesCounter()
{
    empty();
}

template <int K>
void MisraGriesCounter<K>::empty()
{
    for (int k = 0; k < K; ++k)
        A[k] = keys[k] = 0;
}

template <int K>
int MisraGriesCounter<K>::count(int i)
{
    // Is i already in one of the key slots?
    for (int k = 0; k < K; ++k)
        if (keys[k] == i && A[k] != 0)
            return ++A[k];
    
    // Is there an empty key slot?
    for (int k = 0; k < K; ++k)
        if (A[k] == 0)
            return (keys[k] = i, A[k] = 1);

    // Decrement every other key
    for (int k = 0; k < K; ++k)
        if (A[k] != 0)
            --A[k];

    return 0;
}

template <int K>
int MisraGriesCounter<K>::count(int i, unsigned n)
{
    assert(n > 0);

    /*
    for (unsigned j = 0; j < n - 1; ++j)
        count(i);
    return count(i);
    */
    // Is i already in one of the key slots?
    for (int k = 0; k < K; ++k)
        if (keys[k] == i && A[k] != 0)
            return (A[k] += n);
    
    // Is there an empty key slot?
    for (int k = 0; k < K; ++k)
        if (A[k] == 0)
            return (keys[k] = i, A[k] = n);

    // Find one with a count below n, if it exists
    int d = n, r = -1;
    for (int k = 1; k < K; ++k)
        if (A[k] < d)
            r = k, d = A[k];
    
    if (r != -1)
    {
        // Found one to replace
        keys[r] = i;
        A[r] = n;
    }
    // Decrement every other key
    for (int k = 0; k < K; ++k)
        A[k] -= d;

    return 0;
}

template <int K>
int MisraGriesCounter<K>::majority() const
{
    int i = 0;
    for (int k = 1; k < K; ++k)
        if (A[k] > A[i])
            i = k;
    return keys[i];
}

#endif