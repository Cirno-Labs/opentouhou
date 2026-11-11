#define SDL_MAIN_USE_CALLBACKS
#define SDL_MAIN_NOIMPL
#include "core/app.h"

#include "core/audio.h"

#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/config.h"
#include "core/gfx.h"
#include "core/ini.h"
#include "core/io/path.h"
#include "core/patch.h"

#include "clock.h"
#include "hotkeys.h"
#include "request_queue.h"
#include "settings.h"
#include "screenshot.h"
#include "tick_rate.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifdef _WIN32
    #include <windows.h>
#endif

#include <stdio.h>
#include <string.h>

#define MAX_TICKS_PER_FRAME 4

typedef struct APP_CONTEXT APP_CONTEXT;

struct APP_CONTEXT {
    SDL_Window *window;
    SDL_GLContext gl_context;
    CLOCK clock;
    TICK_RATE tick_rate;
    bool tick_rate_measured;
    float measured_tick_rate;
    bool vsync;
    bool game_init_called;
    INPUT_STATE *input;
    // Holds g_GameDesc.input_schema->players input structs, one after
    // another.
    void *input_buffer;
    // Holds the directory with the original game's data.
    char *data_dir;
    char *user_dir;
    // Holds config.ini's path, or nullptr when the user directory could
    // not be found.
    char *ini_path;
    INI *ini;
    APP_SETTINGS settings;
    PATCH_STACK *patches;
    HOTKEYS hotkeys;
    HOTKEYS hotkeys_edges;
    bool screenshot_pending;
    APP_REQUESTS requests;
    bool pad_pressed;
    INPUT_BINDING pad_press;
};

static APP_CONTEXT m_Context;

// Splits the comma-separated ids of text into ids, trimming the spaces
// around each, and returns their number. The ids point into text, which
// the function changes.
static int32_t M_SplitIds(
    char *const text, const char **const ids, const int32_t capacity)
{
    int32_t count = 0;
    char *rest = text;
    while (rest != nullptr && count < capacity) {
        char *const comma = strchr(rest, ',');
        if (comma != nullptr) {
            *comma = '\0';
        }
        while (*rest == ' ' || *rest == '\t') {
            rest++;
        }
        size_t length = strlen(rest);
        while (length > 0
               && (rest[length - 1] == ' ' || rest[length - 1] == '\t')) {
            rest[--length] = '\0';
        }
        if (length > 0) {
            ids[count++] = rest;
        }
        rest = comma != nullptr ? comma + 1 : nullptr;
    }
    return count;
}

static void M_OpenPatches(APP_CONTEXT *const ctx)
{
    if (g_GameDesc.patch_game == nullptr || !ctx->settings.patches_enabled) {
        return;
    }
    char *dir;
    if (!Result_Log(Path_GetPatchesDir(&dir), LOG_LEVEL_ERROR)) {
        return;
    }
    char disabled_text[CONFIG_STRING_SIZE];
    memcpy(
        disabled_text, ctx->settings.patches_disabled, sizeof(disabled_text));
    const char *disabled[CONFIG_STRING_SIZE / 2];
    const int32_t disabled_count =
        M_SplitIds(disabled_text, disabled, (int32_t)ARRAY_SIZE(disabled));
    Result_Log(
        Patch_OpenStack(
            dir, g_GameDesc.id, g_GameDesc.patch_game, disabled, disabled_count,
            &ctx->patches),
        LOG_LEVEL_ERROR);
    Mem_Free(dir);
    for (int32_t i = 0; i < Patch_GetCount(ctx->patches); i++) {
        const PATCH_INFO *const info = Patch_Get(ctx->patches, i);
        LOG_INFO("Patch %d: %s (%s)", i + 1, info->id, info->title);
    }
}

static RESULT M_CreateWindow(APP_CONTEXT *const ctx)
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#ifdef __APPLE__
    // macOS offers a 3.3 core context only when it is forward-compatible.
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif

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

// Returns the default data directory for an executable in exe_dir. On
// macOS, an executable inside an application bundle looks beside the
// bundle, because the bundle holds only the program.
static char *M_DataDirOf(const char *const exe_dir)
{
    char *const dir = Str_Dup(exe_dir);
#ifdef __APPLE__
    static const char suffix[] = ".app/Contents/MacOS";
    size_t length = strlen(dir);
    while (length > 1 && dir[length - 1] == '/') {
        dir[--length] = '\0';
    }
    if (length > sizeof(suffix) - 1
        && strcmp(dir + length - (sizeof(suffix) - 1), suffix) == 0) {
        dir[length - (sizeof("/Contents/MacOS") - 1)] = '\0';
        char *const slash = strrchr(dir, '/');
        if (slash != nullptr) {
            slash[1] = '\0';
        }
    }
#endif
    return dir;
}

