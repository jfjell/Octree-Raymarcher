#include <time.h>
#include <stdlib.h>
#include <vector>
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

SDL_Window *window;
SDL_GLContext glContext;
bool running = true;
int width = 800, height = 600;
glm::vec3 position = glm::vec3(0.f, 0.f, 0.f);
float vertAngle = 0.f, horzAngle = 0.f;
float fov = 90.f;
float speed = .75f, sensitivity = 0.004f;
int frameCount = 0;
glm::vec3 direction, right, up;
glm::mat4 mvp;
Input input;

void computeMVP();
void initialize();
void deinitialize();
void initializeControls();
void glCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *argp);

int main() 
{
    using glm::vec3;

    Stopwatch sw;

    int depth = 9;
    // Height pyramid for world gen
    SW_START(sw, "Generating bounded pyramid");
    BoundsPyramid pyramid(1 << depth, 16, 1.0 / (1 << depth), 0, 0, 16);
    SW_STOP(sw);
    print(&pyramid);

    // Tree
    SW_START(sw, "Generating octree");
    Ocroot root;
    grow(&root, vec3(0, 0, 0), 128, depth, &pyramid);
    SW_STOP(sw);

    print(&root);

    // Mesh
    SW_START(sw, "Generating mesh");
    // OctreeCubefaceDrawer d;
    // OctreeCubemapDrawer d;
    ParallaxDrawer d;
    d.loadTree(&root); 
    SW_STOP(sw);
    print(&d.mesh);

    // OpenGL, SDL, etc.
    SW_START(sw, "Initializing OpenGL and SDL");
    initialize();
    initializeControls();
    Text text("C:/Windows/Fonts/arial.ttf", 18, width, height);
    SW_STOP(sw);

    // Load mesh into openGL
    SW_START(sw, "Uploading mesh to GPU");
    d.loadGL("textures/quad.png");
    SW_STOP(sw);

    Stopwatch theoframe, realframe, textframe;
    textframe.start();
    double real = 0, theo = 0;

    while (running) 
    {
        realframe.start();
        theoframe.start();

        input.poll();

        computeMVP();

        glClearColor(0.3, 0.3, 0.6, 1.);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // d.draw(mvp);
        d.draw(mvp, position);

        if (textframe.elapsed() > 1. / 24)
        {
            text.clear();
            text.printf("frame time: %lfs, theoretical fps: %lf, real fps: %lf", theo, 1.0 / theo, 1.0 / real);
            text.printf("w: %d, h: %d", width, height);
            text.printf("fov: %f, h: %f, v: %f", fov, glm::degrees(horzAngle), glm::degrees(vertAngle));
            text.printf("x: %f, y: %f, z: %f, ", position.x, position.y, position.z);

            textframe.start();
        }
        text.draw();

        theo = theoframe.stop();

        SDL_GL_SwapWindow(window);

        real = realframe.stop();
    }

    text.~Text();
    deinitialize();

    return 0;
}

void computeMVP()
{
    using glm::mat4;
    using glm::vec3;

    const float cv = cos(vertAngle);
    const float sv = sin(vertAngle);
    const float ch = cos(horzAngle);
    const float sh = sin(horzAngle);

    direction = vec3(cv * sh, sv, cv * ch);
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
    SDL_GL_SetSwapInterval(1); 

    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK)
        die("glewInit: %s\n", glewGetErrorString(glewErr));

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
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
    input.bind(SDL_QUIT, [&]() { running = false; });
    input.bindKey('w', [&]() { position += direction * speed; });
    input.bindKey('s', [&]() { position -= direction * speed; });
    input.bindKey('a', [&]() { position -= right * speed; });
    input.bindKey('d', [&]() { position += right * speed; });
    input.bindKey(SDLK_SPACE, [&]() { position += glm::vec3(0.f, 1.f, 0.f) * speed; });
    input.bindKey(SDLK_LSHIFT, [&]() { position -= glm::vec3(0.f, 1.f, 0.f) * speed; });
    input.bindKey(SDLK_ESCAPE, [&]() { running = false; });
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