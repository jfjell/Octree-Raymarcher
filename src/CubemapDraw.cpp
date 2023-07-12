#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/glew.h>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Draw.h"
#include "Octree.h"
#include "Shader.h"

static void cubeTree(const Ocroot *r, Octree t, glm::vec3 pos, float size, Mesh *m)
{
    using glm::vec3;

    switch (t.type())
    {
        case EMPTY:
            return;
        case LEAF:
            m->cubemap(pos, vec3(size, size, size));
            return;
        case TWIG:
        {
            float leafSize = size / (1 << TWIG_LEVELS);
            Octwig twig = r->twig[t.offset()];
            for (int y = 0; y < TWIG_SIZE; ++y)
                for (int z = 0; z < TWIG_SIZE; ++z)
                    for (int x = 0; x < TWIG_SIZE; ++x)
                        if (twig.leafmap[Octwig::word(x, y, z)] & (1 << Octwig::bit(x, y, z)))
                            m->cubemap(pos + vec3(x, y, z) * leafSize, vec3(leafSize, leafSize, leafSize));
            return;
        }
        case BRANCH:
        {
            for (int i = 0; i < 8; ++i)
            {
                bool xg, yg, zg;
                Octree::cut(i, &xg, &yg, &zg);
                cubeTree(r, r->tree[t.offset() + i], pos + vec3(xg, yg, zg) * (size / 2), size / 2, m);
            }
            return;
        }
    }
}

OctreeCubemapDrawer::~OctreeCubemapDrawer()
{
    glDeleteProgram(shader);
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
}

void OctreeCubemapDrawer::loadTree(const Ocroot *root)
{
    // Load the tree into a mesh
    cubeTree(root, root->tree[0], root->position, root->size, &this->mesh);
}

void OctreeCubemapDrawer::loadGL(const char *texture)
{
    vao = shader = tex = 0;

    // VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Shader
    /*
    this->shader = glCreateProgram();
    Shader::compileAttach("shaders/Cubemap.Vertex.glsl", GL_VERTEX_SHADER, shader);
    Shader::compileAttach("shaders/Cubemap.Fragment.glsl", GL_FRAGMENT_SHADER, shader);
    Shader::link(shader);
    */
   this->shader = Shader(glCreateProgram()).vertex("shaders/Cubemap.Vertex.glsl")
                                           .fragment("shaders/Cubemap.Fragment.glsl")
                                           .link();

    this->mvp = glGetUniformLocation(shader, "mvp");
    assert(mvp != -1);
    this->sampler = glGetUniformLocation(shader, "sampler");
    assert(sampler != -1);

    // Vertices
    mesh.bind();

    // Texture
    glGenTextures(1, &this->tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, this->tex);

    SDL_Surface *vt = IMG_Load(texture);
    for (unsigned i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, vt->w, vt->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, vt->pixels);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);  

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    SDL_FreeSurface(vt);
}

void OctreeCubemapDrawer::draw(const glm::mat4 MVP)
{
    glBindVertexArray(vao);
    glUseProgram(shader);

    glUniformMatrix4fv(mvp, 1, GL_FALSE, glm::value_ptr(MVP));
    glUniform1i(sampler, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, (void *)0);

    glUseProgram(0);
    glBindVertexArray(0);
}