#include <time.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
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
#include "Traverse.h"
#include "GBuffer.h"
#include "Shader.h"
#include "World.h"
#include "Worm.h"

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
Text text;
World world;
GBuffer gbuffer;
Counter sw;

using glm::vec3;

void computeTarget(const World *world);
void computeMVP();
void initialize();
void deinitialize();
void initializeControls();
void glCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *argp);

int main() 
{
    using glm::vec3;

    initialize();

    initializeControls();

    text.init("C:/Windows/Fonts/arial.ttf", 18, width, height);

    imag.init(glm::vec3(3.0, 1.0, 0.5));

    world.init(5, 5, 5, 128);
    print(&world.chunk[0]);
    world.load_gpu();

    gbuffer.init(width, height);

    gbuffer.enable();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  

    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xff);
    glClearStencil(0);
    glStencilFunc(GL_NOTEQUAL, 1, 0xff);
    glStencilOp(GL_KEEP, GL_INCR, GL_INCR);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW); 

    int sbits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &sbits);
    // assert(sbits > 0);

    gbuffer.disable();

    Worm worm;
    worm.init();

    Counter frame, textframe;
    textframe.start();
    frame.start();

    while (running) 
    {
        input.poll();

        computeMVP();

        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        gbuffer.enable();

        glClearColor(0.1f, .4f, .8f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        int culled = 0;
        world.draw(mvp, position);

        glDisable(GL_CULL_FACE);
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glDepthFunc(GL_LESS);
        computeTarget(&world);
        if (imag.real) 
            imag.draw(mvp);

        worm.draw(mvp);

        gbuffer.disable();
        gbuffer.draw();

        if (textframe.elapsed() > 1. / 24)
        {
            double avg = frame.avg();
            text.clear();
            text.printf("frame time: %lfms, fps: %lf", avg * 1000, 1.0 / avg);
            text.printf("w: %d, h: %d", width, height);
            text.printf("fov: %f, h: %f, v: %f", fov, glm::degrees(horzAngle), glm::degrees(vertAngle));
            text.printf("x: %f, y: %f, z: %f, ", position.x, position.y, position.z);
            text.printf("speed: %f", speed);
            text.printf("culled/trees: %d/%d = %f%%", culled, world.volume, (float)culled * 100 / world.volume);

            textframe.restart();
        }
        glDisable(GL_DEPTH_TEST);
        text.draw();

        SDL_GL_SwapWindow(window);

        frame.restart();
    }

    world.deinit();
    gbuffer.deinit();
    imag.deinit();
    text.deinit();
    deinitialize();

    return 0;
}

void computeTarget(const World *w)
{
    vec3 sigma = vec3(0);
    imag.real = chunkmarch(position, direction, w, &sigma);
    imag.position(sigma);
}

template <typename F>
void modify(const ImagCube *imag, World *world, F f)
{
    using glm::ivec3;

    if (!imag->real) return;

    for (int i = 0; i < 8; ++i)
    {
        vec3 p = imag->bmin + vec3(glm::bvec3(i & 4, i & 2, i & 1)) * imag->scale;
        ivec3 q = world->index_float(p);
        int j = world->index(q.x, q.y, q.z);

        if (!isInsideCube(p, world->chunk[j].position, world->chunk[j].position + (float)world->chunksize)) continue;

        f(j);
    }
}

void destroy()
{
    auto dd = [&](int i) { 
        Ocdelta deltatree, deltatwig;
        world.chunk[i].destroy(imag.bmin, imag.bmin + imag.scale, &deltatree, &deltatwig);
        world.modify(i, &deltatree, &deltatwig);
    };
    modify(&imag, &world, dd);
}

void build()
{
    auto db = [&](int i) { 
        Ocdelta deltatree, deltatwig;
        world.chunk[i].build(imag.bmin, imag.bmin + imag.scale, 5, &deltatree, &deltatwig);
        world.modify(i, &deltatree, &deltatwig);
    };
    modify(&imag, &world, db);
}

void replace()
{
    auto dr = [&](int i) { 
        Ocdelta deltatree, deltatwig;
        world.chunk[i].replace(imag.bmin, imag.bmin + imag.scale, 5, &deltatree, &deltatwig);
        world.modify(i, &deltatree, &deltatwig);
    };
    modify(&imag, &world, dr);
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
    const mat4 R = I;
    const mat4 SRT = T * S * R;
    const mat4 M = SRT;
    const mat4 P = glm::perspective(glm::radians(fov/2), (float)width / height, .1f, 10000.f);
    const mat4 V = glm::lookAt(position, position + direction, up);
    const mat4 MVP = P * V * M;

    mvp = MVP;
}

void initialize()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        die("SDL_Init: %s\n", SDL_GetError());

    if (TTF_Init() < 0)
        die("TTF_Init: %s\n", TTF_GetError());

    if (IMG_Init(IMG_INIT_PNG) < 0)
        die("IMG_Init: %s\n", IMG_GetError());

    assert(!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4));
    assert(!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
    assert(!SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8));

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
    glDebugMessageCallback(glCallback, NULL);

    /*
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  

    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xff);
    glClearStencil(0);
    glStencilFunc(GL_NOTEQUAL, 1, 0xff);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW); 

    int sbits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &sbits);
    */
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
    input.bind(SDL_QUIT, [&]() { running = false; });
    input.bindKey(SDLK_UP, [&]() { speed = (float)glm::clamp(speed * 2.0, 0.001, 100.0); });
    input.bindKey(SDLK_DOWN, [&]() { speed = (float)glm::clamp(speed / 2.0, 0.001, 100.0); });
    input.bindKey('w', [&]() { position += direction * speed; });
    input.bindKey('s', [&]() { position -= direction * speed; });
    input.bindKey('a', [&]() { position -= right * speed; });
    input.bindKey('d', [&]() { position += right * speed; });
    input.bindKey('+', [&]() { imag.scale += 0.5; });
    input.bindKey('-', [&]() { imag.scale = glm::max(imag.scale - 0.5f, 0.0f); });
    input.bindKey('x', [&]() { destroy(); });
    input.bindKey('z', [&]() { build(); });
    input.bindKey('c', [&]() { replace(); });
    input.bindKey('g', [&]() { 
        // Ocroot r = world.chunk[0].defragcopy();
        Ocroot r = world.chunk[0].lodmm();
        print(&r);
        world.chunk[0] = r;
        Ocdelta tree(true), twig(true);
        world.modify(0, &tree, &twig);
    });
    input.bindKey('1', [&]() { world.shift(glm::ivec3(1, 0, 0)); });
    input.bindKey('2', [&]() { world.shift(glm::ivec3(-1, 0, 0)); });
    input.bindKey('3', [&]() { world.shift(glm::ivec3(0, 1, 0)); });
    input.bindKey('4', [&]() { world.shift(glm::ivec3(0, -1, 0)); });
    input.bindKey('5', [&]() { world.shift(glm::ivec3(0, 0, 1)); });
    input.bindKey('6', [&]() { world.shift(glm::ivec3(0, 0, -1)); });
    input.bindKey(SDLK_SPACE, [&]() { position += glm::vec3(0.f, 1.f, 0.f) * speed; });
    input.bindKey(SDLK_LSHIFT, [&]() { position -= glm::vec3(0.f, 1.f, 0.f) * speed; });
    input.bindKey(SDLK_ESCAPE, [&]() { running = false; });

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

    die("OpenGL: type = 0x%x, severity = 0x%x, message = %s\n", type, severity, message);
}
