#include "shell/game.h"
#include "shell/game/entry.h"

#include "core/audio.h"

#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/config.h"
#include "core/gfx.h"
#include "core/ini.h"
#include "core/io/locale.h"
#include "core/io/path.h"
#include "core/patch.h"
#include "core/vfs.h"
#include "shell/fatal.h"
#include "shell/folders.h"
#include "shell/log_file.h"

#include "clock.h"
#include "hotkeys.h"
#include "request_queue.h"
#include "settings.h"
#include "screenshot.h"
#include "tick_rate.h"

#include <SDL3/SDL.h>

#ifdef _WIN32
    #include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TICKS_PER_FRAME 4
#define M_MAX_LOCALES 8

static_assert((int)SHELL_CONTINUE == (int)SDL_APP_CONTINUE);
static_assert((int)SHELL_SUCCESS == (int)SDL_APP_SUCCESS);
static_assert((int)SHELL_FAILURE == (int)SDL_APP_FAILURE);

typedef struct SHELL_CONTEXT SHELL_CONTEXT;

struct SHELL_CONTEXT {
    const GAME_DESC *desc;
    SDL_Window *window;
    SDL_GLContext gl_context;
    CLOCK clock;
    TICK_RATE tick_rate;
    bool tick_rate_measured;
    float measured_tick_rate;
    bool vsync;
    bool game_init_called;
    INPUT_STATE *input;
    // Holds desc->input_schema->players input structs, one after
    // another.
    void *input_buffer;
    FOLDERS folders;
    // Points at the entry of desc->data_files that the data folder
    // lacks, or is nullptr.
    const char *missing_data_file;
    // Holds config.ini's path, or nullptr when the user directory could
    // not be found.
    char *ini_path;
    INI *ini;
    // Holds the loaded settings as the game writes them, so that the save
    // can tell which ones changed.
    INI *loaded_settings;
    SHELL_SETTINGS settings;
    PATCH_STACK *patches;
    HOTKEYS hotkeys;
    HOTKEYS hotkeys_edges;
    bool screenshot_pending;
    SHELL_REQUESTS requests;
    bool pad_pressed;
    INPUT_BINDING pad_press;
};

static SHELL_CONTEXT m_Context;

static void M_OpenPatches(SHELL_CONTEXT *const ctx)
{
    if (ctx->desc->patch_game == nullptr) {
        return;
    }
    char *dir;
    if (!Result_Log(
            Folders_GetPatchesDir(&ctx->folders, &dir), LOG_LEVEL_ERROR)) {
        return;
    }
    LOCALE locales[M_MAX_LOCALES];
    PATCH_CHOICE choice = {
        .locales = locales,
        .locale_count = Locale_ListPreferred(locales, M_MAX_LOCALES),
    };
    Patch_ReadChoice(ctx->ini, &choice);
    Result_Log(
        Patch_OpenStack(
            dir, ctx->desc->id, ctx->desc->patch_game, &choice, &ctx->patches),
        LOG_LEVEL_ERROR);
    Mem_Free(dir);
}

static void M_WriteSettings(const SHELL_CONTEXT *const ctx, INI *const ini)
{
    Config_Save(&g_ShellSettingsSchema, ini, &ctx->settings);
    if (ctx->desc->settings_schema != nullptr) {
        Config_Save(ctx->desc->settings_schema, ini, ctx->desc->settings);
    }
    Input_Save(ctx->input, ini);
}

// Reads config.ini again so that an edit made while the game ran, such as
// by othpatch, stays.
static void M_SaveSettings(const SHELL_CONTEXT *const ctx)
{
    INI *current;
    if (!Result_Log(
            Ini_Parse("", 0, &g_HeapAllocator, &current), LOG_LEVEL_ERROR)) {
        return;
    }
    M_WriteSettings(ctx, current);
    INI *disk = nullptr;
    Result_Log(
        Ini_Load(ctx->ini_path, &g_HeapAllocator, &disk), LOG_LEVEL_ERROR);
    INI *const target = disk != nullptr ? disk : ctx->ini;
    Ini_MergeChanges(target, ctx->loaded_settings, current);
    Result_Log(Ini_Save(target, ctx->ini_path), LOG_LEVEL_ERROR);
    Ini_Free(disk);
    Ini_Free(current);
}

