#define SDL_MAIN_USE_CALLBACKS
#define SDL_MAIN_NOIMPL
#include "core/app.h"

#include "core/audio.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/config.h"
#include "core/gfx.h"
#include "core/ini.h"
#include "core/io/path.h"

#include "clock.h"
#include "hotkeys.h"
#include "settings.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <string.h>

#define MAX_TICKS_PER_FRAME 4

typedef struct APP_CONTEXT APP_CONTEXT;

struct APP_CONTEXT {
    SDL_Window *window;
    SDL_GLContext gl_context;
    CLOCK clock;
    bool vsync;
    bool game_init_called;
    INPUT_STATE *input;
    // Holds g_GameDesc.input_schema->players input structs, one after
    // another.
    void *input_buffer;
    // Holds the directory with the original game's data.
    char *data_dir;
    // Holds config.ini's path, or nullptr when the user directory could
    // not be found.
    char *ini_path;
    INI *ini;
    APP_SETTINGS settings;
    HOTKEYS hotkeys;
    HOTKEYS hotkeys_edges;
};

static APP_CONTEXT m_Context;

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

    MUST(Gfx_Init(
        ctx->window, g_GameDesc.logical_width, g_GameDesc.logical_height));

    ctx->vsync = SDL_GL_SetSwapInterval(1);
    if (!ctx->vsync) {
        LOG_INFO("Vsync is not available: %s", SDL_GetError());
    }
    return OK;
}

static RESULT M_ParseDataDir(
    const int argc, char *const argv[], char **const out)
{
    *out = nullptr;
    for (int i = 1; i < argc; i++) {
        FAIL_IF(
            strcmp(argv[i], "--data") != 0, "Unknown argument: %s", argv[i]);
        FAIL_IF(i + 1 >= argc, "--data needs a directory");
        FAIL_IF(*out != nullptr, "--data is given twice");
        *out = Str_Dup(argv[++i]);
    }
    if (*out == nullptr) {
        const char *const exe_dir = Path_GetExeDir();
        FAIL_IF(exe_dir == nullptr, "Cannot find the executable's directory");
        *out = Str_Dup(exe_dir);
    }
    return OK;
}

const char *App_GetDataDir(void)
{
    return m_Context.data_dir;
}

static RESULT M_Init(APP_CONTEXT *const ctx, const int argc, char *const argv[])
{
    MUST(M_ParseDataDir(argc, argv, &ctx->data_dir));
    FAIL_IF(
        !SDL_Init(SDL_INIT_VIDEO), "Cannot initialize SDL: %s", SDL_GetError());
    MUST(M_CreateWindow(ctx));

    Clock_Init(&ctx->clock, APP_TICK_RATE, MAX_TICKS_PER_FRAME);

    char *user_dir = nullptr;
    if (Result_Log(
            Path_GetUserDir(g_GameDesc.id, &user_dir), LOG_LEVEL_ERROR)) {
        ctx->ini_path = Path_Join(&g_HeapAllocator, user_dir, "config.ini");
        Mem_Free(user_dir);
    }
    if (ctx->ini_path != nullptr) {
        Result_Log(
            Ini_Load(ctx->ini_path, &g_HeapAllocator, &ctx->ini),
            LOG_LEVEL_ERROR);
    }
    if (ctx->ini == nullptr) {
        MUST(Ini_Parse("", 0, &g_HeapAllocator, &ctx->ini));
    }
    Config_Load(&g_AppSettingsSchema, ctx->ini, &ctx->settings);

    ctx->input = Input_Create(g_GameDesc.input_schema, &g_HeapAllocator);
    Input_AddSchema(ctx->input, &g_HotkeysSchema);
    Input_Load(ctx->input, ctx->ini);

    ctx->input_buffer = Allocator_AllocZ(
        &g_HeapAllocator,
        g_GameDesc.input_schema->size
            * (size_t)g_GameDesc.input_schema->players);

    Result_Log(Audio_Init(), LOG_LEVEL_WARNING);

    ctx->game_init_called = true;
    MUST(g_GameDesc.init(), "%s failed to start", g_GameDesc.id);
    return OK;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    return Result_Log(M_Init(&m_Context, argc, argv), LOG_LEVEL_ERROR)
        ? SDL_APP_CONTINUE
        : SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    APP_CONTEXT *const ctx = &m_Context;

    const int ticks = Clock_Advance(&ctx->clock, SDL_GetTicksNS());
    if (ticks > 0) {
        Input_Update(ctx->input, ctx->input_buffer);
        HOTKEYS hotkeys;
        Input_Read(ctx->input, &g_HotkeysSchema, &hotkeys);
        Input_Debounce(
            &g_HotkeysSchema, &ctx->hotkeys, &hotkeys, &ctx->hotkeys_edges);
        ctx->hotkeys = hotkeys;
    }
    for (int i = 0; i < ticks; i++) {
        Gfx_BeginTick((GFX_COLOR) { 0, 0, 0, 255 });
        const bool running = g_GameDesc.tick(ctx->input_buffer);
        Gfx_EndTick();
        if (!running) {
            return SDL_APP_SUCCESS;
        }
    }

    int width;
    int height;
    SDL_GetWindowSizeInPixels(ctx->window, &width, &height);
    const GFX_OUTPUT output = {
        .width = width,
        .height = height,
        .border = { 0, 0, 0, 255 },
    };
    const float t = (float)((double)ctx->clock.pending / 1000000000.0);
    if (!Result_Log(
            Gfx_Present(true, t > 1.0f ? 1.0f : t, &output), LOG_LEVEL_ERROR)) {
        return SDL_APP_FAILURE;
    }
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
    Audio_Shutdown();
    if (ctx->ini != nullptr) {
        Config_Save(&g_AppSettingsSchema, ctx->ini, &ctx->settings);
        Input_Save(ctx->input, ctx->ini);
        if (ctx->ini_path != nullptr) {
            Result_Log(Ini_Save(ctx->ini, ctx->ini_path), LOG_LEVEL_ERROR);
        }
        Ini_Free(ctx->ini);
    }
    Mem_Free(ctx->ini_path);
    Mem_Free(ctx->data_dir);
    if (ctx->input_buffer != nullptr) {
        Allocator_Free(
            &g_HeapAllocator, ctx->input_buffer,
            g_GameDesc.input_schema->size
                * (size_t)g_GameDesc.input_schema->players);
    }
    Input_Destroy(ctx->input);
    Gfx_Shutdown();
    if (ctx->gl_context != nullptr) {
        SDL_GL_DestroyContext(ctx->gl_context);
    }
    if (ctx->window != nullptr) {
        SDL_DestroyWindow(ctx->window);
    }
}
