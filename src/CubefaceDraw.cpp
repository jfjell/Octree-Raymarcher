#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/glew.h>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Draw.h"
#include "Octree.h"

static void cubeTree(const Ocroot *r, Octree t, glm::vec3 pos, float size, Mesh *m)
{
    using glm::vec3;

    switch (t.type())
    {
        case EMPTY:
            return;
        case LEAF:
            m->cubeface(pos, vec3(size, size, size));
            return;
        case TWIG:
        {
            float leafSize = size / (1 << TWIG_LEVELS);
            Octwig twig = r->twig[t.offset()];
            for (int y = 0; y < TWIG_SIZE; ++y)
                for (int z = 0; z < TWIG_SIZE; ++z)
                    for (int x = 0; x < TWIG_SIZE; ++x)
                        if (twig.leafmap[Octwig::word(x, y, z)] & (1 << Octwig::bit(x, y, z)))
                            m->cubeface(pos + vec3(x, y, z) * leafSize, vec3(leafSize, leafSize, leafSize));
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

OctreeCubefaceDrawer::~OctreeCubefaceDrawer()
{
    glDeleteProgram(shader);
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
}

void OctreeCubefaceDrawer::loadTree(const Ocroot *root)
{
    // Load the tree into a mesh
    cubeTree(root, root->tree[0], root->position, root->size, &this->mesh);
}

void OctreeCubefaceDrawer::loadGL(const char *texture)
{
    vao = shader = tex = 0;

    // VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Shader
    this->shader = glCreateProgram();
    Shader::compileAttach("shaders/cube.vrtx.glsl", GL_VERTEX_SHADER, shader);
    Shader::compileAttach("shaders/cube.frag.glsl", GL_FRAGMENT_SHADER, shader);
    Shader::link(shader);

    this->mvp = glGetUniformLocation(shader, "mvp");
    assert(mvp != -1);
    this->sampler = glGetUniformLocation(shader, "samp");
    assert(sampler != -1);

    // Vertices
    mesh.bind();

    // Texture
    glGenTextures(1, &this->tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->tex);

    SDL_Surface *vt = IMG_Load(texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vt->w, vt->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, vt->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    SDL_FreeSurface(vt);
}

void OctreeCubefaceDrawer::draw(const glm::mat4 MVP)
{
    glBindVertexArray(vao);
    glUseProgram(shader);

    glUniformMatrix4fv(mvp, 1, GL_FALSE, glm::value_ptr(MVP));
    glUniform1i(sampler, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, (void *)0);

    glUseProgram(0);
    glBindVertexArray(0);
}