static RESULT M_CheckData(SHELL_CONTEXT *const ctx)
{
    if (ctx->desc->data_file_count == 0) {
        return OK;
    }
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    RESULT result = Vfs_MountDir(vfs, ctx->folders.data_dir, 0);
    for (int32_t i = 0; IS_OK(result) && i < ctx->desc->data_file_count; i++) {
        const char *const name = ctx->desc->data_files[i];
        READER file;
        result = Vfs_Open(vfs, name, VFS_STREAM, &file);
        if (IS_OK(result)) {
            Reader_Close(&file);
        } else {
            ctx->missing_data_file = name;
            result = Result_Prefix(result, "%s", name);
        }
    }
    Vfs_Destroy(vfs);
    // A data folder that cannot be read lacks every file.
    if (!IS_OK(result) && ctx->missing_data_file == nullptr) {
        ctx->missing_data_file = ctx->desc->data_files[0];
    }
    return result;
}

static RESULT M_CreateWindow(SHELL_CONTEXT *const ctx)
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
        ctx->desc->title, ctx->desc->logical_width, ctx->desc->logical_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    FAIL_IF(
        ctx->window == nullptr, "Cannot create the window: %s", SDL_GetError());

    ctx->gl_context = SDL_GL_CreateContext(ctx->window);
    FAIL_IF(
        ctx->gl_context == nullptr,
        "Cannot create an OpenGL 3.3 core context: %s", SDL_GetError());

    MUST(Gfx_Init(
        ctx->window, ctx->desc->logical_width, ctx->desc->logical_height));

    ctx->vsync = SDL_GL_SetSwapInterval(1);
    if (!ctx->vsync) {
        LOG_INFO("Vsync is not available: %s", SDL_GetError());
    }
    return OK;
}

// Prepares stdout for printing. On Windows, a release build has no console
// of its own, so stdout goes to the console of the shell that started it,
// unless stdout already goes to a pipe or a file.
static void M_OpenStdout(void)
{
#ifdef _WIN32
    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    const bool redirected = handle != nullptr && handle != INVALID_HANDLE_VALUE
        && GetFileType(handle) != FILE_TYPE_UNKNOWN;
    if (!redirected && AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
    }
#endif
}

// Prints the usage, the mode and the folders to stdout.
static void M_PrintHelp(
    const GAME_DESC *const desc, const FOLDERS *const folders)
{
    M_OpenStdout();
    char *patches_dir = nullptr;
    Result_Ignore(Folders_GetPatchesDir(folders, &patches_dir));
    printf(
        "Usage: %s [--data DIR] [--user-dir DIR]\n"
        "\n"
        "Runs %s, version %s.\n"
        "\n"
        "  --data DIR      read the original game's data from DIR\n"
        "  --user-dir DIR  keep settings, scores and replays in DIR\n"
        "  --help          print this help and quit\n"
        "  --version       print the version and quit\n"
        "\n"
        "Mode:            %s\n"
        "Program folder:  %s\n"
        "Data folder:     %s\n"
        "User folder:     %s\n"
        "Patches folder:  %s\n",
        folders->program_name, desc->title, desc->version,
        Folders_GetModeName(folders->mode),
        folders->program_dir != nullptr ? folders->program_dir : "unknown",
        folders->data_dir != nullptr ? folders->data_dir : "unknown",
        folders->user_dir != nullptr ? folders->user_dir : "none",
        patches_dir != nullptr ? patches_dir : "none");
    fflush(stdout);
    Mem_Free(patches_dir);
}

// Reads the option at argv[*i] that takes a value into *value.
static RESULT M_ParseValue(
    const int argc, char *const argv[], int *const i, const char **const value)
{
    const char *const name = argv[*i];
    FAIL_IF(*i + 1 >= argc, "%s needs a directory", name);
    FAIL_IF(*value != nullptr, "%s is given twice", name);
    *value = argv[++*i];
    return OK;
}

