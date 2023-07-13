#include <time.h>
#include <stdlib.h>
#include <vector>
#include <windows.h>
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/noise.hpp>
#include "Util.h"
#include "Text.h"
#include "Octree.h"
#include "Input.h"
#include "BoundsPyramid.h"
#include "Draw.h"
#include "Debug.h"
#include "ImaginaryCube.h"

SDL_Window *window;
SDL_GLContext glContext;
bool running = true;
int width = 1920, height = 1080;
glm::vec3 position = glm::vec3(0.f, 0.f, 0.f);
float vertAngle = 0.f, horzAngle = 0.f;
float fov = 90.f;
float speed = 4.8f, sensitivity = 0.004f;
int frameCount = 0;
glm::vec3 direction, right, up;
glm::mat4 mvp;
Input input;
ImagCube imag;

using glm::vec3;

bool chunkmarch(vec3 alpha, vec3 beta, const Ocroot *chunks, vec3 *sigma);
void computeTarget(const Ocroot *chunks);
void computeMVP();
void initialize();
void deinitialize();
void initializeControls();
void glCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *argp);

bool isFile(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    fclose(fp);
    return true;
}

void makeTree(Ocroot *root, glm::vec3 pos, float size, int depth, BoundsPyramid *pyr)
{
    grow(root, pos, size, depth, pyr);

    /*
    char path[256] = {0};
    sprintf(path, "%d.tree", depth);
    if (isFile(path))
    {
        propagate(root, path);
    }
    else
    {
        grow(root, vec3(0, 0, 0), 128, depth, pyr);
        pollinate(root, path);
    }
    */
}

