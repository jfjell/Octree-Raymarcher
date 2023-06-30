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
    printf("[BoundsPyramid]\n");
    printf("Size: %d\n", pyr->size);
    printf("Levels: %d\n", pyr->levels);

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

   (void)grid;
    C('\n');
}

static void printTree(const Ocroot *r, uint32_t t)
{
    const char *tps[] = { "empty", "leaf", "branch", "twig" };
    Octype tp = (Octype)r->tree[t].type();
    printf("%08x:[%s:%08x]\n", t, tps[tp], (uint32_t)r->tree[t].offset());
    if (tp == TWIG)
        printf("\t%016llx\n", (uint64_t)r->twig[r->tree[t].offset()].leafmap[0]);
    if (tp == BRANCH)
    {
        for (int i = 0; i < 8; ++i)
            printTree(r, r->tree[t].offset() + i);
    }
}


void print(const Ocroot *root)
{
    printf("[Ocroot]\n");
    printf("Position: %f,%f,%f\n", root->position.x, root->position.y, root->position.z);
    printf("Size: %f\n", root->size);
    printf("Density: %f\n", root->density);
    printf("Depth: %d\n", root->depth);
    printf("#Trees: %llu\n", root->trees);
    printf("#Bricks: %llu\n", root->twigs);
    printf("#Materials: %llu\n", root->barks);

    printf("Used %f%% of tree address space!\n", 100.f * (float)root->trees / (double)(uint32_t)~(3 << 30));
    printf("Used %f%% of brick address space!\n", 100.f * (float)root->twigs / (double)(uint32_t)~(3 << 30));
    printf("Used %f%% of material address space!\n", 100.f * (float)root->barks / (double)(uint32_t)~(3 << 30));
    
    S("Memory: ");
    printsize(root->trees * sizeof(Octree) + root->twigs * sizeof(Octwig) + root->barks * sizeof(Ocbark));

    // printTree(root, 0);
    (void)printTree;

    C('\n');
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
