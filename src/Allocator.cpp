#include <GL/glew.h>
#include <assert.h>
#include <limits.h>
#include "Allocator.h"
#include "Octree.h"

void RootAllocator::init(int chunks)
{
    tree.init(chunks, 1, 2);
    twig.init(chunks, 1, 3);
}

void RootAllocator::release()
{
    tree.release();
    twig.release();
}

void RootAllocator::bind()
{
    for (ssize_t i = 0; i < tree.regions; ++i)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, tree.region[i].index, tree.region[i].ssbo);

    for (ssize_t i = 0; i < twig.regions; ++i)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, twig.region[i].index, twig.region[i].ssbo);
}

RootAllocation RootAllocator::alloc(int key, const Ocroot *root)
{
    Allocation t = tree.alloc(key, root->tree, 
        root->treestoragesize * sizeof(Octree), root->trees * sizeof(Octree));
    Allocation w = twig.alloc(key, root->twig, 
        root->twigstoragesize * sizeof(Octwig), root->twigs * sizeof(Octwig));
    return RootAllocation(t, w);
}

RootAllocation RootAllocator::subst(int key, const Ocroot *root, 
    const Ocdelta *dt, const Ocdelta *dw)
{
    RootAllocation a(tree.allocation[key], twig.allocation[key]);

    if (dt->realloc)
        a.tree = tree.alloc(key, root->tree, 
            root->treestoragesize * sizeof(Octree), root->trees * sizeof(Octree));
    else if (dt->left < dt->right)
        tree.subst(key, root->tree, dt->left * sizeof(Octree), dt->right * sizeof(Octree));

    if (dw->realloc)
        a.twig = twig.alloc(key, root->twig,
            root->twigstoragesize * sizeof(Octwig), root->twigs * sizeof(Octwig));
    else if (dw->left < dw->right)
        twig.subst(key, root->twig, dw->left * sizeof(Octwig), dw->right * sizeof(Octwig));

    return a;
}

void RootAllocator::free(int key)
{
    tree.free(key);
    twig.free(key);
}

void Allocator::init(int keys, int regions, unsigned int index)
{
    this->keys = keys;
    allocation = new Allocation[keys];
    for (ssize_t i = 0; i < keys; ++i)
        allocation[i] = Allocation(-1, 0, 0);

    this->regions = regions;
    region = new Region[regions];
    for (ssize_t i = 0; i < regions; ++i)
        region[i].init(index + (unsigned int)i);
}

void Allocator::release()
{
    for (ssize_t i = 0; i < regions; ++i)
        region[i].release();
    delete[] allocation;
    delete[] region;
}

Allocation Allocator::alloc(int key, const void *bytes, ssize_t bytecount, ssize_t copycount)
{
    if (allocation[key].region != -1)
        free(key);

    ssize_t reg = 0;
    // Find the smallest region
    for (ssize_t i = 1; i < regions; ++i)
        if (region[i].rightmost < region[reg].rightmost)
            reg = i;

    ssize_t off = region[reg].alloc(bytes, bytecount, copycount);

    allocation[key] = Allocation(reg, off, bytecount);
    return allocation[key];
}

void Allocator::subst(int key, const void *bytes, ssize_t left, ssize_t right)
{
    Allocation a = allocation[key];
    assert(a.region != -1);
    region[a.region].subst(bytes, a.offset, left, right);
}

void Allocator::free(int key)
{
    Allocation a = allocation[key];
    assert(a.region != -1);
    region[a.region].free(a.offset, a.count);
    allocation[key] = Allocation(-1, 0, 0);
}

void Region::init(unsigned int index)
{
    this->index = index;
    count = 4096;
    rightmost = 0;
    freechunk.head = nullptr;
    
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count, NULL, GL_STATIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    freechunk.give(0, count);
}

void Region::release()
{
    freechunk.release();
    glDeleteBuffers(1, &ssbo);
}

void Region::grow()
{
    const ssize_t MAX_SIZE = INT_MAX;

    ssize_t nextcount = count * 2;
    assert(nextcount <= MAX_SIZE);

    char *prev = new char[rightmost];
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, rightmost, prev);

    glBufferData(GL_SHADER_STORAGE_BUFFER, nextcount, NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, rightmost, prev);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    freechunk.give(count, count);

    count = nextcount;

    delete[] prev;
}

ssize_t Region::alloc(const void *bytes, ssize_t bytecount, ssize_t copycount)
{
    ssize_t offset = -1;
    while ((offset = freechunk.take(bytecount)) < 0)
        grow();

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, copycount, bytes);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    ssize_t nextright = offset + bytecount;
    if (rightmost < nextright) rightmost = nextright; 
    return offset;
}

void Region::subst(const void *bytes, ssize_t offset, ssize_t left, ssize_t right)
{
    ssize_t delta = right - left;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, delta, bytes);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Region::free(ssize_t offset, ssize_t count)
{
    freechunk.give(offset, count);
    if (rightmost == offset + count) rightmost = offset;
}

void LinkedFreeChunkList::release()
{
    LinkedFreeChunk *chunk = head, *next = nullptr;
    while (chunk)
    {
        next = chunk->next;
        delete chunk;
        chunk = next;
    }
}

void LinkedFreeChunkList::give(ssize_t offset, ssize_t count)
{
    head = give(head, offset, offset + count);
}

ssize_t LinkedFreeChunkList::take(ssize_t count)
{
    ssize_t offset = -1;
    head = take(head, count, &offset);
    return offset;
}

LinkedFreeChunk * LinkedFreeChunkList::give(LinkedFreeChunk *chunk, ssize_t start, ssize_t end)
{
    if (!chunk)
        // Reached the end of the list
        return new LinkedFreeChunk(start, end, nullptr);
    if (end < chunk->start)
        // New chunk should be before this one
        return new LinkedFreeChunk(start, end, chunk);
    
    if (start > chunk->end)
        // New chunk should be after this one
        chunk->next = give(chunk->next, start, end);
    else if (end == chunk->start)
        // Combine left
        chunk->start = start;
    else if (start == chunk->end)
        // Combine right
        chunk->end = end;
    
    return chunk;
}

LinkedFreeChunk * LinkedFreeChunkList::take(LinkedFreeChunk *chunk, ssize_t count, ssize_t *offset)
{
    if (!chunk)
    {
        // Reached the end of the list without finding a match!
        *offset = -1;
        return nullptr;
    }

    ssize_t chunkcount = chunk->end - chunk->start;
    if (chunkcount == count) 
    {
        // Found an exact match
        *offset = chunk->start;
        LinkedFreeChunk *next = chunk->next;
        delete chunk;
        return next;
    }

    if (chunkcount > count) 
    {
        // Chunk is larger => split it
        *offset = chunk->start;
        chunk->start += count;
        return chunk;
    }

    // This one is too small => check the next one in the list
    return take(chunk->next, count, offset);
}
