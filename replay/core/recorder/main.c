#include "../recorder.h"

#include "../runner/shell_null.h"
#include "bot.h"

#include "core/args.h"
#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/math/vec.h"

#include <stdint.h>
#include <stdio.h>

typedef struct {
    const char *data;
    const char *out;
    int32_t difficulty;
    int32_t stage;
    int32_t character;
    int32_t shot_type;
    int32_t seed;
    int32_t max_frames;
    bool passive;
} M_ARGS;

static const ARG_OPTION m_Options[] = {
    ARG_STRING_OF(
        M_ARGS, "--data", data, nullptr, "<dir>",
        "the folder of the original data", ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "--out", out, nullptr, "<file>", "the .rpy file to write",
        ARG_REQUIRED),
    ARG_INT_OF(
        M_ARGS, "--difficulty", difficulty, 1, "<0-5>",
        "0 Easy to 3 Lunatic, 4 Extra, 5 Phantasm"),
    ARG_INT_OF(
        M_ARGS, "--stage", stage, 1, "<1-8>",
        "the practice stage, 7 Extra, 8 Phantasm"),
    ARG_INT_OF(
        M_ARGS, "--character", character, 0, "<0-2>",
        "0 Reimu, 1 Marisa, 2 Sakuya"),
    ARG_INT_OF(M_ARGS, "--shot", shot_type, 0, "<0-1>", "0 A, 1 B"),
    ARG_INT_OF(M_ARGS, "--seed", seed, 1, "<n>", "the bot's own RNG seed"),
    ARG_INT_OF(
        M_ARGS, "--max-frames", max_frames, 216000, "<n>",
        "gives up after this many calcs"),
    ARG_FLAG_OF(
        M_ARGS, "--passive", passive,
        "never focuses or bombs by a nearby threat, for a game-over run"),
};

static RESULT M_Run(const RECORDER_GAME *const game, const M_ARGS *const args)
{
    RunnerShell_SetInputSize(game->input_schema->size);
    MUST(game->open_data(args->data));
    MUST(game->init());
    MUST(game->start_practice(
        args->difficulty, args->stage - 1, args->character, args->shot_type));

    BOT bot;
    Bot_Init(&bot, game->input_schema, (uint16_t)args->seed, args->passive);
    uint8_t *const input = Mem_Alloc(game->input_schema->size);
    int32_t frame = 0;
    RECORDER_END end;
    VEC2 threats[BOT_MAX_THREATS];
    while (!game->has_ended(&end)) {
        FAIL_IF(
            frame >= args->max_frames, "the run did not end within %d calcs",
            args->max_frames);
        const int32_t count = game->list_threats(threats, BOT_MAX_THREATS);
        Bot_NextInput(&bot, threats, count, input);
        FAIL_IF(!game->tick(input), "the game quit during practice");
        frame++;
    }
    Mem_Free(input);

    MUST(game->save_replay(args->out));
    printf(
        "saved %s after %d calcs, %s\n", args->out, frame,
        end == RECORDER_STAGE_CLEARED ? "stage cleared" : "game over");

    game->shutdown();
    game->close_data();
    return OK;
}

int Recorder_Main(
    const RECORDER_GAME *const game, const int argc, char **const argv)
{
    const ARG_SCHEMA schema = ARG_SCHEMA_OF(
        game->program,
        "Plays a practice stage with a deterministic bot and saves the "
        "recording as a .rpy.",
        m_Options);
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
