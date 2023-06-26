#include <GL/glew.h>
#include "Draw.h"
#include "Util.h"

unsigned Shader::compileAttach(const char *path, GLenum type, unsigned program)
{
    unsigned shader = glCreateShader(type);
    char *glsl = readFile(path);
    glShaderSource(shader, 1, &glsl, 0);
    glCompileShader(shader);

    int compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        int size;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &size);
        char *log = new char[size];
        glGetShaderInfoLog(shader, size, &size, log);
        die("glCompileShader(\"%s\"): %s\n", path, log);
        delete[] log;
    }
    glAttachShader(program, shader);

    delete[] glsl;
    glDeleteShader(shader);

    return program;
}

unsigned Shader::link(unsigned program)
{
    glLinkProgram(program);

    int linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        int size;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &size);
        char *log = new char[size];
        glGetProgramInfoLog(program, size, &size, log);
        die("glLinkProgram: %s\n", log);
        delete[] log;
    }

    return program;
}