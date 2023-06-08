#include <GL/glew.h>
#include "Shader.h"
#include "Util.h"

Shader::Shader()
{
    this->program = glCreateProgram();
}

Shader::~Shader()
{
    glDeleteProgram(this->program);
    this->program = 0;
}

void Shader::compile(const char *path, GLenum type)
{
    unsigned shader = glCreateShader(type);
    char *glsl = readFile(path);
    glShaderSource(shader, 1, &glsl, 0);
    delete[] glsl;
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
    glAttachShader(this->program, shader);
    glDeleteShader(shader);
}

unsigned Shader::link()
{
    glLinkProgram(this->program);
    int linked;
    glGetProgramiv(this->program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        int size;
        glGetProgramiv(this->program, GL_INFO_LOG_LENGTH, &size);
        char *log = new char[size];
        glGetProgramInfoLog(this->program, size, &size, log);
        die("glLinkProgram: %s\n", log);
        delete[] log;
    }
    return this->program;
}