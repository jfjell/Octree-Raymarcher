#pragma once

#ifndef DEBUG_H
#define DEBUG_H

#define SW_START(sw, ...) do { \
    int _sw_start_n = printf(__VA_ARGS__); \
    for (int _sw_i = 0; _sw_i < 40 - _sw_start_n; ++_sw_i) putchar('.'); \
    sw.start(); \
}while(0)

#define SW_STOP(sw) printf("%fs\n", sw.stop())

struct BoundsPyramid;
void print(const BoundsPyramid *pyr);

struct Ocroot;
void print(const Ocroot *root);

struct Mesh;
void print(const Mesh *mesh);

#endif