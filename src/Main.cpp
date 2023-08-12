#include <time.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
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
#include "Skybox.h"
#include "Light.h"
#include "Camera.h"

#define FAR 8192.f
#define NEAR 0.125f

SDL_Window *window;
SDL_GLContext glContext;
bool running = true;
int width = 1920, height = 1080;
// glm::vec3 position = glm::vec3(0.f, 0.f, 0.f);
// float vertAngle = 0.f, horzAngle = 0.f;
// float fov = 90.f;
float speed = 4.8f, sensitivity = 0.4f;
int frameCount = 0;
// glm::vec3 direction, right, up;
// glm::mat4 mvp;
Input input;
ImagCube imag;
Text text;
World world;
GBuffer gbuffer;
Counter sw;
PerspectiveCamera camera;
bool cursor = false;

using glm::mat4;
using glm::vec3;

void computeTarget(const World *world);
// void computeMVP(mat4 *p, mat4 *v);
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

    world.init(4, 4, 4, 128);
    print(&world.chunk[0]);
    world.load_gpu();

    gbuffer.init(width, height);

    gbuffer.enable();
    // glEnable(GL_DEPTH_TEST);
    // glDepthFunc(GL_LESS);

    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  

    /*
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xff);
    glClearStencil(0);
    glStencilFunc(GL_NOTEQUAL, 1, 0xff);
    glStencilOp(GL_KEEP, GL_INCR, GL_INCR);
    */

    /*
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW); 
    */

    int sbits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &sbits);
    // assert(sbits > 0);

    int mts = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mts);
    printf("GL_MAX_TEXTURE_SIZE=%d\n", mts);

    gbuffer.disable();

    Worm worm;
    worm.init();

    Skybox skybox;
    skybox.init();

    Counter frame, textframe;
    textframe.start();
    frame.start();

    unsigned int bark_ssbo = 0;
    glGenBuffers(1, &bark_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bark_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4, NULL, GL_STATIC_DRAW); 
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    PointLight pointLight;
    pointLight.position = vec3(50, 8, 65);
    pointLight.color = vec3(0.8, 0.6, 0.2);
    pointLight.ambient = vec3(0.2);
    pointLight.diffuse = vec3(0.5);
    pointLight.specular = vec3(1);
    pointLight.constant = 1.0;
    pointLight.linear = 0.14;
    pointLight.quadratic = 0.09;

    PointLightContext pointLightContext;
    pointLightContext.init();

    DirectionalLight directionalLight;
    directionalLight.direction = glm::normalize(vec3(-0.2, -1.0, -0.3));
    directionalLight.ambient = vec3(0.6, 0.6, 0.8);
    directionalLight.diffuse = vec3(0.3, 0.3, 0.6);
    directionalLight.specular = vec3(0);

    Spotlight spotlight;
    spotlight.position = vec3(50, 20, 70);
    spotlight.direction = glm::normalize(vec3(0.1, -1.0, -0.1));
    spotlight.ambient = vec3(0.2, 0.8, 0.3);
    spotlight.diffuse = vec3(0.2, 0.8, 0.3);
    spotlight.specular = vec3(1);
    spotlight.phi_deg = 25.0;
    spotlight.gamma_deg = 35.0;
    spotlight.constant = 1.0;
    spotlight.linear = 0.045;
    spotlight.quadratic = 0.0075;

    camera.position = vec3(0);
    camera.direction = vec3(0, 0, 1);
    camera.right = vec3(-1, 0, 0);
    camera.up = vec3(0, 1, 0);
    camera.near = NEAR;
    camera.far = FAR;
    camera.fov_deg = 90;
    camera.width = width;
    camera.height = height;
    camera.yaw_deg = 0;
    camera.pitch_deg = 0;
    camera.roll_deg = 0;

    Shadowmap shadowmap;
    shadowmap.init(width, height);

    OrthoCamera dcamera;
    dcamera.direction = directionalLight.direction;
    dcamera.near = NEAR;
    dcamera.far = FAR;
    dcamera.width = width;
    dcamera.height = height;

    while (running) 
    {
        input.poll();

        mat4 p = camera.proj();
        mat4 v = camera.view();
        mat4 mvp = p * v;

        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gbuffer.enable();
        glDisable(GL_FRAMEBUFFER_SRGB);

        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        int culled = 0;

        float t = (double)clock() / CLOCKS_PER_SEC;
        pointLight.position.x = 50.0 + cos(t) * 10.f;
        pointLight.position.z = 65.0 + sin(t) * 10.f;

        glUseProgram(world.shader);

        pointLight.bind(world.shader, "pointLight");
        directionalLight.bind(world.shader, "directionalLight");
        spotlight.bind(world.shader, "spotlight");

        world.draw(mvp, camera.position);
        pointLightContext.draw(mvp, pointLight.position, pointLight.color);
        pointLightContext.draw(mvp, spotlight.position, spotlight.diffuse);

        skybox.draw(v, p);

        glDisable(GL_STENCIL_TEST);

        glDisable(GL_CULL_FACE);
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        computeTarget(&world);
        if (imag.real) 
            imag.draw(mvp);

        worm.draw(mvp);

        gbuffer.disable();
        gbuffer.draw();
        glDisable(GL_FRAMEBUFFER_SRGB);

        if (textframe.elapsed() > 1. / 24)
        {

            double avg = frame.avg();
            text.clear();
            text.printf("frame time: %lfms, fps: %lf", avg * 1000, 1.0 / avg);
            text.printf("width: %d, height: %d", width, height);
            text.printf("fov: %f, yaw: %f, pitch: %f, roll: %f", camera.fov_deg, camera.yaw_deg, camera.pitch_deg, camera.roll_deg);
            text.printf("x: %f, y: %f, z: %f, ", camera.position.x, camera.position.y, camera.position.z);
            text.printf("speed: %f", speed);
            text.printf("grid size: %dx%dx%d = %d", world.width, world.height, world.depth, world.volume);
            text.printf("culled/trees: %d/%d = %f%%", culled, world.volume, (float)culled * 100 / world.volume);
            {
                using std::string;
                using std::to_string;

                auto bytesize = [&](int64_t B) -> string
                {
                    constexpr int64_t K = 1024, M = K*K, G = M*K, T = G*K;
                    if (B >= T) return to_string(B / T) + " TB";
                    if (B >= G) return to_string(B / G) + " GB";
                    if (B >= M) return to_string(B / M) + " MB";
                    if (B >= K) return to_string(B / K) + " KB";
                    return to_string(B) + " B";
                };

                auto allocator_to_string = [&](Allocator a) -> string
                {
                    string s = "" + to_string(a.regions) + " buffers; size: ";
                    for (int i = 0; i < a.regions; ++i)
                        s += bytesize(a.region[i].size) + (i == a.regions - 1 ? "" : ", ");
                    s += "; used: ";
                    for (int i = 0; i < a.regions; ++i)
                        s += "~" + to_string(a.region[i].count * 100 / a.region[i].size) + "%%" + (i == a.regions - 1 ? "" : ", ");
                    return s;
                };

                int buffers = world.allocator.tree.regions + world.allocator.twig.regions;
                string s = string("") 
                         + to_string(buffers) 
                         + " buffers total | tree: (" 
                         + allocator_to_string(world.allocator.tree)
                         + ") | twig: ("
                         + allocator_to_string(world.allocator.twig)
                         + ")";
                text.printf(s.c_str());
            }

            textframe.restart();
        }
        glDisable(GL_DEPTH_TEST);
        text.draw();

        SDL_GL_SwapWindow(window);

        frame.restart();
    }

    shadowmap.release();
    pointLightContext.release();
    skybox.release();
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
    imag.real = chunkmarch(camera.position, camera.direction, w, &sigma);
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