// Prints the usage, the data directory and the user directory to stdout.
// On Windows, a release build has no console of its own, so the text goes
// to the console of the shell that started it.
static void M_PrintHelp(const char *const program)
{
#ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
    }
#endif
    const char *const exe_dir = Path_GetExeDir();
    char *const data_dir = exe_dir != nullptr ? M_DataDirOf(exe_dir) : nullptr;
    char *user_dir = nullptr;
    Result_Ignore(Path_GetUserDir(g_GameDesc.id, &user_dir));
    printf(
        "Usage: %s [--data DIR]\n"
        "\n"
        "Runs %s, version %s.\n"
        "\n"
        "  --data DIR  read the original game's data from DIR\n"
        "  --help      print this help and quit\n"
        "\n"
        "Data directory without --data: %s\n"
        "Settings, scores and replays:  %s\n",
        program, g_GameDesc.title, g_GameDesc.version,
        data_dir != nullptr ? data_dir : "unknown",
        user_dir != nullptr ? user_dir : "no user directory");
    fflush(stdout);
    Mem_Free(data_dir);
    Mem_Free(user_dir);
}

// Reads the command line. Sets *help when it asks for the usage.
static RESULT M_ParseArgs(
    const int argc, char *const argv[], char **const data_dir, bool *const help)
{
    *data_dir = nullptr;
    *help = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            *help = true;
            continue;
        }
        FAIL_IF(
            strcmp(argv[i], "--data") != 0, "Unknown argument: %s (see --help)",
            argv[i]);
        FAIL_IF(i + 1 >= argc, "--data needs a directory");
        FAIL_IF(*data_dir != nullptr, "--data is given twice");
        *data_dir = Str_Dup(argv[++i]);
    }
    if (*data_dir == nullptr) {
        const char *const exe_dir = Path_GetExeDir();
        FAIL_IF(exe_dir == nullptr, "Cannot find the executable's directory");
        *data_dir = M_DataDirOf(exe_dir);
    }
    return OK;
}

static RESULT M_SetFullscreen(APP_CONTEXT *const ctx, const bool fullscreen)
{
    FAIL_IF(
        !SDL_SetWindowFullscreen(ctx->window, fullscreen),
        "Cannot change fullscreen mode: %s", SDL_GetError());
    ctx->settings.fullscreen = fullscreen;
    return OK;
}

