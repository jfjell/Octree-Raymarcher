#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <GL/glew.h>
#include <unordered_map>
#include "Shader.h"
#include "Util.h"

Shader& Shader::compile(const char *path, unsigned int type)
{
    assert(this->program != 0);

    char *glss = readfile(path);

    int segments = 2 + (int)includes.size();
    const char **segment = new const char*[segments];
    segment[0] = "#version 430 core\n";
    for (int i = 0; i < (int)includes.size(); ++i)
        segment[i+1] = includes[i].c_str();
    segment[segments-1] = glss;

    unsigned int shader = glCreateShader(type);
    assert(shader != 0);
    glShaderSource(shader, segments, segment, 0);
    glCompileShader(shader);

    int compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        int size = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &size);
        char *plog = new char[size];
        glGetShaderInfoLog(shader, size, &size, plog);
        die("glCompileShader(\"%s\"): %s\n", path, plog);
        // delete[] plog;
    }
    glAttachShader(this->program, shader);

    delete[] segment;
    delete[] glss;
    glDeleteShader(shader);

    includes.clear();

    return *this;
}

Shader& Shader::vertex(const char *path)
{
    return this->compile(path, GL_VERTEX_SHADER);
}

Shader& Shader::fragment(const char *path)
{
    return this->compile(path, GL_FRAGMENT_SHADER);
}

unsigned int Shader::link()
{
    assert(this->program != 0);

    glLinkProgram(this->program);

    int linked = 0;
    glGetProgramiv(this->program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        int size = 0;
        glGetProgramiv(this->program, GL_INFO_LOG_LENGTH, &size);
        char *plog = new char[size];
        glGetProgramInfoLog(this->program, size, &size, plog);
        die("glLinkProgram: %s\n", plog);
        // delete[] plog;
    }

    return this->program;
}

Shader& Shader::include(const char *path)
{
    char *glss = readfile(path);
    includes.push_back(std::string(glss));
    delete[] glss;
    return *this;
}