// Prints "<game id> <version>" to stdout.
static void M_PrintVersion(const GAME_DESC *const desc)
{
    M_OpenStdout();
    printf("%s %s\n", desc->id, desc->version);
    fflush(stdout);
}

// Reads the command line into input. Sets *help when it asks for the
// usage, and *version when it asks for the version.
static RESULT M_ParseArgs(
    const int argc, char *const argv[], FOLDERS_INPUT *const input,
    bool *const help, bool *const version)
{
    *help = false;
    *version = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            *help = true;
        } else if (strcmp(argv[i], "--version") == 0) {
            *version = true;
        } else if (strcmp(argv[i], "--data") == 0) {
            MUST(M_ParseValue(argc, argv, &i, &input->data_dir));
        } else if (strcmp(argv[i], "--user-dir") == 0) {
            MUST(M_ParseValue(argc, argv, &i, &input->user_dir));
        } else {
            return FAIL("Unknown argument: %s (see --help)", argv[i]);
        }
    }
    return OK;
}

static RESULT M_SetFullscreen(SHELL_CONTEXT *const ctx, const bool fullscreen)
{
    FAIL_IF(
        !SDL_SetWindowFullscreen(ctx->window, fullscreen),
        "Cannot change fullscreen mode: %s", SDL_GetError());
    ctx->settings.fullscreen = fullscreen;
    return OK;
}

// Starts the program. Sets *done when the command line asks only for the
// usage or the version, which are then printed.
static RESULT M_Init(
    SHELL_CONTEXT *const ctx, const int argc, char *const argv[],
    bool *const done)
{
    FOLDERS_INPUT input = {
        .exe_dir = Path_GetExeDir(),
        .appimage = getenv("APPIMAGE"),
        .appdir = getenv("APPDIR"),
        .program = argc > 0 ? argv[0] : nullptr,
    };
    bool help;
    bool version;
    MUST(M_ParseArgs(argc, argv, &input, &help, &version));
    *done = help || version;
    if (version && !help) {
        M_PrintVersion(ctx->desc);
        return OK;
    }
    const bool found = Result_Log(
        Folders_Find(&input, ctx->desc->id, &ctx->folders), LOG_LEVEL_ERROR);
    if (help) {
        M_PrintHelp(ctx->desc, &ctx->folders);
        return OK;
    }
    if (found) {
        Result_Log(
            LogFile_Open(ctx->folders.user_dir, ctx->desc->id),
            LOG_LEVEL_WARNING);
    }
    FAIL_IF(
        ctx->folders.data_dir == nullptr,
        "Cannot find the executable's directory");
    LOG_INFO("%s %s", ctx->desc->id, ctx->desc->version);
    LOG_INFO("Folder mode: %s", Folders_GetModeName(ctx->folders.mode));
    MUST(M_CheckData(ctx), "%s failed to start", ctx->desc->id);
    FAIL_IF(
        !SDL_Init(SDL_INIT_VIDEO), "Cannot initialize SDL: %s", SDL_GetError());
    MUST(M_CreateWindow(ctx));

    Clock_Init(&ctx->clock, SHELL_TICK_RATE, MAX_TICKS_PER_FRAME);
    ctx->requests.fullscreen = -1;

    if (found) {
        ctx->ini_path =
            Path_Join(&g_HeapAllocator, ctx->folders.user_dir, "config.ini");
    }
    if (ctx->ini_path != nullptr) {
        Result_Log(
            Ini_Load(ctx->ini_path, &g_HeapAllocator, &ctx->ini),
            LOG_LEVEL_ERROR);
    }
    if (ctx->ini == nullptr) {
        MUST(Ini_Parse("", 0, &g_HeapAllocator, &ctx->ini));
    }
    Config_Load(&g_ShellSettingsSchema, ctx->ini, &ctx->settings);
    if (ctx->desc->settings_schema != nullptr) {
        Config_Load(ctx->desc->settings_schema, ctx->ini, ctx->desc->settings);
    }
    M_OpenPatches(ctx);
    if (ctx->settings.fullscreen
        && !Result_Log(M_SetFullscreen(ctx, true), LOG_LEVEL_WARNING)) {
        ctx->settings.fullscreen = false;
    }

    ctx->input = Input_Create(ctx->desc->input_schema, &g_HeapAllocator);
    Input_AddSchema(ctx->input, &g_HotkeysSchema);
    Input_Load(ctx->input, ctx->ini);
    MUST(Ini_Parse("", 0, &g_HeapAllocator, &ctx->loaded_settings));
    M_WriteSettings(ctx, ctx->loaded_settings);

    ctx->input_buffer = Allocator_AllocZ(
        &g_HeapAllocator,
        ctx->desc->input_schema->size
            * (size_t)ctx->desc->input_schema->players);

    Result_Log(Audio_Init(), LOG_LEVEL_WARNING);

    ctx->game_init_called = true;
    MUST(ctx->desc->init(), "%s failed to start", ctx->desc->id);
    return OK;
}