int main() 
{
    using glm::vec3;

    Counter sw;

    srand((unsigned int)time(NULL));

#define TREES_WIDTH 5
#define TREES (TREES_WIDTH * TREES_WIDTH)
    int depth = 7;
    int pyramidepth = 256;
    // Height pyramid for world gen
    SW_START(sw, "Generating bounded pyramid");
    BoundsPyramid pyramid[TREES];
    for (int i = 0; i < TREES_WIDTH; ++i)
        for (int j = 0; j < TREES_WIDTH; ++j)
            pyramid[i * TREES_WIDTH + j].init(pyramidepth, 16.0f, 1.0f / pyramidepth, (float)i*pyramidepth, 16.0f, (float)j*pyramidepth);
    SW_STOP(sw);
    print(&pyramid[0]);

#define TREE_SIZE 128
    // Tree
    SW_START(sw, "Generating octree");
    Ocroot root[TREES];
    for (int i = 0; i < TREES_WIDTH; ++i)
        for (int j = 0; j < TREES_WIDTH; ++j)
        makeTree(&root[i * TREES_WIDTH + j], vec3(TREE_SIZE * i, 0.0, TREE_SIZE * j), TREE_SIZE, depth, &pyramid[i * TREES_WIDTH + j]);
    SW_STOP(sw);

    print(&root[0]);

    // Mesh
    SW_START(sw, "Generating mesh");
    // OctreeCubefaceDrawer d;
    // OctreeCubemapDrawer d;
    ParallaxDrawer d[TREES];
    for (int i = 0; i < TREES; ++i)
        d[i].loadTree(&root[i]); 
    SW_STOP(sw);
    // print(&d[0].mesh);

    // OpenGL, SDL, etc.
    SW_START(sw, "Initializing OpenGL and SDL");
    initialize();
    initializeControls();
    Text text;
    text.init("C:/Windows/Fonts/arial.ttf", 18, width, height);
    SW_STOP(sw);

    // Load mesh into openGL
    SW_START(sw, "Uploading mesh to GPU");
    for (int i = 0; i < TREES; ++i)
        d[i].loadGL("textures/quad.png");
    SW_STOP(sw);

    imag.init(1.0);

    Counter frame, textframe;
    textframe.start();
    frame.start();

    while (running) 
    {
        input.poll();

        computeMVP();

        glClearColor(0.3f, 0.3f, 0.6f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // d.draw(mvp);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        d[0].pre(mvp, position);
        for (int i = 0; i < TREES; ++i)
            d[i].draw();
        d[0].post();

        glDisable(GL_DEPTH_TEST);
        computeTarget(root);
        if (imag.real) imag.draw(mvp);

        if (textframe.elapsed() > 1. / 24)
        {
            double avg = frame.avg();
            text.clear();
            text.printf("frame time: %lfms, fps: %lf", avg * 1000, 1.0 / avg);
            text.printf("w: %d, h: %d", width, height);
            text.printf("fov: %f, h: %f, v: %f", fov, glm::degrees(horzAngle), glm::degrees(vertAngle));
            text.printf("x: %f, y: %f, z: %f, ", position.x, position.y, position.z);
            text.printf("speed: %f", speed);

            textframe.restart();
        }
        glDepthFunc(GL_ALWAYS);
        text.draw();

        SDL_GL_SwapWindow(window);

        frame.restart();
    }

    imag.deinit();
    text.deinit();
    deinitialize();

    return 0;
}

void computeTarget(const Ocroot *chunks)
{
    vec3 sigma = vec3(0);
    imag.real = chunkmarch(position, direction, chunks, &sigma);
    imag.position(sigma);
}

void computeMVP()
{
    using glm::mat4;
    using glm::vec3;

    const float cv = cos(vertAngle);
    const float sv = sin(vertAngle);
    const float ch = cos(horzAngle);
    const float sh = sin(horzAngle);

    direction = glm::normalize(vec3(cv * sh, sv, cv * ch));
    right     = vec3(sin(horzAngle - M_PI/2.f), 0, cos(horzAngle - M_PI/2.f));
    up        = glm::cross(right, direction);

    const mat4 I = mat4(1.f);
    const mat4 T = I; 
    const mat4 S = I;
    const mat4 R = glm::rotate(I, glm::radians(0.f), up);
    const mat4 SRT = T * S * R;
    const mat4 M = SRT;
    const mat4 P = glm::perspective(glm::radians(fov/2), (float)width / height, .1f, 10000.f);
    const mat4 V = glm::lookAt(position, position + direction, up);
    const mat4 MVP = P * V * M;

    mvp = MVP;
}

using glm::vec3;
using glm::bvec3;
using glm::ivec3;
using glm::greaterThanEqual;
using glm::all;
using glm::min;
using glm::max;

#define EPS (1.0f / 8192.0)

struct Tree
{
    vec3     bmin;
    float    size;
    uint64_t offset;

    Tree(vec3 p, float s, uint64_t i): bmin(p), size(s), offset(i) {}
};

bool isInsideCube(vec3 p, vec3 cmin, vec3 cmax) 
{
    bvec3 geq = greaterThanEqual(p, cmin);
    bvec3 leq = greaterThanEqual(cmax, p);
    return all(geq) && all(leq);
}

float cubeEscapeDistance(vec3 a, vec3 b, vec3 cmin, vec3 cmax) 
{
    vec3 gamma = vec3(1.0 / b.x, 1.0 / b.y, 1.0 / b.z);
    vec3 tmin = (cmin - a) * gamma;
    vec3 tmax = (cmax - a) * gamma;
    vec3 t = max(tmin, tmax);
    return min(t.x, min(t.y, t.z));
}

Tree traverse(vec3 p, const Ocroot *root)
{
    Tree t = Tree(root->position, root->size, 0);
    for ( ; ; )
    {
        if (root->tree[t.offset].type() != BRANCH) return t;
        float halfsize = t.size * 0.5f;
        vec3 mid = t.bmin + halfsize;
        bvec3 ge = greaterThanEqual(p, mid);
        vec3 bmin = t.bmin + (vec3)ge * halfsize;
        uint64_t i = Octree::branch(ge.x, ge.y, ge.z);
        uint64_t next = root->tree[t.offset].offset() + i;
        t = Tree(bmin, halfsize, next);
    }
}

bool twigmarch(vec3 a, vec3 b, vec3 bmin, float size, float leafsize, const Octwig *twig, float *s)
{
    vec3 bmax = bmin + size;
    float t = 0.0;
    for ( ; ; )
    {
        vec3 p = a + b * t;
        if (!isInsideCube(p, bmin, bmax)) return false;
        ivec3 off = ivec3((p - bmin) / leafsize);
        if (!isInsideCube(off, vec3(0), vec3(TWIG_SIZE-1))) return false;
        uint32_t word = Octwig::word(off.x, off.y, off.z);
        if (twig->leaf[word] != 0)
        {
            *s = t - EPS;
            return true;
        }
        vec3 leafmin = bmin + vec3(off) * leafsize;
        vec3 leafmax = leafmin + leafsize;
        float escape = cubeEscapeDistance(p, b, leafmin, leafmax);
        t += escape + EPS;
    }
}

bool treemarch(vec3 a, vec3 b, const Ocroot *root, float *s)
{
    vec3 rmin = root->position;
    vec3 rmax = root->position + root->size;
    float t = 0.0;
    for ( ; ; )
    {
        vec3 p = a + b * t;
        if (!isInsideCube(p, rmin, rmax)) return false;

        Tree tree = traverse(p, root);
        uint32_t type = root->tree[tree.offset].type();
        if (type == EMPTY)
        {
            float escape = cubeEscapeDistance(p, b, tree.bmin, tree.bmin + tree.size);
            t += escape + EPS;
        }
        else if (type == LEAF)
        {
            *s = t - EPS;
            return true;
        }
        else if (type == TWIG)
        {
            float leafsize = tree.size / (1 << TWIG_LEVELS);
            if (twigmarch(p, b, tree.bmin, tree.size, leafsize, &root->twig[root->tree[tree.offset].offset()], s))
            {
                *s += t;
                return true;
            }
            float escape = cubeEscapeDistance(p, b, tree.bmin, tree.bmin + tree.size);
            t += escape + EPS;
        }
        else
        {
            assert(false);
        }
    }
}

float intersectCube(vec3 a, vec3 b, vec3 cmin, vec3 cmax, bool *intersect)
{
    vec3 tmin = (cmin - a) / b;
    vec3 tmax = (cmax - a) / b;
    vec3 t1 = min(tmin, tmax);
    vec3 t2 = max(tmin, tmax);
    float tnear = max(max(t1.x, t1.y), t1.z);
    float tfar = min(min(t2.x, t2.y), t2.z);
    *intersect = tfar > tnear;
    return tnear;
}

bool chunkmarch(vec3 alpha, vec3 beta, const Ocroot *chunks, vec3 *sigma)
{
    float chsize = TREE_SIZE;
    vec3 chmin = chunks[0].position;
    vec3 chmax = chunks[TREES-1].position + chsize;

    float t = 0.0f;
    bool intersect = true;
    if (!isInsideCube(alpha, chmin, chmax)) 
        t = intersectCube(alpha, beta, chmin, chmax, &intersect);
    if (!intersect)
        return false;

    for ( ; ; )
    {
        vec3 p = alpha + beta * t;
        if (!isInsideCube(p, chmin, chmax)) 
            return false;

        ivec3 chi = (p - chmin) / chsize;
        int i = chi.x * TREES_WIDTH + chi.z;
        if (i < 0 || i >= TREES) 
            return false;
        if (!isInsideCube(p, chunks[i].position, chunks[i].position + chsize))
            return false;

        float s = 0;
        if (treemarch(p, beta, &chunks[i], &s))
        {
            t += s;
            *sigma = alpha + beta * t;
            return true;
        }
        else
        {
            float escape = cubeEscapeDistance(p, beta, chunks[i].position, chunks[i].position + chsize);
            t += escape + EPS;
        }
    }
}

void initialize()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        die("SDL_Init: %s\n", SDL_GetError());

    if (TTF_Init() < 0)
        die("TTF_Init: %s\n", TTF_GetError());

    if (IMG_Init(IMG_INIT_PNG) < 0)
        die("IMG_Init: %s\n", IMG_GetError());


    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    window = SDL_CreateWindow(
        "Octree", 
        SDL_WINDOWPOS_CENTERED, 
        SDL_WINDOWPOS_CENTERED,
        width, 
        height,
        SDL_WINDOW_OPENGL
    );

    if (!window)
        die("SDL_CreateWindow: %s\n", SDL_GetError());

    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    glContext = SDL_GL_CreateContext(window);
    if (!glContext)
        die("SDL_GL_CreateContext: %s\n", SDL_GetError());
    SDL_GL_SetSwapInterval(0); 

    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK)
        die("glewInit: %s\n", glewGetErrorString(glewErr));

    glEnable(GL_DEBUG_OUTPUT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  

    glDebugMessageCallback(glCallback, NULL);
}

