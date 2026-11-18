#include "session_native.h"

#include "session.h"
#include "runner/shell_null.h"

#include "core/args.h"
#include "core/base/memory.h"
#include "core/config.h"
#include "core/gfx.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/io/writer.h"
#include "core/patch.h"

#include <SDL3/SDL.h>
#include <string.h>

typedef struct {
    const char *data;
    const char *inputs;
    const char *out;
    const char *patches;
    const char *language;
    const char *stage_replay;
    const char *compare;
    const char *compare_state;
    bool record_state;
    bool headless;
    bool fast;
} M_ARGS;

typedef struct {
    SESSION_STATE *state;
    CASE_WRITER *writer;
    WRITER output;
    bool output_open;
    READER expected;
    bool expected_open;
    SESSION_COMPARE *compare;
    RUNNER_GAME game;
    RUNNER_FIELD *fields;
} M_TRACE;

static const char *m_Data;
static char *m_User;
static PATCH_STACK *m_Patches;

static SESSION_STATE *M_State(const RUNNER_GAME *const game)
{
    const int32_t count = game->start_count + game->calc_count;
    const char **const names = Mem_AllocArr((size_t)count, sizeof(*names));
    for (int32_t i = 0; i < game->start_count; i++) {
        names[i] = game->start_fields[i].name;
    }
    for (int32_t i = 0; i < game->calc_count; i++) {
        names[game->start_count + i] = game->calc_fields[i].name;
    }
    const SESSION_STATE_GAME schema = {
        .id = game->id,
        .start_names = names,
        .start_count = game->start_count,
        .calc_names = names + game->start_count,
        .calc_count = game->calc_count,
        .start = game->fill_start,
        .calc = game->fill_calc,
    };
    SESSION_STATE *const state = SessionState_Create(&schema);
    Mem_Free(names);
    return state;
}

static RESULT M_OpenTrace(
    const SESSION_GAME *const shim, const M_ARGS *const args,
    M_TRACE *const trace)
{
    if (!args->record_state && args->compare_state == nullptr) {
        return OK;
    }
    FAIL_IF(
        shim->point == nullptr || shim->comparison == nullptr,
        "game has no session state schema");
    trace->state = M_State(shim->comparison);
    if (args->record_state) {
        char *const path =
            Path_Join(&g_HeapAllocator, args->out, "session.otrc");
        const RESULT result = Writer_OpenFile(&trace->output, path);
        Mem_Free(path);
        MUST(result);
        trace->output_open = true;
        trace->writer = SessionState_OpenWriter(trace->state, &trace->output);
    }
    if (args->compare_state == nullptr) {
        return OK;
    }
    MUST(Reader_OpenFile(&trace->expected, args->compare_state));
    trace->expected_open = true;
    const CASE_NAMES *const columns = SessionState_Columns(trace->state);
    trace->fields =
        Mem_AllocArr((size_t)columns->count, sizeof(*trace->fields));
    const int32_t common = columns->count - shim->comparison->start_count
        - shim->comparison->calc_count;
    for (int32_t i = 0; i < columns->count; i++) {
        RUNNER_KIND kind = RUNNER_U32;
        if (i >= common + shim->comparison->start_count) {
            kind = shim->comparison
                       ->calc_fields[i - common - shim->comparison->start_count]
                       .kind;
        } else if (i >= common) {
            kind = shim->comparison->start_fields[i - common].kind;
        }
        trace->fields[i] =
            (RUNNER_FIELD) { .name = columns->items[i], .kind = kind };
    }
    trace->game.id = shim->game->id;
    trace->game.calc_fields = trace->fields;
    trace->game.calc_count = columns->count;
    return SessionCompare_Open(&trace->expected, &trace->game, &trace->compare);
}

