#include <stdio.h>
#include <string>
#include <iostream>
#include "Debug.h"
#include "Octree.h"
#include "BoundsPyramid.h"
#include "Draw.h"

using std::string;

#define C putchar
#define S printf

using namespace std;

void printsize(size_t value)
{
    size_t K = 1024;
    size_t M = K*K;
    size_t G = K*K*K;

    string s = "";
    if (value > G)
        cout << ((double)value / G) << "GB";
    else if (value > M)
        cout << ((double)value / M) << "MB";
    else if (value > K)
        cout << ((double)value / K) << "KB";
    else
        cout << value << "B";
}

/*
static void B(int size)
{
    C('+');
    for (int i = 0; i < size; ++i) 
        C('-');
    C('+');
}
*/

/*
static void grid(float *g, int s)
{
    int bm = 9, bp = s-1+2;
    B(s*bm+bp); 
    C('\n');

    for (int y = 0; y < s; ++y)
    {
        S("| ");
        for (int x = 0; x < s; ++x)
        {
            printf("%+02.6f ", g[y * s + x]);
        }
        S("|\n");
    }

    B(s*bm+bp); 
    C('\n');
}
*/

void print(const BoundsPyramid *pyr)
{
    printf("[BoundsPyramid]\n");
    printf("Size: %d\n", (int)pyr->size);
    printf("Levels: %d\n", (int)pyr->levels);

    size_t m = pyr->size * pyr->size * sizeof(float);
    size_t s = pyr->size / 2;
    while (s > 0)
    {
        m += 2 * s * s * sizeof(float);
        s /= 2;
    }

    S("Memory: ");
    printsize(m);
    C('\n');

    /*
    for (int i = 0; i < pyr->levels; ++i)
    {
        int s = 1 << i;

        printf("min#%d:\n", i);
        grid(pyr->bounds[i][0], s);

        printf("max#%d:\n", i);
        grid(pyr->bounds[i][1], s);
    }

    printf("Base:\n");
    grid(pyr->base, pyr->size);
    */

   // (void)grid;
    C('\n');
}

/*
static void printTree(const Ocroot *r, uint32_t t)
{
    const char *tps[] = { "empty", "leaf", "branch", "twig" };
    Octype tp = (Octype)r->tree[t].type();
    printf("%08x:[%s:%08x]\n", t, tps[tp], (uint32_t)r->tree[t].offset());
    if (tp == TWIG)
        (void)0; //printf("\t%016llx\n", (uint64_t)r->twig[r->tree[t].offset()].leafmap[0]);
    if (tp == BRANCH)
    {
        for (int i = 0; i < 8; ++i)
            printTree(r, (uint32_t)r->tree[t].offset() + i);
    }
}
*/

struct MinMax { uint32_t min, max; };

void popMM(MinMax *mm, const Ocroot *root, uint32_t index, int depth)
{
    if (mm[depth].min == (uint32_t)~0 || mm[depth].min > index) mm[depth].min = index;
    if (mm[depth].max == (uint32_t)~0 || mm[depth].max < index) mm[depth].max = index;

    if (root->tree[index].type() == BRANCH)
        for (uint32_t i = 0; i < 8; ++i)
            popMM(mm, root, (uint32_t)root->tree[index].offset() + i, depth+1);
}

void print(const Ocroot *root)
{
    printf("[Ocroot]\n");
    printf("Position: %f,%f,%f\n", root->position.x, root->position.y, root->position.z);
    printf("Size: %f\n", root->size);
    printf("Depth: %d\n", root->depth);
    printf("#Trees: %llu\n", root->trees);
    printf("#Allocated trees: %llu\n", root->treestoragesize);
    printf("#Bricks: %llu\n", root->twigs);
    printf("#Allocated bricks: %llu\n", root->twigstoragesize);
    printf("Tree utilization: %f%%\n", (double)root->trees * 100.0 / root->treestoragesize);
    printf("Brick utilization: %f%%\n", (double)root->twigs * 100.0 / root->twigstoragesize);

    // printf("Used %f%% of tree address space!\n", 100.f * (float)root->trees / (double)(uint32_t)~(3 << 30));
    // printf("Used %f%% of brick address space!\n", 100.f * (float)root->twigs / (double)(uint32_t)~(3 << 30));
    
    S("Optimal memory: ");
    printsize(root->trees * sizeof(Octree) + root->twigs * sizeof(Octwig));

    C('\n');

    S("Real memory: ");
    printsize(root->treestoragesize * sizeof(Octree) + root->twigstoragesize * sizeof(Octwig));

    C('\n');

    /*
#define MAXLVLS 32
    MinMax mm[MAXLVLS];
    for (int i = 0; i < MAXLVLS; ++i)
    {
        mm[i].min = (uint32_t)~0;
        mm[i].max = (uint32_t)~0;
    }
    popMM(mm, root, 0, 0);

    for (int i = 0; i < MAXLVLS && mm[i].min != (uint32_t)~0; ++i)
        printf("Depth %d: [%u, %u]\n", i, mm[i].min, mm[i].max);
    */


    // printTree(root, 0);
    // (void)printTree;

    S("\n\n");
}

void print(const Mesh *mesh)
{
    printf("[Mesh]\n");
    printf("#Vertex coordinates: %llu\n", mesh->vtxcoords.size());
    printf("#UV coordinates: %llu\n", mesh->uvcoords.size());
    printf("#Indices: %llu\n", mesh->indices.size());

    if (mesh->vtxcoords.size() / 3 > 0xffff)
        printf("Note: does not fit in an unsigned short!\n");

    if (mesh->vtxcoords.size() / 3 > 0xffffffff)
        printf("Note: does not fit in an unsigned int!\n");

    size_t vtxbytes = mesh->vtxcoords.size() * sizeof(mesh->vtxcoords[0]);
    size_t uvbytes = mesh->uvcoords.size() * sizeof(mesh->uvcoords[0]);
    size_t indexbytes = mesh->indices.size() * sizeof(mesh->indices[0]);
    size_t sum = vtxbytes + uvbytes + indexbytes;
    S("Memory: vertices + uv + indices = ");
    printsize(vtxbytes);
    S(" + ");
    printsize(uvbytes);
    S(" + ");
    printsize(indexbytes);
    S(" = ");
    printsize(sum);

    S("\n\n");
}