using glm::mat4;

/*
void computeMVP(mat4 *p, mat4 *v)
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
    const mat4 P = glm::perspective(fov/2, (float)width / height, NEAR, FAR);
    const mat4 V = glm::lookAt(position, position + direction, up);
    const mat4 MVP = P * V * M;

    *p = P;
    *v = V;

    mvp = MVP;
}
*/

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
    input.bindKey('w', [&]() { camera.position += camera.direction * speed; });
    input.bindKey('s', [&]() { camera.position -= camera.direction * speed; });
    input.bindKey('a', [&]() { camera.position -= camera.right * speed; });
    input.bindKey('d', [&]() { camera.position += camera.right * speed; });
    input.bindKey('+', [&]() { imag.scale += 0.5; });
    input.bindKey('-', [&]() { imag.scale = glm::max(imag.scale - 0.5f, 0.0f); });
    input.bindKey('x', [&]() { destroy(); });
    input.bindKey('z', [&]() { build(); });
    input.bindKey('c', [&]() { replace(); });
    input.bindKey('g', [&]() { 
        // Ocroot r = world.chunk[0].defragcopy();
        using glm::ivec3;
        ivec3 ip = world.index_float(camera.position);
        int i = world.index(ip.x, ip.y, ip.z);
        Ocroot r = world.chunk[i].lodmm();
        print(&r);
        world.chunk[i] = r;
        Ocdelta tree(true), twig(true);
        world.modify(i, &tree, &twig);
    });
    input.bindKey('1', [&]() { world.shift(glm::ivec3(1, 0, 0)); });
    input.bindKey('2', [&]() { world.shift(glm::ivec3(-1, 0, 0)); });
    input.bindKey('3', [&]() { world.shift(glm::ivec3(0, 1, 0)); });
    input.bindKey('4', [&]() { world.shift(glm::ivec3(0, -1, 0)); });
    input.bindKey('5', [&]() { world.shift(glm::ivec3(0, 0, 1)); });
    input.bindKey('6', [&]() { world.shift(glm::ivec3(0, 0, -1)); });
    input.bindKey(SDLK_SPACE, [&]() { camera.position += glm::vec3(0.f, 1.f, 0.f) * speed; });
    input.bindKey(SDLK_LSHIFT, [&]() { camera.position -= glm::vec3(0.f, 1.f, 0.f) * speed; });
    input.bindKey(SDLK_ESCAPE, [&]() { running = false; });

    input.bindKey(SDLK_TAB, [&]() { 
        SDL_ShowCursor(SDL_ENABLE);
        SDL_SetRelativeMouseMode(SDL_FALSE);
        cursor = true;
    });

    input.bind(SDL_MOUSEBUTTONDOWN, [&](SDL_Event&e) {
        if (e.button.button == SDL_BUTTON_LEFT) 
        {
            SDL_ShowCursor(SDL_DISABLE);
            SDL_SetRelativeMouseMode(SDL_TRUE);
            cursor = false;
        }
        else if (e.button.button == SDL_BUTTON_X1)
            camera.roll_deg -= (float)sensitivity * 50;
        else if (e.button.button == SDL_BUTTON_X2)
            camera.roll_deg += (float)sensitivity * 50;
    });

    input.bind(SDL_MOUSEMOTION, [&](SDL_Event& e) {
        if (!cursor) {
            camera.yaw_deg -= (float)e.motion.xrel * sensitivity;
            camera.pitch_deg += (float)e.motion.yrel * sensitivity;
        }
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
