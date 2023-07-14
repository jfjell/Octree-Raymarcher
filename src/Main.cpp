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
Counter sw;

using glm::vec3;

bool cullAngle(vec3 alpha, vec3 beta, vec3 bmin, vec3 bmax);
void computeTarget(const Ocroot *chunks);
void computeMVP();
void initialize();
void deinitialize();
void initializeControls();
void glCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *argp);
float manhattanCube(vec3 p, vec3 bmin, vec3 bmax);

#define TREES_X 3
#define TREES_Z 3
#define TREES_Y 1
#define TREES_T (TREES_X * TREES_Y * TREES_Z)
#define TREE_MAX_DEPTH 9
#define TREE_SIZE 128
#define PYRAMID_RESOLUTION 64

void generateWorld(Ocroot *root, BoundsPyramid *pyramid);

int main() 
{
    using glm::vec3;

    srand((unsigned int)time(NULL));

    BoundsPyramid pyramid[TREES_X * TREES_Z];
    Ocroot root[TREES_T];
    generateWorld(root, pyramid);

    // Mesh
    SW_START(sw, "Generating mesh");
    ParallaxDrawer d[TREES_T];
    for (int i = 0; i < TREES_T; ++i)
        d[i].loadTree(&root[i]); 
    SW_STOP(sw);

    // OpenGL, SDL, etc.
    SW_START(sw, "Initializing OpenGL and SDL");
    initialize();
    initializeControls();
    Text text;
    text.init("C:/Windows/Fonts/arial.ttf", 18, width, height);
    SW_STOP(sw);

    // Load mesh into openGL
    SW_START(sw, "Uploading mesh to GPU");
    for (int i = 0; i < TREES_T; ++i)
        d[i].loadGL("textures/quad.png");
    SW_STOP(sw);

    imag.init(1.0);

    Counter frame, textframe;
    textframe.start();
    frame.start();

    int order[TREES_T];
    for (int i = 0; i < TREES_T; ++i)
        order[i] = -1;

    GBuffer gbuffer;
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
    printf("sbits=%d\n", sbits);

    gbuffer.disable();

    while (running) 
    {
        input.poll();

        computeMVP();

        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        gbuffer.enable();

        glClearColor(1.f, 1.f, 1.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        for (int i = 0; i < TREES_T; ++i)
            order[i] = i;

        auto less = [&](int i, int j) -> bool {
            float di = manhattanCube(position, root[i].position, root[i].position + root[i].size);
            float dj = manhattanCube(position, root[j].position, root[j].position + root[j].size);
            return di < dj;
        };
        std::sort(order, &order[TREES_T], less);

        glEnable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glDepthFunc(GL_ALWAYS);
        d[0].pre(mvp, position);
        int culled = 0;
        for (int i = 0; i < TREES_T; ++i)
        {
            int j = order[i];
            d[j].draw();
        }
        d[0].post();

        glDisable(GL_CULL_FACE);
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        computeTarget(root);
        if (imag.real) 
            imag.draw(mvp);

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
            text.printf("culled/trees: %d/%d = %f%%", culled, TREES_T, (float)culled * 100 / TREES_T);

            textframe.restart();
        }
        glDisable(GL_DEPTH_TEST);
        text.draw();

        SDL_GL_SwapWindow(window);

        frame.restart();
    }

    gbuffer.deinit();
    imag.deinit();
    text.deinit();
    deinitialize();

    return 0;
}

void generateWorld(Ocroot *root, BoundsPyramid *pyramid)
{
    SW_START(sw, "Generating %d pyramids", TREES_Z * TREES_X);
    for (int z = 0; z < TREES_Z; ++z)
    {
        for (int x = 0; x < TREES_X; ++x)
        {
            int i = z * TREES_X + x;
            float amplitude = 16;
            float period = 1.0f / PYRAMID_RESOLUTION;
            float xshift = (float)x * PYRAMID_RESOLUTION;
            float yshift = 16.0f;
            float zshift = (float)z * PYRAMID_RESOLUTION;
            pyramid[i].init(PYRAMID_RESOLUTION, amplitude, period, xshift, yshift, zshift);

            printf("%d ", i);
        }
    }
    putchar('\n');
    SW_STOP(sw);
    print(&pyramid[0]);

    // Tree
    SW_START(sw, "Generating %d octrees", TREES_T);
    for (int z = 0; z < TREES_Z; ++z)
    {
        for (int y = 0; y < TREES_Y; ++y)
        {
            for (int x = 0; x < TREES_X; ++x)
            {
                int zm = abs(z - (TREES_Z / 2));
                int ym = abs(y - (TREES_Y / 2));
                int xm = abs(x - (TREES_X / 2));
                int dm = xm + ym + zm;
                dm = 0;

                int i = z * TREES_X * TREES_Y + y * TREES_X + x;
                int j = z * TREES_X + x;
                vec3 p = vec3(x * TREE_SIZE, (y - (TREES_Y / 2)) * TREE_SIZE, z * TREE_SIZE);
                unsigned int depth = glm::max(TREE_MAX_DEPTH - dm, TWIG_LEVELS+1);
                grow(&root[i], p, TREE_SIZE, depth, &pyramid[j]);

                printf("%d ", i);
            }
        }
    }
    putchar('\n');
    SW_STOP(sw);

    print(&root[0]);
}


float manhattanCube(vec3 p, vec3 bmin, vec3 bmax)
{
    vec3 bmid = (bmax + bmin) * 0.5f;
    return glm::distance(p, bmid);
}

bool cullAngle(vec3 alpha, vec3 beta, vec3 bmin, vec3 bmax)
{
    float theta1 = acos(glm::dot(glm::normalize(bmin - alpha), glm::normalize(beta)));
    float theta2 = acos(glm::dot(glm::normalize(bmax - alpha), glm::normalize(beta)));
    return glm::min(theta1, theta2) >= glm::radians(90.0f);
}

void computeTarget(const Ocroot *chunks)
{
    vec3 sigma = vec3(0);
    imag.real = chunkmarch(position, direction, chunks, TREES_T, vec3(1, TREES_X, TREES_Y * TREES_X), &sigma);
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
