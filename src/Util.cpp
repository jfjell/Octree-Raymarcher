#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <chrono>
#include "Util.h"

static inline uintmax_t now_ns()
{
    auto dt = std::chrono::high_resolution_clock::now().time_since_epoch();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(dt);
    return ns.count();
}

void Counter::start()
{
    this->counter = now_ns();
}

double Counter::restart()
{
    double ns = this->elapsed();
    this->start();
    size_t i = this->index++ % Counter::MEASUREMENTS;
    this->count[i] = ns;
    return ns;
}

double Counter::elapsed()
{
    return (now_ns() - this->counter) / 1.0e9;
}

double Counter::avg()
{
    double sum = 0.0;
    for (size_t i = 0; i < Counter::MEASUREMENTS; ++i)
        sum += this->count[i];
    double avg = sum / Counter::MEASUREMENTS;
    return avg;
}

double Counter::std()
{
    double avg = this->avg();
    double sigma = 0.0;
    for (size_t i = 0; i < Counter::MEASUREMENTS; ++i)
        sigma += (this->count[i] - avg) * (this->count[i] - avg);
    return sqrt(sigma / Counter::MEASUREMENTS);
}

double Counter::min()
{
    double val = this->count[0];
    for (size_t i = 1; i < Counter::MEASUREMENTS; ++i)
        if (val > this->count[i]) 
            val = this->count[i];
    return val;
}

double Counter::max()
{
    double val = this->count[0];
    for (size_t i = 1; i < Counter::MEASUREMENTS; ++i)
        if (val < this->count[i]) 
            val = this->count[i];
    return val;
}

[[noreturn]] void die(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    exit(-1);
}

char * readfile(const char *path)
{
    FILE *stream = fopen(path, "rb");    
    if (!stream)
        die("fopen(\"%s\"): %s\n", path, strerror(errno));
    fseek(stream, 0, SEEK_END);
    size_t size = ftell(stream);
    rewind(stream);
    char *cont = new char[size+1];
    fread(&cont[0], 1, size, stream);
    cont[size] = '\0';
    fclose(stream);
    return cont;
}