#include <stdio.h>
#include "Debug.h"
#include "Octree.h"
#include "BoundsPyramid.h"
#include "Draw.h"

#define C putchar

#define S printf

static void B(int size)
{
    C('+');
    for (int i = 0; i < size; ++i) 
        C('-');
    C('+');
}

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

void print(const BoundsPyramid *pyr)
{
    printf("BoundsPyramid\n");
    printf("Size: %d\n", pyr->size);
    printf("Levels: %d\n", pyr->levels);

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
}

void printTree(const Ocroot *r, uint32_t t)
{
    const char *tps[] = { "empty", "leaf", "branch", "twig" };
    Octype tp = (Octype)r->tree[t].type;
    printf("%08x:[%s:%08x]\n", t, tps[tp], r->tree[t].offset);
    if (tp == TWIG)
        printf("\t%016llx\n", r->twig[r->tree[t].offset].leafmap[0]);
    if (tp == BRANCH)
    {
        for (int i = 0; i < 8; ++i)
            printTree(r, r->tree[t].offset + i);
    }
}

void print(const Ocroot *root)
{
    printf("Ocroot\n");
    printf("Position: %f,%f,%f\n", root->position.x, root->position.y, root->position.z);
    printf("Size: %f\n", root->size);
    printf("Density: %f\n", root->density);
    printf("Depth: %d\n", root->depth);
    printf("#Trees: %llu\n", root->trees);
    printf("#Twigs: %llu\n", root->twigs);

    printf("Used %f%% of tree address space!\n", 100.f * (float)root->trees / (double)(uint32_t)~(3 << 30));
    printf("Used %f%% of twig address space!\n", 100.f * (float)root->twigs / (double)(uint32_t)~(3 << 30));

    // printTree(root, 0);

    C('\n');
}

void print(const Mesh *mesh)
{
    printf("Mesh\n");
    printf("#Vertex coordinates: %llu\n", mesh->vtxcoords.size());
    printf("#UV coordinates: %llu\n", mesh->uvcoords.size());
    printf("#Indices: %llu\n", mesh->indices.size());

    C('\n');
}
