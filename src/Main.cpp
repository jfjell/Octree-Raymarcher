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

int randInt(int min, int max)
{
    return rand() % (max - min) + min;
}

/*
void generateRandomCubes(Mesh& mesh)
{
    using glm::vec3;

    srand(time(NULL));
    for (int i = 0; i < 1000; ++i)
    {
        addCube(mesh, vec3(randInt(-100, 100), randInt(-100, 100), randInt(-100, 100)), vec3(randInt(1, 5), randInt(1, 5), randInt(1, 5)));
    }
}
*/

/*
void generateWorld(Mesh& mesh)
{
    using glm::vec2;
    using glm::vec3;

    int low = -20, high = 20, ampl = 10;
    for (int x = low; x <= high; ++x)
    {
        for (int z = low; z <= high; ++z)
        {
            float y = glm::perlin(vec2((float)x / high, (float)z / high)) * ampl;
            addCube(mesh, vec3(x, y, z), vec3(1, 1, 1));
        }
    }
}*/

int getNodes(const Octree *t)
{
    if(!t) return 0;
    int n = 1;
    for (int i = 0; i < 8; ++i)
        n += getNodes(t->branches[i]);
    return n;
}

int main() 
{
    using glm::vec3;

    Stopwatch sw;

    printf("Generating world...\n"); sw.start();
    int CHUNKS = 4;
    int CHUNKS3 = CHUNKS*CHUNKS*CHUNKS;
    float CHUNKSIZE = 16;
    Octree *chunk[CHUNKS3];
    memset(chunk, 0, sizeof(chunk));
    Mesh mesh[CHUNKS3];
    int chunkIndex = 0;
    int depth = 5;
    int total = 0;
    for (int x = -CHUNKS/2; x < CHUNKS/2; ++x)
    {
        for (int y = -CHUNKS/2; y < CHUNKS/2; ++y)
        {
            for (int z = -CHUNKS/2; z < CHUNKS/2; ++z)
            {
                vec3 pos = vec3((float)x, (float)y, (float)z) * CHUNKSIZE;
                // chunk[chunkIndex++] = generateWorldDumb(pos * CHUNKSIZE, depth, CHUNKSIZE, 0.125, 16);
                chunk[chunkIndex++] = generateWorld(pos, depth, CHUNKSIZE, 16., 0.125, 0.);
                int n = getNodes(chunk[chunkIndex-1]);
                total += n;
                printf("Chunk #%d at (%f, %f, %f): %d tree(s)\n", chunkIndex - 1, pos.x, pos.y, pos.z, n);
            }
        }
    }
    double gentime = sw.stop();
    printf("%lfs\n", gentime);
    printf("Total: %d nodes\n", total);
    printf("Generating mesh...\n"); 
    sw.start();
    assert(chunkIndex == CHUNKS3);
    for (int i = 0; i < CHUNKS3; ++i)
    {
        mesh[i] = Mesh();
        plantMeshDumb(chunk[i], mesh[i]);
        printf("Mesh #%d: %llu vertices\n", i, mesh[i].vertices.size());
    }
    double meshtime = sw.stop();
    // addCube(mesh, vec3(0, 0, 0), vec3(1, 1, 1));

    printf("%lfs\nInitializing ", meshtime); sw.start();

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

    for (int i = 0; i < CHUNKS3; ++i)
        mesh[i].finalize();

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

        for (int i = 0; i < CHUNKS3; ++i)
            drawMesh(mesh[i]);

        drawText(text, frametime);

        glUseProgram(0);
        
        SDL_ShowCursor(SDL_DISABLE);
        SDL_SetRelativeMouseMode(SDL_TRUE);

        ++frameCount;

        frametime = sw.stop();

        SDL_GL_SwapWindow(window);
    }

    text.~Text();
    deinitialize();

    return 0;
}