#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Mesh.h"

Mesh::Mesh()
{
    this->vao = this->vbo[0] = this->vbo[1] = this->ebo = this->tex = 0;
}

Mesh::~Mesh()
{
    if (!this->vertices.size())
        return;

    glDeleteVertexArrays(1, &this->vao);
    glDeleteBuffers(2, this->vbo);
    glDeleteBuffers(1, &this->ebo);
}

void Mesh::finalize()
{
    if (!this->vertices.size())
        return;

    glGenVertexArrays(1, &this->vao);
    glBindVertexArray(this->vao);

    glGenBuffers(2, this->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, this->vertices.size() * sizeof(float), this->vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, this->vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, this->uv.size() * sizeof(float), this->uv.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(1);

    glGenBuffers(1, &this->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size() * sizeof(unsigned short), this->indices.data(), GL_STATIC_DRAW);

    glGenTextures(1, &this->tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->tex);

    SDL_Surface *vt = IMG_Load("textures/quad.png");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vt->w, vt->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, vt->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);
    SDL_FreeSurface(vt);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

template<typename T, typename V, typename C>
size_t bs(const T& v, size_t n, V x, C c)
{
    size_t h = n-1, l = 0, m = (l + h) / 2;
    while (l < h)
    {
        int f = c(v[m], x);
        if (f == 0) return m;
        if (f < 0) h = m-1;
        if (f > 0) l = m+1;
    }
    return m;
}

void Mesh::optimize(float gran)
{
    assert(this->vertices.size() % 3 == 0);
    assert(this->uv.size() % 2 == 0);
    assert(this->indices.size() % 3 == 0);

    (void)gran;

    using std::vector;
    auto coordinates = this->vertices.size();
    auto points = coordinates / 3;

    auto nv = vector<float>();
    auto nu = vector<float>();
    auto ni = vector<unsigned short>();
    auto im = new unsigned short[points];

    printf("Should set %llu\n", points);

    for (size_t i = 0; i < points; ++i)
    {
        assert(i*3+2 < this->vertices.size());
        assert(i*2+1 < this->uv.size());

        float x = this->vertices[i*3+0];
        float y = this->vertices[i*3+1];
        float z = this->vertices[i*3+2];
        float u = this->uv[i*2+0];
        float v = this->uv[i*2+1];

        printf("%llu    %f %f %f    %f %f\n", i, x, y, z, u, v);

        bool found = false;
        auto moved = nv.size() / 3;
        for (size_t j = 0; j < moved; ++j)
        {
            assert(j*3+2 < nv.size());
            assert(j*2+1 < nu.size());

            float a = nv[j*3+0];
            float b = nv[j*3+1];
            float c = nv[j*3+2];
            float d = nu[j*2+0];
            float e = nu[j*2+1];

            if (a == x && b == y && c == z && d == u && e == v)
            {
                printf("match %llu    %f %f %f    %f %f\n", j, a, b, c, d, e);
                printf("Set %llu\n", i);
                im[i] = j;
                found = true;
                break;
            }
        }

        if (!found)
        {
            printf("add\n");

            nv.push_back(x);
            nv.push_back(y);
            nv.push_back(z);

            nu.push_back(u);
            nu.push_back(v);

            printf("Set %llu\n", i);
            im[i] = i;
        }
    }

    for (size_t i = 0; i < this->indices.size(); ++i)
        ni.push_back(im[this->indices[i]]);

    this->vertices = nv;
    this->uv = nu;
    this->indices = ni;

    delete[] im;

    assert(this->vertices.size() % 3 == 0);
    assert(this->uv.size() % 2 == 0);
    assert(this->indices.size() % 3 == 0);
}

void Mesh::add(const float *vertex, 
               size_t vertices,
               const float translate[3],
               const float scale[3],
               const float *uv, 
               const unsigned short *index,
               size_t indices)
{
    assert(this->vertices.size() % 3 == 0);
    assert(this->uv.size() % 2 == 0);
    assert(this->indices.size() % 3 == 0);

    size_t offset = this->vertices.size() / 3;
    size_t coords = vertices * 3;

    this->vertices.reserve(this->vertices.size() + vertices);
    for (size_t i = 0; i < coords; ++i)
        this->vertices.push_back((vertex[i] * scale[i % 3]) + translate[i % 3]);

    this->uv.insert(this->uv.end(), uv, uv + vertices * 2);

    this->indices.reserve(this->indices.size() + indices);
    for (size_t i = 0; i < indices; ++i)
        this->indices.push_back(index[i] + offset);

    assert(this->vertices.size() % 3 == 0);
    assert(this->uv.size() % 2 == 0);
    assert(this->indices.size() % 3 == 0);
}

/* aQuadVertices: flat quad when looking straight at a-axis
 * Vertices should be specified in counter-clockwise order!
 */

const float zQuadVertices[12] = {
    0.0, 0.0, 0.0, // Bottom left
    0.0, 1.0, 0.0, // Top left
    1.0, 0.0, 0.0, // Bottom right
    1.0, 1.0, 0.0, // Top right
};

const float yQuadVertices[12] = {
    0.0, 0.0, 0.0, // Bottom left
    0.0, 0.0, 1.0, // Top left
    1.0, 0.0, 0.0, // Bottom right
    1.0, 0.0, 1.0, // Top right
};

const float xQuadVertices[12] = {
    0.0, 0.0, 0.0, // Bottom left
    0.0, 1.0, 0.0, // Top left
    0.0, 0.0, 1.0, // Bottom right
    0.0, 1.0, 1.0, // Top right
};

const float quadUV[8] = {
    0.0, 1.0, // Top left
    0.0, 0.0, // Bottom left
    1.0, 1.0, // Top right
    1.0, 0.0, // Bottom right
};

const unsigned short quadIndices[6] = {
    3, 1, 2,
    0, 2, 1
};

const float cubeVertices[24] = {
    0.0, 0.0, 0.0, // Near bottom left
    1.0, 0.0, 0.0, // Near bottom right
    0.0, 1.0, 0.0, // Near top left,
    1.0, 1.0, 0.0, // Near top right
    1.0, 0.0, 1.0, // Far bottom right
    1.0, 1.0, 1.0, // Far top right
    0.0, 0.0, 1.0, // Far bottom left
    0.0, 1.0, 1.0, // Far top left,
};

const float cubeUV[16] = {
    0.0, 0.0,
    0.25, 0.0,
    0.0, 0.25,
    0.25, 0.25,
    0.5, 0.0,
    0.5, 0.5,
    0.75, 0.0,
    0.75, 0.75
};

const unsigned short cubeIndices[] {
    0, 1, 3, // Front bottom
    0, 3, 2, // Front top
    /*
    1, 4, 5, // Right bottom
    1, 5, 2, // Right top
    4, 5, 6, // Far bottom
    6, 7, 4, // Far top
    6, 0, 2, // Left bottom
    6, 2, 7, // Left top
    2, 3, 5, // Top bottom,
    2, 5, 7, // Top top
    0, 1, 4, // Bottom bottom,
    0, 4, 6, // Bottom top
    */
};

void addCube(Mesh& m, const glm::vec3& offset, const glm::vec3& size)
{
    using glm::vec3;
    using glm::value_ptr;

    vec3 farX = offset + vec3(size.x, 0.f, 0.f);
    m.add(xQuadVertices, 4, value_ptr(offset), value_ptr(size), quadUV, quadIndices, 6);
    m.add(xQuadVertices, 4, value_ptr(farX), value_ptr(size), quadUV, quadIndices, 6);

    vec3 farY = offset + vec3(0.f, size.y, 0.f);
    m.add(yQuadVertices, 4, value_ptr(offset), value_ptr(size), quadUV, quadIndices, 6);
    m.add(yQuadVertices, 4, value_ptr(farY), value_ptr(size), quadUV, quadIndices, 6);

    vec3 farZ = offset + vec3(0.f, 0.f, size.z);
    m.add(zQuadVertices, 4, value_ptr(offset), glm::value_ptr(size), quadUV, quadIndices, 6);
    m.add(zQuadVertices, 4, value_ptr(farZ), glm::value_ptr(size), quadUV, quadIndices, 6);

   (void)xQuadVertices;
   (void)yQuadVertices;
   (void)zQuadVertices;
   (void)quadUV;
   (void)quadIndices;
   (void)cubeIndices;
   (void)cubeUV;
   (void)cubeVertices;

   // m.add(cubeVertices, 8, glm::value_ptr(offset), glm::value_ptr(size), cubeUV, cubeIndices, sizeof(cubeIndices) / sizeof(short));
}