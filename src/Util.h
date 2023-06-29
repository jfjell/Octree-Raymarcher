#pragma once

#include <stdint.h>

[[noreturn]] void die(const char *format, ...);
char * readFile(const char *path);

struct Counter
{
    static constexpr size_t MEASUREMENTS = 32;

    void start();
    double restart();
    double elapsed();
    double avg();
    double std();
    double min();
    double max();

    double count[MEASUREMENTS] = {0};
    uintmax_t counter = 0;
    size_t index = 0;
};