const char *Shell_GetDataDir(void)
{
    return m_Context.folders.data_dir;
}

const char *Shell_GetUserDir(void)
{
    return m_Context.folders.user_dir;
}

const PATCH_STACK *Shell_GetPatches(void)
{
    return m_Context.patches;
}

static void M_ApplyRequests(SHELL_CONTEXT *const ctx)
{
    ShellRequests_ApplyInput(
        &ctx->requests, ctx->input, ctx->desc->input_schema);
    if (ctx->requests.fullscreen >= 0) {
        const bool fullscreen = ctx->requests.fullscreen != 0;
        ctx->requests.fullscreen = -1;
        if (fullscreen != ctx->settings.fullscreen) {
            Result_Log(M_SetFullscreen(ctx, fullscreen), LOG_LEVEL_WARNING);
        }
    }
}

bool Shell_GetPadPress(INPUT_BINDING *const out)
{
    if (m_Context.pad_pressed) {
        *out = m_Context.pad_press;
    }
    return m_Context.pad_pressed;
}

size_t Shell_GetBindings(
    const size_t action, INPUT_BINDING *const out, const size_t capacity)
{
    ASSERT(action < m_Context.desc->input_schema->count);
    return Input_GetBindings(
        m_Context.input, m_Context.desc->input_schema, 0, action, out,
        capacity);
}

void Shell_RequestBind(const size_t action, const INPUT_BINDING binding)
{
    ASSERT(action < m_Context.desc->input_schema->count);
    ShellRequests_Add(
        &m_Context.requests,
        (SHELL_REQUEST) { SHELL_REQUEST_BIND, action, binding });
}

void Shell_RequestUnbind(const size_t action, const INPUT_BINDING binding)
{
    ASSERT(action < m_Context.desc->input_schema->count);
    ShellRequests_Add(
        &m_Context.requests,
        (SHELL_REQUEST) { SHELL_REQUEST_UNBIND, action, binding });
}

void Shell_RequestResetPadBindings(void)
{
    ShellRequests_Add(
        &m_Context.requests,
        (SHELL_REQUEST) { .type = SHELL_REQUEST_RESET_PAD });
}

bool Shell_GetTickRate(float *const out)
{
    if (m_Context.tick_rate_measured) {
        *out = m_Context.measured_tick_rate;
    }
    return m_Context.tick_rate_measured;
}

bool Shell_IsFullscreen(void)
{
    return m_Context.settings.fullscreen;
}

void Shell_RequestFullscreen(const bool fullscreen)
{
    m_Context.requests.fullscreen = fullscreen ? 1 : 0;
}

