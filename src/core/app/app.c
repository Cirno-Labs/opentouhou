#define SDL_MAIN_USE_CALLBACKS
#define SDL_MAIN_NOIMPL
#include "core/app.h"

#include "core/base/log.h"

#include "clock.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#define MAX_TICKS_PER_FRAME 4

typedef const GLubyte *(GLAPIENTRY *GL_GET_STRING_FUNC)(GLenum name);
typedef void(GLAPIENTRY *GL_VIEWPORT_FUNC)(
    GLint x, GLint y, GLsizei width, GLsizei height);
typedef void(GLAPIENTRY *GL_CLEAR_COLOR_FUNC)(
    GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void(GLAPIENTRY *GL_CLEAR_FUNC)(GLbitfield mask);

typedef struct APP_CONTEXT APP_CONTEXT;

struct APP_CONTEXT {
    SDL_Window *window;
    SDL_GLContext gl_context;
    GL_GET_STRING_FUNC gl_get_string;
    GL_VIEWPORT_FUNC gl_viewport;
    GL_CLEAR_COLOR_FUNC gl_clear_color;
    GL_CLEAR_FUNC gl_clear;
    CLOCK clock;
    bool vsync;
    bool game_init_called;
};

struct FRAME {
    uint64_t tick;
};

static APP_CONTEXT m_Context;
static FRAME m_Frame;

static RESULT M_CreateWindow(APP_CONTEXT *const ctx)
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    ctx->window = SDL_CreateWindow(
        g_GameDesc.title, g_GameDesc.logical_width, g_GameDesc.logical_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    FAIL_IF(
        ctx->window == nullptr, "Cannot create the window: %s", SDL_GetError());

    ctx->gl_context = SDL_GL_CreateContext(ctx->window);
    FAIL_IF(
        ctx->gl_context == nullptr,
        "Cannot create an OpenGL 3.3 core context: %s", SDL_GetError());

    ctx->gl_get_string =
        (GL_GET_STRING_FUNC)SDL_GL_GetProcAddress("glGetString");
    ctx->gl_viewport = (GL_VIEWPORT_FUNC)SDL_GL_GetProcAddress("glViewport");
    ctx->gl_clear_color =
        (GL_CLEAR_COLOR_FUNC)SDL_GL_GetProcAddress("glClearColor");
    ctx->gl_clear = (GL_CLEAR_FUNC)SDL_GL_GetProcAddress("glClear");
    FAIL_IF(
        ctx->gl_get_string == nullptr || ctx->gl_viewport == nullptr
            || ctx->gl_clear_color == nullptr || ctx->gl_clear == nullptr,
        "Cannot load the OpenGL functions: %s", SDL_GetError());

    ctx->vsync = SDL_GL_SetSwapInterval(1);
    if (!ctx->vsync) {
        LOG_INFO("Vsync is not available: %s", SDL_GetError());
    }

    LOG_INFO(
        "OpenGL %s, %s", (const char *)ctx->gl_get_string(GL_VERSION),
        (const char *)ctx->gl_get_string(GL_RENDERER));
    return OK;
}

static RESULT M_Init(APP_CONTEXT *const ctx)
{
    FAIL_IF(
        !SDL_Init(SDL_INIT_VIDEO), "Cannot initialize SDL: %s", SDL_GetError());
    MUST(M_CreateWindow(ctx));

    Clock_Init(&ctx->clock, APP_TICK_RATE, MAX_TICKS_PER_FRAME);

    ctx->game_init_called = true;
    MUST(g_GameDesc.init(), "%s failed to start", g_GameDesc.id);
    return OK;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    return Result_Log(M_Init(&m_Context), LOG_LEVEL_ERROR) ? SDL_APP_CONTINUE
                                                           : SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    APP_CONTEXT *const ctx = &m_Context;

    const int ticks = Clock_Advance(&ctx->clock, SDL_GetTicksNS());
    for (int i = 0; i < ticks; i++) {
        const TICK_INPUT input = {};
        if (!g_GameDesc.tick(&input, &m_Frame)) {
            return SDL_APP_SUCCESS;
        }
        m_Frame.tick++;
    }

    int width;
    int height;
    SDL_GetWindowSizeInPixels(ctx->window, &width, &height);
    ctx->gl_viewport(0, 0, width, height);
    ctx->gl_clear_color(0.10f, 0.02f, 0.05f, 1.0f);
    ctx->gl_clear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(ctx->window);

    if (!ctx->vsync) {
        SDL_DelayNS(Clock_NsUntilNextTick(&ctx->clock));
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    APP_CONTEXT *const ctx = &m_Context;
    if (ctx->game_init_called) {
        g_GameDesc.shutdown();
    }
    if (ctx->gl_context != nullptr) {
        SDL_GL_DestroyContext(ctx->gl_context);
    }
    if (ctx->window != nullptr) {
        SDL_DestroyWindow(ctx->window);
    }
}
