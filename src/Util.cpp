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

void Stopwatch::start()
{
    this->counter = now_ns();
}

double Stopwatch::stop()
{
    this->counter = now_ns() - this->counter;
    return (double)this->counter / 1.0e9;
}

[[noreturn]] void die(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    exit(-1);
}

char * readFile(const char *path)
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