SHELL_STATUS Shell_Init(const GAME_DESC *const desc, int argc, char *argv[])
{
    m_Context.desc = desc;
    Fatal_Install(desc->id);
    bool done = false;
    const RESULT result = M_Init(&m_Context, argc, argv, &done);
    if (IS_OK(result)) {
        return done ? SHELL_SUCCESS : SHELL_CONTINUE;
    }
    char text[4096];
    if (m_Context.missing_data_file != nullptr) {
        Fatal_FormatMissingData(
            text, sizeof(text), m_Context.missing_data_file,
            m_Context.folders.data_dir, LogFile_GetPath());
    } else {
        Fatal_FormatStartupError(
            text, sizeof(text), result.message, LogFile_GetPath());
    }
    Result_Log(result, LOG_LEVEL_ERROR);
    Fatal_ShowStartupError(text);
    return SHELL_FAILURE;
}

SHELL_STATUS Shell_Event(const SDL_Event *const event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SHELL_SUCCESS;
    }
    if (event->type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN) {
        m_Context.settings.fullscreen = true;
    } else if (event->type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN) {
        m_Context.settings.fullscreen = false;
    }
    return SHELL_CONTINUE;
}

SHELL_STATUS Shell_Iterate(void)
{
    SHELL_CONTEXT *const ctx = &m_Context;

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
    if (ShellRequests_ReadPadPress(ctx->input, &press) && !ctx->pad_pressed) {
        ctx->pad_pressed = true;
        ctx->pad_press = press;
    }
    for (int i = 0; i < ticks; i++) {
        Gfx_BeginTick((GFX_COLOR) { 0, 0, 0, 255 });
        const bool running = ctx->desc->tick(ctx->input_buffer);
        Gfx_EndTick();
        ctx->pad_pressed = false;
        ctx->tick_rate_measured = false;
        M_ApplyRequests(ctx);
        if (!running) {
            return SHELL_SUCCESS;
        }
    }

    int width;
    int height;
    SDL_GetWindowSizeInPixels(ctx->window, &width, &height);
    const GFX_OUTPUT output = {
        .width = width,
        .height = height,
        .integer_scale = ctx->settings.scaling == SHELL_SCALING_INTEGER,
        .border = { 0, 0, 0, 255 },
        .filter = ctx->settings.scaling == SHELL_SCALING_SMOOTH
            ? GFX_FILTER_LINEAR
            : GFX_FILTER_NEAREST,
    };
    const float t = (float)((double)ctx->clock.pending / 1000000000.0);
    if (!Result_Log(
            Gfx_Present(
                ctx->settings.interpolation, t > 1.0f ? 1.0f : t, &output),
            LOG_LEVEL_ERROR)) {
        return SHELL_FAILURE;
    }
    if (ctx->screenshot_pending) {
        Result_Log(
            Screenshot_Save(
                ctx->folders.user_dir, ctx->desc->id, &ctx->settings),
            LOG_LEVEL_WARNING);
        ctx->screenshot_pending = false;
    }
    SDL_GL_SwapWindow(ctx->window);

    if (!ctx->vsync) {
        SDL_DelayNS(Clock_NsUntilNextTick(&ctx->clock));
    }
    return SHELL_CONTINUE;
}

void Shell_Quit(void)
{
    SHELL_CONTEXT *const ctx = &m_Context;
    if (ctx->game_init_called) {
        ctx->desc->shutdown();
    }
    Patch_CloseStack(ctx->patches);
    Audio_Shutdown();
    if (ctx->loaded_settings != nullptr && ctx->ini_path != nullptr) {
        M_SaveSettings(ctx);
    }
    Ini_Free(ctx->loaded_settings);
    Ini_Free(ctx->ini);
    Mem_Free(ctx->ini_path);
    Folders_Free(&ctx->folders);
    if (ctx->input_buffer != nullptr) {
        Allocator_Free(
            &g_HeapAllocator, ctx->input_buffer,
            ctx->desc->input_schema->size
                * (size_t)ctx->desc->input_schema->players);
    }
    Input_Destroy(ctx->input);
    Gfx_Shutdown();
    if (ctx->gl_context != nullptr) {
        SDL_GL_DestroyContext(ctx->gl_context);
    }
    if (ctx->window != nullptr) {
        SDL_DestroyWindow(ctx->window);
    }
    LogFile_Close();
}