static RESULT M_CloseTrace(M_TRACE *const trace)
{
    RESULT result = OK;
    if (trace->writer != nullptr) {
        result = Case_CloseWriter(trace->writer);
    }
    if (trace->output_open) {
        const RESULT closed = Writer_Close(&trace->output);
        if (IS_OK(result)) {
            result = closed;
        } else {
            Result_Ignore(closed);
        }
    }
    if (trace->compare != nullptr) {
        RUNNER_REPORT report;
        const RESULT compared = SessionCompare_Close(trace->compare, &report);
        if (IS_OK(result)) {
            result = compared;
        } else {
            Result_Ignore(compared);
        }
        if (IS_OK(result)) {
            fprintf(
                stderr, "matched %llu session calcs\n",
                (unsigned long long)report.rows);
        }
    }
    if (trace->expected_open) {
        Reader_Close(&trace->expected);
    }
    Mem_Free(trace->fields);
    SessionState_Free(trace->state);
    return result;
}

static RESULT M_StageReplay(
    const SESSION_GAME *const game, const char *const source)
{
    if (source == nullptr) {
        return OK;
    }
    READER reader;
    MUST(Reader_OpenFile(&reader, source));
    char *const folder = Path_Join(&g_HeapAllocator, m_User, "replay");
    RESULT result = File_MakeDir(folder);
    Mem_Free(folder);
    if (!IS_OK(result)) {
        Reader_Close(&reader);
        return result;
    }
    char *const target = Path_Join(&g_HeapAllocator, m_User, game->replay_copy);
    WRITER writer;
    result = Writer_OpenFile(&writer, target);
    Mem_Free(target);
    if (!IS_OK(result)) {
        Reader_Close(&reader);
        return result;
    }
    uint8_t buffer[65536];
    while (Reader_Remaining(&reader) != 0 && !Reader_Failed(&reader)) {
        const size_t size = MIN(Reader_Remaining(&reader), sizeof(buffer));
        Reader_ReadBytes(&reader, buffer, size);
        Writer_WriteBytes(&writer, buffer, size);
    }
    result = Reader_Result(&reader);
    Reader_Close(&reader);
    const RESULT written = Writer_Close(&writer);
    if (!IS_OK(result)) {
        Result_Ignore(written);
        return result;
    }
    return written;
}

const char *Shell_GetDataDir(void)
{
    return m_Data;
}

const char *Shell_GetUserDir(void)
{
    return m_User;
}

const PATCH_STACK *Shell_GetPatches(void)
{
    return m_Patches;
}

typedef struct {
    CAPTURE_SESSION session;
    SDL_Window *window;
    SDL_GLContext context;
    void *input;
    bool gfx_started;
    bool game_started;
    READER case_file;
    bool case_open;
    SESSION_COMPARE *compare;
    const SESSION_GAME *shim;
    bool headless;
    M_TRACE trace;
} M_NATIVE;

static bool M_CalcEnd(void *const arg)
{
    M_NATIVE *const state = arg;
    if (state->compare != nullptr) {
        SessionCompare_Calc(
            state->compare, state->shim->case_ready(),
            state->shim->case_ended());
    }
    M_TRACE *const trace = &state->trace;
    if (trace->state != nullptr) {
        SESSION_POINT point = {};
        state->shim->point(&point);
        point.frame = state->session.frame;
        const uint32_t *const row = SessionState_Sample(trace->state, &point);
        if (trace->writer != nullptr) {
            Case_WriteRow(trace->writer, row);
        }
        if (trace->compare != nullptr) {
            SessionCompare_Row(trace->compare, nullptr, row);
        }
    }
    return true;
}

