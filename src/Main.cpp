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
#include "Shader.h"
#include "Util.h"
#include "Text.h"
#include "Mesh.h"
#include "Octree.h"
#include "Input.h"

SDL_Window *window;
SDL_GLContext glContext;
bool running = true;
int width = 800, height = 600;
glm::vec3 position = glm::vec3(0.f, 0.f, 0.f);
float vertAngle = 0.f, horzAngle = 0.f;
float fov = 90.f;
float speed = 0.75f, sensitivity = 0.004f;
int frameCount = 0;
glm::vec3 direction, right, up;
glm::mat4 mvp;
Input input;

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

    fprintf(stderr, "GL: type = 0x%x, severity = 0x%x, message = %s\n", type, severity, message);
    if (severity == GL_DEBUG_TYPE_ERROR) 
        die("\n");
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

void calculateMVP()
{
    float cv = cos(vertAngle), sv = sin(vertAngle), ch = cos(horzAngle), sh = sin(horzAngle);
    direction = glm::vec3(cv * sh, sv, cv * ch);
    right = glm::vec3(sin(horzAngle - M_PI/2.f), 0, cos(horzAngle - M_PI/2.f));
    up = glm::cross(right, direction);

    glm::mat4 model = glm::mat4(1.f); 
    glm::mat4 proj = glm::perspective(glm::radians(fov/2), (float)width / height, .1f, 10000.f);
    glm::mat4 view = glm::lookAt(position, position + direction, up);
    mvp = proj * view * model;
}

void drawText(Text& text, double frametime)
{
    if (frameCount % 8 == 0)
    {
        text.clear();
        text.printf("frame: %lfs, fps: %lf", frametime, 1.0 / frametime);
        text.printf("w: %d, h: %d", width, height);
        text.printf("fov: %f, h: %f, v: %f", fov, glm::degrees(horzAngle), glm::degrees(vertAngle));
        text.printf("x: %f, y: %f, z: %f, ", position.x, position.y, position.z);
    }
    text.draw();
}

void drawMesh(Mesh& mesh)
{
    if (!mesh.vertices.size())
        return;

    glBindVertexArray(mesh.vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mesh.tex);

    glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_SHORT, (void *)0);

    glBindVertexArray(0);
}

#define CHUNKS 4
#define CHUNKS3 ((CHUNKS)*(CHUNKS)*(CHUNKS))

Octree ** generateWorld()
{
    using glm::vec3;

    const int CHUNKSIZE = 16;
    const int DEPTH = 5;

    assert(CHUNKS % 2 == 0);

    Octree **chunk = new Octree*[CHUNKS3];

    int start = -CHUNKS / 2;
    int end = CHUNKS / 2;

    int i = 0;
    for (int x = start; x != end; ++x)
    {
        for (int y = start; y != end; ++y)
        {
            for (int z = start; z != end; ++z)
            {
                vec3 p = vec3((float)x, (float)y, (float)z) * (float)CHUNKSIZE;
                chunk[i++] = growTree(p, DEPTH, CHUNKSIZE, 16., 0.125, 0.);
            }
        }
    }
    assert(i == CHUNKS3);

    return chunk;
}

Mesh * generateMesh(Octree **chunk)
{
    Mesh *mesh = new Mesh[CHUNKS3];
    for (int i = 0; i < CHUNKS3; ++i)
    {
        mesh[i] = Mesh();
        plantCubes(chunk[i], mesh[i]);
    }
    return mesh;
}

struct RaymarchedCube
{
    unsigned vao, vbo, ebo, tex;
    int im, is;
    Shader shader;

    RaymarchedCube();
    ~RaymarchedCube();
    void draw();
};


RaymarchedCube::RaymarchedCube()
{
    static const float vertices[3*8] = {
        0., 0., 0.,
        0., 0., 1.,
        0., 1., 0.,
        0., 1., 1.,
        1., 0., 0.,
        1., 0., 1.,
        1., 1., 0.,
        1., 1., 1.,
    };

    static const unsigned short indices[3*6*2] = {
        0, 4, 6,
        6, 2, 0,
        1, 0, 2,
        2, 3, 1,
        5, 7, 3,
        3, 1, 5,
        4, 5, 7,
        7, 6, 4,
        4, 0, 1,
        1, 5, 4,
        2, 6, 7,
        7, 3, 2,
    };

    glGenVertexArrays(1, &this->vao);
    glBindVertexArray(this->vao);

    glGenBuffers(1, &this->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &this->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glGenTextures(1, &this->tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->tex);
    SDL_Surface *st = IMG_Load("textures/quad.png");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, st->w, st->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, st->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);
    SDL_FreeSurface(st);

    this->shader.compile("shaders/raymarch.vertex.glsl");
    this->shader.compile("shaders/raymarch.fragment.glsl");
    this->shader.link();
    this->im = glGetUniformLocation(shader.program, "mvp");
    this->is = glGetUniformLocation(shader.program, "sampler");

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

RaymarchedCube::~RaymarchedCube()
{

}

void RaymarchedCube::draw()
{

}

int main() 
{
    using glm::vec3;

    Stopwatch sw;

    /*
    printf("Generating world...\n"); sw.start();
    Octree **chunks = generateWorld();
    double gentime = sw.stop();
    printf("%lfs\n", gentime);

    printf("Generating mesh...\n"); 
    sw.start();
    Mesh *mesh = generateMesh(chunks);
    double meshtime = sw.stop();
    printf("%lfs\n", meshtime);
    */

    printf("Initializing...\n"); 
    sw.start();

    initialize();

    initializeControls();

    Text text("C:/Windows/Fonts/arial.ttf", 18, width, height);

    Shader shader;
    shader.compile("shaders/cube.vrtx.glsl", GL_VERTEX_SHADER);
    shader.compile("shaders/cube.frag.glsl", GL_FRAGMENT_SHADER);
    shader.link();
    int mvpIndex = glGetUniformLocation(shader.program, "mvp");
    int samplerIndex = glGetUniformLocation(shader.program, "samp");

    double inittime = sw.stop();
    printf("%fs\n", inittime);


    /*
    printf("Before optimization:\n");
    printf("|vbo|: %llu\n", mesh.vertices.size());
    printf("|uv|: %llu\n", mesh.uv.size());
    printf("|ebo|: %llu\n", mesh.indices.size());

    mesh.optimize(0);
    printf("After optimization:\n");
    printf("|vbo|: %llu\n", mesh.vertices.size());
    printf("|uv|: %llu\n", mesh.uv.size());
    printf("|ebo|: %llu\n", mesh.indices.size());
    */

    /*
    for (int i = 0; i < CHUNKS3; ++i)
        mesh[i].finalize();
    */

    double frametime = 0;
    while (running) 
    {
        sw.start();

        input.poll();

        calculateMVP();

        glClearColor(0.3, 0.3, 0.6, 1.);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader.program);

        glUniformMatrix4fv(mvpIndex, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform1i(samplerIndex, 0);

        /*
        for (int i = 0; i < CHUNKS3; ++i)
            drawMesh(mesh[i]);
        */

        drawText(text, frametime);

        glUseProgram(0);
        
        ++frameCount;

        frametime = sw.stop();

        SDL_GL_SwapWindow(window);
    }

    text.~Text();
    deinitialize();

    return 0;
}