void deinitialize()
{
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

void initializeControls()
{
    input.bind(SDL_QUIT, [&]() { 
        running = false; 
    });
    input.bindKey(SDLK_UP, [&]() {
        speed = (float)glm::clamp(speed * 2.0, 0.001, 100.0);
    });
    input.bindKey(SDLK_DOWN, [&]() {
        speed = (float)glm::clamp(speed / 2.0, 0.001, 100.0);
    });
    input.bindKey('w', [&]() { 
        position += direction * speed; 
    });
    input.bindKey('s', [&]() { 
        position -= direction * speed; 
    });
    input.bindKey('a', [&]() { 
        position -= right * speed; 
    });
    input.bindKey('d', [&]() { 
        position += right * speed; 
    });
    input.bindKey(SDLK_SPACE, [&]() { 
        position += glm::vec3(0.f, 1.f, 0.f) * speed; 
    });
    input.bindKey(SDLK_LSHIFT, [&]() { 
        position -= glm::vec3(0.f, 1.f, 0.f) * speed; 
    });
    input.bindKey(SDLK_ESCAPE, [&]() {
        running = false; 
    });
    input.bindKey(SDLK_TAB, [&]() { 
        SDL_ShowCursor(SDL_ENABLE);
        SDL_SetRelativeMouseMode(SDL_FALSE);
    });
    input.bind(SDL_MOUSEBUTTONDOWN, [&](SDL_Event&e) {
        if (e.button.button != SDL_BUTTON_LEFT) return;
        SDL_ShowCursor(SDL_DISABLE);
        SDL_SetRelativeMouseMode(SDL_TRUE);
    });
    input.bind(SDL_MOUSEMOTION, [&](SDL_Event& e) {
        horzAngle -= (float)e.motion.xrel * sensitivity;
        vertAngle -= (float)e.motion.yrel * sensitivity;
        float orth = glm::radians(90.f);
        if (vertAngle >= orth) vertAngle = orth;
        if (vertAngle <= -orth) vertAngle = -orth;
    });
}

void glCallback(GLenum source, 
    GLenum type, 
    GLuint id, 
    GLenum severity, 
    GLsizei length, 
    const GLchar *message,
    const void *argp)
{
    (void)source;
    (void)id;
    (void)length;
    (void)argp;

    fprintf(stderr, "OpenGL: type = 0x%x, severity = 0x%x, message = %s\n", type, severity, message);
    if (severity == GL_DEBUG_TYPE_ERROR) 
        die("\n");
}
