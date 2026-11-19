#include "native.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/config.h"
#include "core/gfx.h"
#include "shell/game.h"
#include "shell/null.h"

#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define M_ARENA_SIZE (1024 * 1024)

typedef struct {
    uint32_t mask;
    size_t offset;
} M_BUTTON;

// Holds a native run; the shell/game functions below read it.
typedef struct {
    const NATIVE_PORT *port;
    RUN run;
    ARENA arena;
    SDL_Window *window;
    SDL_GLContext gl;
    bool game_started;
    DRIVER *driver;
    M_BUTTON *buttons;
    size_t button_count;
    void *input;
    size_t input_size;
    uint32_t frame;
    // Holds the rate that the game was last shown, and whether the shell
    // reports a rate to the game's next tick.
    FPS shown;
    bool rate_pending;
} M_NATIVE;

static M_NATIVE *m_Native;

bool Shell_GetTickRate(float *const out)
{
    if (m_Native->rate_pending) {
        *out = m_Native->shown.rate;
    }
    return m_Native->rate_pending;
}

uint64_t Shell_GetWallClock(void)
{
    const double frame_rate = m_Native->port->port.game->frame_rate;
    return (uint64_t)ceil((double)m_Native->frame * 1000.0 / frame_rate);
}

static RESULT M_MapButtons(M_NATIVE *const native)
{
    const HARNESS_GAME *const game = native->port->port.game;
    const INPUT_SCHEMA *const schema = native->port->game->input_schema;
    native->buttons =
        Arena_AllocArrZ(&native->arena, game->binding_count, sizeof(M_BUTTON));
    native->button_count = game->binding_count;
    for (size_t i = 0; i < game->binding_count; i++) {
        const BINDING *const binding = &game->bindings[i];
        size_t found = schema->count;
        for (size_t j = 0; j < schema->count; j++) {
            if (schema->actions[j].type == INPUT_ACTION_BUTTON
                && strcmp(schema->actions[j].name, binding->name) == 0) {
                found = j;
                break;
            }
        }
        FAIL_IF(
            found == schema->count, "%s has no button %s", game->id,
            binding->name);
        native->buttons[i] = (M_BUTTON) {
            .mask = binding->mask,
            .offset = schema->actions[found].offset,
        };
    }
    native->input_size = schema->size * (size_t)schema->players;
    native->input = Arena_AllocZ(&native->arena, native->input_size);
    NullShell_SetInputSize(native->input_size);
    return OK;
}

static void M_SetInput(M_NATIVE *const native, const uint32_t buttons)
{
    memset(native->input, 0, native->input_size);
    for (size_t i = 0; i < native->button_count; i++) {
        bool *const held =
            (bool *)((char *)native->input + native->buttons[i].offset);
        *held = (buttons & native->buttons[i].mask) != 0;
    }
}

// Measures one rate per half second of frames, which on the frame clock is
// the frame rate itself, and passes it through Driver_Fps.
static void M_UpdateRate(M_NATIVE *const native)
{
    const HARNESS_GAME *const game = native->port->port.game;
    const uint32_t period =
        game->frame_rate >= 2 ? (uint32_t)(game->frame_rate / 2) : 1;
    const bool measured = native->frame >= period;
    FPS fps = { .available = measured, .rate = (float)game->frame_rate };
    if (game->shows_fps) {
        fps = Driver_Fps(native->driver, fps);
    }
    native->rate_pending = fps.available
        && (native->frame % period == 0 || !native->shown.available
            || fps.rate != native->shown.rate);
    native->shown = fps;
}