static RESULT M_Start(
    const SESSION_GAME *const shim, const M_ARGS *const args,
    M_NATIVE *const state)
{
    MUST(Session_Open(
        &state->session, args->inputs, shim->bindings, shim->binding_count));
    MUST(
        Session_SetActions(&state->session, shim->actions, shim->action_count));
    state->headless = args->headless;
    state->session.fast = args->fast;
    state->session.screenshots = !args->headless;
    state->shim = shim;
    if (args->compare != nullptr) {
        MUST(Reader_OpenFile(&state->case_file, args->compare));
        state->case_open = true;
        MUST(SessionCompare_Open(
            &state->case_file, shim->comparison, &state->compare));
    }
    FAIL_IF(File_Exists(args->out), "--out must name a new directory");
    MUST(File_MakeDir(args->out));
    MUST(M_OpenTrace(shim, args, &state->trace));
    m_User = Path_Join(&g_HeapAllocator, args->out, "saves");
    MUST(File_MakeDir(m_User));
    MUST(M_StageReplay(shim, args->stage_replay));
    m_Data = args->data;
    const GAME_DESC *const game = shim->game;
    if (args->patches != nullptr) {
        const PATCH_CHOICE choice = { .enabled = true,
                                      .language = args->language };
        MUST(Patch_OpenStack(
            args->patches, game->id, game->patch_game, &choice, &m_Patches));
    }
    FAIL_IF(
        !SDL_Init(args->headless ? 0 : SDL_INIT_VIDEO), "%s", SDL_GetError());
    if (!args->headless) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(
            SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        state->window =
            SDL_CreateWindow("opentouhou capture", 640, 480, SDL_WINDOW_OPENGL);
        FAIL_IF(state->window == nullptr, "%s", SDL_GetError());
        state->context = SDL_GL_CreateContext(state->window);
        FAIL_IF(state->context == nullptr, "%s", SDL_GetError());
        FAIL_IF(
            !SDL_SetWindowPosition(state->window, 0, 0), "%s", SDL_GetError());
        FAIL_IF(!SDL_SyncWindow(state->window), "%s", SDL_GetError());
        SDL_GL_SetSwapInterval(0);
        MUST(Gfx_Init(state->window, 640, 480));
        state->gfx_started = true;
    }
    Config_SetDefaults(game->settings_schema, game->settings);
    RunnerShell_SetInputSize(game->input_schema->size);
    state->game_started = true;
    MUST(game->init());
    shim->prepare();
    if (state->compare != nullptr || state->trace.state != nullptr) {
        shim->comparison->chain->on_calc_end = M_CalcEnd;
        shim->comparison->chain->on_calc_end_arg = state;
    }
    state->input = Mem_Alloc(game->input_schema->size);
    return OK;
}

static RESULT M_Play(const SESSION_GAME *const shim, M_NATIVE *const state)
{
    const GAME_DESC *const game = shim->game;
    CAPTURE_SESSION *const session = &state->session;
    const GFX_OUTPUT output = { .width = 640,
                                .height = 480,
                                .filter = GFX_FILTER_NEAREST };
    RESULT result = OK;
    while (!Session_Done(session)) {
        if (!state->headless) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    return FAIL("capture window was closed");
                }
            }
        }
        shim->set_buttons(
            state->input, Script_Buttons(&session->script, session->frame));
        Gfx_BeginTick((GFX_COLOR) { 0, 0, 0, 255 });
        const bool running = game->tick(state->input);
        Gfx_EndTick();
        if (state->compare != nullptr
            && shim->comparison->chain->on_calc_end != M_CalcEnd) {
            return FAIL("game reset during session comparison");
        }
        if (state->trace.state != nullptr) {
            shim->comparison->chain->on_calc_end = M_CalcEnd;
            shim->comparison->chain->on_calc_end_arg = state;
        }
        if (!state->headless
            && (!session->fast
                || Script_Capture(&session->script, session->frame))) {
            result = Gfx_Present(false, 1, &output);
            if (!IS_OK(result)) {
                break;
            }
            SDL_GL_SwapWindow(state->window);
        }
        if (!running) {
            result = FAIL("game quit at frame %u", session->frame);
            break;
        }
        int x = 0, y = 0, width = 0, height = 0;
        if (!state->headless) {
            FAIL_IF(
                !SDL_GetWindowPosition(state->window, &x, &y)
                    || !SDL_GetWindowSize(state->window, &width, &height),
                "%s", SDL_GetError());
        }
        result = Session_Presented(session, x, y, width, height);
        if (!IS_OK(result)) {
            break;
        }
    }
    return result;
}

