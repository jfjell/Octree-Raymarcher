#pragma once

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdint.h>
#include <assert.h>

typedef int64_t ssize_t;

struct LinkedFreeChunk
{
    ssize_t start, end;
    LinkedFreeChunk *next;

    LinkedFreeChunk(ssize_t s, ssize_t e, LinkedFreeChunk *n) : start(s), end(e), next(n) { assert(s < e); }
};

class LinkedFreeChunkList
{
public:
    LinkedFreeChunk *head = nullptr;

    void release();
    void give(ssize_t offset, ssize_t count);
    ssize_t take(ssize_t count);

    static LinkedFreeChunk * give(LinkedFreeChunk *chunk, ssize_t start, ssize_t end);
    static LinkedFreeChunk * take(LinkedFreeChunk *chunk, ssize_t count, ssize_t *offset);
};

class Region
{
public:
    LinkedFreeChunkList freechunk;
    ssize_t size, count;
    unsigned int ssbo, index;

    void init(unsigned int index);
    void release();
    void grow();
    ssize_t alloc(const void *bytes, ssize_t bytecount, ssize_t copycount);
    void subst(const void *bytes, ssize_t offset, ssize_t left, ssize_t right);
    void free(ssize_t offset, ssize_t count);
};

struct Allocation
{
    ssize_t region, offset, count;

    Allocation() = default;
    Allocation(ssize_t r, ssize_t o, ssize_t c) : region(r), offset(o), count(c) { }
};

class Allocator
{
public:
    Allocation *allocation;
    Region *region;
    ssize_t regions, keys;

    void init(int keys, int regions, unsigned int index);
    void release();
    Allocation alloc(int key, const char *bytes, ssize_t bytecount, ssize_t copycount);
    void subst(int key, const char *bytes, ssize_t left, ssize_t right);
    void free(int key);
};

struct RootAllocation
{
    Allocation tree, twig;

    RootAllocation (Allocation t, Allocation w) : tree(t), twig(w) { }
};

struct Ocroot;
struct Ocdelta;

class RootAllocator
{
public:
    Allocator tree, twig;

    void init(int chunks);
    void release();
    void bind();
    RootAllocation alloc(int key, const Ocroot *root);
    RootAllocation subst(int key, const Ocroot *root, const Ocdelta *dt, const Ocdelta *dw);
    void free(int key);
};

#endif