static RESULT M_Init(
    APP_CONTEXT *const ctx, const int argc, char *const argv[],
    bool *const help)
{
    MUST(M_ParseArgs(argc, argv, &ctx->data_dir, help));
    if (*help) {
        M_PrintHelp(argc > 0 ? argv[0] : g_GameDesc.id);
        return OK;
    }
    LOG_INFO("%s %s", g_GameDesc.id, g_GameDesc.version);
    FAIL_IF(
        !SDL_Init(SDL_INIT_VIDEO), "Cannot initialize SDL: %s", SDL_GetError());
    MUST(M_CreateWindow(ctx));

    Clock_Init(&ctx->clock, APP_TICK_RATE, MAX_TICKS_PER_FRAME);
    ctx->requests.fullscreen = -1;

    if (Result_Log(
            Path_GetUserDir(g_GameDesc.id, &ctx->user_dir), LOG_LEVEL_ERROR)) {
        ctx->ini_path =
            Path_Join(&g_HeapAllocator, ctx->user_dir, "config.ini");
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
    if (g_GameDesc.settings_schema != nullptr) {
        Config_Load(g_GameDesc.settings_schema, ctx->ini, g_GameDesc.settings);
    }
    M_OpenPatches(ctx);
    if (ctx->settings.fullscreen
        && !Result_Log(M_SetFullscreen(ctx, true), LOG_LEVEL_WARNING)) {
        ctx->settings.fullscreen = false;
    }

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

const char *App_GetDataDir(void)
{
    return m_Context.data_dir;
}

const char *App_GetUserDir(void)
{
    return m_Context.user_dir;
}

const PATCH_STACK *App_GetPatches(void)
{
    return m_Context.patches;
}

static void M_ApplyRequests(APP_CONTEXT *const ctx)
{
    AppRequests_ApplyInput(&ctx->requests, ctx->input, g_GameDesc.input_schema);
    if (ctx->requests.fullscreen >= 0) {
        const bool fullscreen = ctx->requests.fullscreen != 0;
        ctx->requests.fullscreen = -1;
        if (fullscreen != ctx->settings.fullscreen) {
            Result_Log(M_SetFullscreen(ctx, fullscreen), LOG_LEVEL_WARNING);
        }
    }
}

bool App_GetPadPress(INPUT_BINDING *const out)
{
    if (m_Context.pad_pressed) {
        *out = m_Context.pad_press;
    }
    return m_Context.pad_pressed;
}

size_t App_GetBindings(
    const size_t action, INPUT_BINDING *const out, const size_t capacity)
{
    ASSERT(action < g_GameDesc.input_schema->count);
    return Input_GetBindings(
        m_Context.input, g_GameDesc.input_schema, 0, action, out, capacity);
}

void App_RequestBind(const size_t action, const INPUT_BINDING binding)
{
    ASSERT(action < g_GameDesc.input_schema->count);
    AppRequests_Add(
        &m_Context.requests,
        (APP_REQUEST) { APP_REQUEST_BIND, action, binding });
}

void App_RequestUnbind(const size_t action, const INPUT_BINDING binding)
{
    ASSERT(action < g_GameDesc.input_schema->count);
    AppRequests_Add(
        &m_Context.requests,
        (APP_REQUEST) { APP_REQUEST_UNBIND, action, binding });
}

void App_RequestResetPadBindings(void)
{
    AppRequests_Add(
        &m_Context.requests, (APP_REQUEST) { .type = APP_REQUEST_RESET_PAD });
}

bool App_GetTickRate(float *const out)
{
    if (m_Context.tick_rate_measured) {
        *out = m_Context.measured_tick_rate;
    }
    return m_Context.tick_rate_measured;
}

bool App_IsFullscreen(void)
{
    return m_Context.settings.fullscreen;
}

void App_RequestFullscreen(const bool fullscreen)
{
    m_Context.requests.fullscreen = fullscreen ? 1 : 0;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    bool help = false;
    if (!Result_Log(M_Init(&m_Context, argc, argv, &help), LOG_LEVEL_ERROR)) {
        return SDL_APP_FAILURE;
    }
    return help ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    if (event->type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN) {
        m_Context.settings.fullscreen = true;
    } else if (event->type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN) {
        m_Context.settings.fullscreen = false;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    APP_CONTEXT *const ctx = &m_Context;

    const uint64_t now_ns = SDL_GetTicksNS();
    const int ticks = Clock_Advance(&ctx->clock, now_ns);
    float tick_rate;
    if (TickRate_Add(&ctx->tick_rate, ticks, now_ns, &tick_rate)) {
        ctx->tick_rate_measured = true;
        ctx->measured_tick_rate = tick_rate;
    }
    Input_Update(ctx->input, ctx->input_buffer);
    HOTKEYS hotkeys;
    Input_Read(ctx->input, &g_HotkeysSchema, &hotkeys);
    Input_Debounce(
        &g_HotkeysSchema, &ctx->hotkeys, &hotkeys, &ctx->hotkeys_edges);
    ctx->hotkeys = hotkeys;
    if (ctx->hotkeys_edges.fullscreen) {
        Result_Log(
            M_SetFullscreen(ctx, !ctx->settings.fullscreen), LOG_LEVEL_WARNING);
    }
    if (ctx->hotkeys_edges.screenshot) {
        ctx->screenshot_pending = true;
    }
    INPUT_BINDING press;
    if (AppRequests_ReadPadPress(ctx->input, &press) && !ctx->pad_pressed) {
        ctx->pad_pressed = true;
        ctx->pad_press = press;
    }
    for (int i = 0; i < ticks; i++) {
        Gfx_BeginTick((GFX_COLOR) { 0, 0, 0, 255 });
        const bool running = g_GameDesc.tick(ctx->input_buffer);
        Gfx_EndTick();
        ctx->pad_pressed = false;
        ctx->tick_rate_measured = false;
        M_ApplyRequests(ctx);
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
            Gfx_Present(
                ctx->settings.interpolation, t > 1.0f ? 1.0f : t, &output),
            LOG_LEVEL_ERROR)) {
        return SDL_APP_FAILURE;
    }
    if (ctx->screenshot_pending) {
        Result_Log(
            Screenshot_Save(ctx->user_dir, g_GameDesc.id, &ctx->settings),
            LOG_LEVEL_WARNING);
        ctx->screenshot_pending = false;
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
    Patch_CloseStack(ctx->patches);
    Audio_Shutdown();
    if (ctx->ini != nullptr) {
        Config_Save(&g_AppSettingsSchema, ctx->ini, &ctx->settings);
        if (g_GameDesc.settings_schema != nullptr) {
            Config_Save(
                g_GameDesc.settings_schema, ctx->ini, g_GameDesc.settings);
        }
        Input_Save(ctx->input, ctx->ini);
        if (ctx->ini_path != nullptr) {
            Result_Log(Ini_Save(ctx->ini, ctx->ini_path), LOG_LEVEL_ERROR);
        }
        Ini_Free(ctx->ini);
    }
    Mem_Free(ctx->ini_path);
    Mem_Free(ctx->data_dir);
    Mem_Free(ctx->user_dir);
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
