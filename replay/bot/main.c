#include "recorder.h"

#include "bot.h"

#include "core/args.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "shell/null.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *data;
    const char *out;
    ARG_ITEMS starts;
    int32_t seed;
    int32_t max_frames;
    const char *strategy;
    const char *bot;
    int32_t until_progress;
} M_ARGS;

// Holds the input structs of a run, one per frame.
typedef struct {
    uint8_t *inputs;
    int32_t frames;
    int32_t capacity;
    size_t size;
} M_INPUTS;

static const ARG_OPTION m_Options[] = {
    ARG_STRING_OF(
        M_ARGS, "--data", data, "<dir>", "the game's directory", ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "--out", out, "<file>", "the file to write", ARG_REQUIRED),
    ARG_LIST_OF(
        M_ARGS, "--start", starts, "<name=value>",
        "a start value of the run, such as stage=1; repeatable"),
    ARG_INT_OF(
        M_ARGS, "--seed", seed, "<n>", "the bot's own RNG seed",
        ARG_FALLBACK_INT(1)),
    ARG_INT_OF(
        M_ARGS, "--max-frames", max_frames, "<n>",
        "the frames after which the run fails", ARG_FALLBACK_INT(216000)),
    ARG_STRING_OF(
        M_ARGS, "--strategy", strategy, "<name>",
        "the bot's strategy: dodge, or wander for a game over",
        ARG_FALLBACK("dodge")),
    ARG_STRING_OF(
        M_ARGS, "--bot", bot, "<file>",
        "the bot.ini that maps the bot's roles to the game's buttons"),
    ARG_INT_OF(
        M_ARGS, "--until-progress", until_progress, "<n>",
        "end the run once its progress reaches n", ARG_FALLBACK_INT(-1)),
};

static void *M_Append(M_INPUTS *const run)
{
    if (run->frames == run->capacity) {
        run->capacity = run->capacity == 0 ? 4096 : run->capacity * 2;
        run->inputs =
            Mem_Realloc(run->inputs, (size_t)run->capacity * run->size);
    }
    void *const input = run->inputs + (size_t)run->frames * run->size;
    run->frames++;
    return input;
}

static bool M_Reached(const RECORDER_GAME *const game, const M_ARGS *const args)
{
    return args->until_progress >= 0
        && game->state().progress >= args->until_progress;
}

// Plays the run from the game's start until it ends, with the script's
// frames first and the bot's after them.
static RESULT M_RunStraight(
    const RECORDER_GAME *const game, const M_ARGS *const args, BOT *const bot,
    M_INPUTS *const run)
{
    VEC2 threats[BOT_MAX_THREATS];
    while (!game->has_ended() && !M_Reached(game, args)) {
        FAIL_IF(
            run->frames >= args->max_frames, "the run did not end in %d frames",
            args->max_frames);
        void *const input = M_Append(run);
        if (!game->script_input(run->frames - 1, input)) {
            const int32_t count = game->list_threats(threats, BOT_MAX_THREATS);
            Bot_NextInput(bot, threats, count, input);
        }
        FAIL_IF(!game->tick(input), "the game quit during the run");
    }
    return OK;
}

static RESULT M_SetStarts(
    const RECORDER_GAME *const game, const M_ARGS *const args)
{
    for (size_t i = 0; i < args->starts.count; i++) {
        const char *const item = args->starts.items[i];
        const char *const equals = strchr(item, '=');
        FAIL_IF(equals == nullptr, "--start takes name=value, not %s", item);
        char name[64];
        FAIL_IF(
            (size_t)(equals - item) >= sizeof(name), "--start %s is too long",
            item);
        memcpy(name, item, (size_t)(equals - item));
        name[equals - item] = '\0';
        MUST(game->set_start(name, equals + 1), "--start %s", item);
    }
    return OK;
}

static RESULT M_Run(const RECORDER_GAME *const game, const M_ARGS *const args)
{
    const BOT_STRATEGY *const strategy = Bot_FindStrategy(args->strategy);
    FAIL_IF(strategy == nullptr, "there is no strategy %s", args->strategy);
    BOT bot;
    MUST(Bot_Load(
        &bot, args->bot != nullptr ? args->bot : game->bot_ini,
        game->input_schema, strategy, (uint16_t)args->seed));
    M_INPUTS run = { .size = game->input_schema->size };
    RESULT result = game->start();
    if (IS_OK(result)) {
        result = M_RunStraight(game, args, &bot, &run);
    }
    if (IS_OK(result)) {
        result = game->save(args->out, run.inputs, run.frames);
    }
    if (IS_OK(result)) {
        const RECORDER_STATE state = game->state();
        printf(
            "saved %s after %d frames, %s, %d lives, progress %d\n", args->out,
            run.frames, game->has_ended() ? "ended" : "not ended", state.lives,
            state.progress);
    }
    Mem_Free(run.inputs);
    return result;
}

static RESULT M_Main(const RECORDER_GAME *const game, const M_ARGS *const args)
{
    MUST(M_SetStarts(game, args));
    NullShell_SetInputSize(
        game->input_schema->size * (size_t)game->input_schema->players);
    RESULT result = NullShell_OpenFiles(args->data, nullptr);
    if (IS_OK(result)) {
        result = M_Run(game, args);
    }
    game->stop();
    NullShell_CloseFiles();
    return result;
}

int Recorder_Main(
    const RECORDER_GAME *const game, const int argc, char **const argv)
{
    const ARG_SCHEMA schema = ARG_SCHEMA_OF(
        game->program, "Plays a run with a bot and saves it.", m_Options);
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
    return Result_Log(M_Main(game, &args), LOG_LEVEL_ERROR) ? 0 : 1;
}