static RESULT M_Run(const SESSION_GAME *const shim, const M_ARGS *const args)
{
    M_NATIVE state = {};
    RESULT result = M_Start(shim, args, &state);
    if (IS_OK(result)) {
        result = M_Play(shim, &state);
    }
    if (state.compare != nullptr) {
        if (state.game_started) {
            shim->comparison->chain->on_calc_end = nullptr;
            shim->comparison->chain->on_calc_end_arg = nullptr;
        }
        RUNNER_REPORT report;
        const RESULT compared = SessionCompare_Close(state.compare, &report);
        if (IS_OK(result)) {
            result = compared;
        } else {
            Result_Ignore(compared);
        }
        if (IS_OK(result)) {
            fprintf(
                stderr, "matched %llu scripted gameplay calcs\n",
                (unsigned long long)report.rows);
        }
    }
    if (state.case_open) {
        Reader_Close(&state.case_file);
    }
    const RESULT traced = M_CloseTrace(&state.trace);
    if (IS_OK(result)) {
        result = traced;
    } else {
        Result_Ignore(traced);
    }
    if (state.game_started) {
        shim->comparison->chain->on_calc_end = nullptr;
        shim->comparison->chain->on_calc_end_arg = nullptr;
    }
    Mem_Free(state.input);
    if (state.game_started) {
        shim->game->shutdown();
    }
    if (state.gfx_started) {
        Gfx_Shutdown();
    }
    if (state.context != nullptr) {
        SDL_GL_DestroyContext(state.context);
    }
    if (state.window != nullptr) {
        SDL_DestroyWindow(state.window);
    }
    SDL_Quit();
    if (m_Patches != nullptr) {
        Patch_CloseStack(m_Patches);
        m_Patches = nullptr;
    }
    Mem_Free(m_User);
    m_User = nullptr;
    Session_Close(&state.session);
    return result;
}

static int M_NativeMain(
    const SESSION_GAME *const game, const int argc, char **const argv)
{
    const ARG_OPTION options[] = {
        ARG_STRING_OF(
            M_ARGS, "--data", data, nullptr, "<dir>", "original game data",
            ARG_REQUIRED),
        ARG_STRING_OF(
            M_ARGS, "--inputs", inputs, nullptr, "<file>", "OTINPUT script",
            ARG_REQUIRED),
        ARG_STRING_OF(
            M_ARGS, "--out", out, nullptr, "<dir>", "new session directory",
            ARG_REQUIRED),
        ARG_STRING_OF(
            M_ARGS, "--patches", patches, nullptr, "<dir>",
            "installed patch repositories"),
        ARG_STRING_OF(
            M_ARGS, "--language", language, "en", "<code>", "patch language"),
        ARG_STRING_OF(
            M_ARGS, "--stage-replay", stage_replay, nullptr, "<file.rpy>",
            "place a replay in the first menu slot"),
        ARG_STRING_OF(
            M_ARGS, "--compare", compare, nullptr, "<case.otrc>",
            "compare this scripted session's gameplay calcs"),
        ARG_FLAG_OF(
            M_ARGS, "--headless", headless,
            "run the same input without a window or screenshots"),
        ARG_FLAG_OF(
            M_ARGS, "--fast", fast, "run without real-time frame waits"),
        ARG_FLAG_OF(
            M_ARGS, "--record-state", record_state,
            "record every scene's calc state in session.otrc"),
        ARG_STRING_OF(
            M_ARGS, "--compare-state", compare_state, nullptr, "<trace>",
            "compare every scene's calc state with a recorded session"),
    };
    const ARG_SCHEMA schema = ARG_SCHEMA_OF(
        "session",
        "Runs scripted input through the native game and captures its window.",
        options);
    M_ARGS args;
    bool help;
    if (!Result_Log(
            Args_Parse(&schema, argc, argv, &args, &help), LOG_LEVEL_ERROR)) {
        return 1;
    }
    if (help) {
        Args_PrintHelp(&schema, stdout);
        return 0;
    }
    return Result_Log(M_Run(game, &args), LOG_LEVEL_ERROR) ? 0 : 1;
}

int Session_Main(
    const SESSION_GAME *const game, const int argc, char **const argv)
{
    if (argc > 1 && strcmp(argv[1], "session") == 0) {
        return M_NativeMain(game, argc - 1, argv + 1);
    }
    return Runner_Main(game->comparison, argc, argv);
}
