#pragma once

#include <stdint.h>

[[noreturn]] void die(const char *format, ...);
char * readFile(const char *path);

struct Stopwatch
{
    void start();
    double stop();
    double elapsed();

    uintmax_t counter;
};