static RESULT M_OpenVideo(M_NATIVE *const native)
{
    // Prefers the offscreen driver, which needs no desktop, and falls back
    // to the platform's driver with a hidden window.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
        FAIL_IF(
            !SDL_Init(SDL_INIT_VIDEO), "Cannot initialize SDL: %s",
            SDL_GetError());
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    const GAME_DESC *const desc = native->port->game;
    native->window = SDL_CreateWindow(
        desc->title, desc->logical_width, desc->logical_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    FAIL_IF(
        native->window == nullptr, "Cannot create the window: %s",
        SDL_GetError());
    native->gl = SDL_GL_CreateContext(native->window);
    FAIL_IF(
        native->gl == nullptr, "Cannot create an OpenGL 3.3 context: %s",
        SDL_GetError());
    return Gfx_Init(native->window, desc->logical_width, desc->logical_height);
}

static RESULT M_Start(M_NATIVE *const native)
{
    const NATIVE_PORT *const port = native->port;
    const RUN *const run = &native->run;
    FAIL_IF(run->live, "a native run cannot be live");
    MUST(M_MapButtons(native));
    MUST(Driver_Open(&port->port, run, "native", &native->driver));
    MUST(NullShell_OpenFiles(run->game_dir, nullptr));
    Driver_MountStaged(native->driver, Shell_GetFiles());
    Driver_MountStaged(native->driver, Shell_GetUserFiles());
    MUST(Driver_MountWork(native->driver, Shell_GetUserFiles()));
    if (!run->headless) {
        MUST(M_OpenVideo(native));
    }
    if (port->game->settings_schema != nullptr) {
        Config_SetDefaults(port->game->settings_schema, port->game->settings);
    }
    native->game_started = true;
    MUST(port->game->init(), "%s failed to start", port->game->id);
    port->port.seed(port->port.context);
    return OK;
}

static RESULT M_Frame(M_NATIVE *const native)
{
    M_SetInput(native, Driver_Buttons(native->driver, 0));
    M_UpdateRate(native);
    native->port->hook(native->port->port.context, native->driver);
    const bool draw = Driver_WantsDraw(native->driver);
    if (draw) {
        Gfx_BeginTick((GFX_COLOR) { 0, 0, 0, 255 });
    }
    const bool running = native->port->game->tick(native->input);
    if (draw) {
        Gfx_EndTick();
    }
    FAIL_IF(!running, "the game quit at frame %u", native->frame);
    IMAGE screen = {};
    const bool shot = Driver_WantsShot(native->driver);
    if (shot) {
        MUST(Gfx_ReadScreen(&g_HeapAllocator, &screen));
    }
    const RESULT result =
        Driver_EndFrame(native->driver, shot ? &screen : nullptr);
    if (shot) {
        Image_Free(&screen);
    }
    native->frame++;
    return result;
}

static void M_Stop(M_NATIVE *const native)
{
    if (native->game_started) {
        native->port->game->shutdown();
    }
    NullShell_CloseFiles();
    Gfx_Shutdown();
    if (native->gl != nullptr) {
        SDL_GL_DestroyContext(native->gl);
    }
    if (native->window != nullptr) {
        SDL_DestroyWindow(native->window);
    }
    SDL_Quit();
}

int Native_Main(
    const NATIVE_PORT *const port, const int argc, char **const argv)
{
    M_NATIVE native = { .port = port };
    Arena_Init(&native.arena, "native run", M_ARENA_SIZE);
    m_Native = &native;
    SDL_SetMainReady();
    bool help = false;
    char program[32];
    snprintf(program, sizeof(program), "%snative", port->port.game->id);
    RESULT result =
        Run_Parse(program, argc, argv, &native.arena, &native.run, &help);
    if (IS_OK(result) && !help) {
        result = M_Start(&native);
    }
    while (IS_OK(result) && !help && !Driver_Done(native.driver)) {
        result = M_Frame(&native);
    }
    M_Stop(&native);
    bool differs = false;
    if (native.driver != nullptr) {
        const RESULT closed =
            Driver_Close(native.driver, IS_OK(result) && !help, &differs);
        if (IS_OK(result)) {
            result = closed;
        } else {
            Result_Ignore(closed);
        }
    }
    m_Native = nullptr;
    Arena_Free(&native.arena);
    if (!Result_Log(result, LOG_LEVEL_ERROR)) {
        return 2;
    }
    return differs ? 1 : 0;
}
