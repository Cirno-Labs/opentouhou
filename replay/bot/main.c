#include "recorder.h"

#include "bot.h"

#include "core/args.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "shell/null.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *data;
    const char *out;
    const char *difficulty;
    const char *stage;
    const char *character;
    const char *shot_type;
    int32_t seed;
    int32_t max_frames;
    bool passive;
} M_ARGS;

static const ARG_OPTION m_Options[] = {
    ARG_STRING_OF(
        M_ARGS, "--data", data, "<dir>", "the game's directory", ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "--out", out, "<file>", "the .rpy file to write", ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "--difficulty", difficulty, "<name>", "the difficulty",
        ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "--stage", stage, "<name>", "the practice stage", ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "--character", character, "<name>", "the character",
        ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "--shot", shot_type, "<name>", "the shot type", ARG_REQUIRED),
    ARG_INT_OF(
        M_ARGS, "--seed", seed, "<n>", "the bot's own RNG seed",
        ARG_FALLBACK_INT(1)),
    ARG_INT_OF(
        M_ARGS, "--max-frames", max_frames, "<n>",
        "the calcs after which the run fails", ARG_FALLBACK_INT(216000)),
    ARG_FLAG_OF(
        M_ARGS, "--passive", passive,
        "wander without dodging, focusing or bombing, for a game over"),
};

// Finds name among the names of choice, without regard to case.
static RESULT M_Choose(
    const ARG_SCHEMA *const schema, const char *const option,
    const RECORDER_CHOICE *const choice, const char *const name,
    int32_t *const out)
{
    char names[256] = "";
    for (int32_t i = 0; i < choice->count; i++) {
        if (Str_EqualIgnoreCase(choice->names[i], name)) {
            *out = i;
            return OK;
        }
        snprintf(
            names + strlen(names), sizeof(names) - strlen(names), "%s%s",
            i > 0 ? ", " : "", choice->names[i]);
    }
    return Args_Fail(schema, "%s takes one of %s, not %s", option, names, name);
}

static RESULT M_Run(const RECORDER_GAME *const game, const M_ARGS *const args)
{
    BOT bot;
    Bot_Init(&bot, game->input_schema, (uint16_t)args->seed, args->passive);
    uint8_t *const input = Mem_AllocZ(game->input_schema->size);
    VEC2 threats[BOT_MAX_THREATS];
    RECORDER_END end;
    int32_t frame = 0;
    RESULT result = OK;
    while (IS_OK(result) && !game->has_ended(&end)) {
        if (frame >= args->max_frames) {
            result = FAIL("the run did not end in %d calcs", args->max_frames);
            break;
        }
        const int32_t count = game->list_threats(threats, BOT_MAX_THREATS);
        Bot_NextInput(&bot, threats, count, input);
        if (!game->tick(input)) {
            result = FAIL("the game quit during the run");
        }
        frame++;
    }
    Mem_Free(input);
    MUST(result);
    MUST(game->save(args->out));
    printf(
        "saved %s after %d calcs, %s\n", args->out, frame,
        end == RECORDER_STAGE_CLEARED ? "stage cleared" : "game over");
    return OK;
}

static RESULT M_Main(
    const RECORDER_GAME *const game, const ARG_SCHEMA *const schema,
    const M_ARGS *const args)
{
    int32_t difficulty;
    int32_t stage;
    int32_t character;
    int32_t shot_type;
    MUST(M_Choose(
        schema, "--difficulty", &game->difficulties, args->difficulty,
        &difficulty));
    MUST(M_Choose(schema, "--stage", &game->stages, args->stage, &stage));
    MUST(M_Choose(
        schema, "--character", &game->characters, args->character, &character));
    MUST(M_Choose(
        schema, "--shot", &game->shot_types, args->shot_type, &shot_type));
    NullShell_SetInputSize(
        game->input_schema->size * (size_t)game->input_schema->players);
    RESULT result = NullShell_OpenFiles(args->data, nullptr);
    if (IS_OK(result)) {
        result = game->start(difficulty, stage, character, shot_type);
    }
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
        game->program,
        "Plays a practice stage with a bot and saves it as a replay file.",
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
    return Result_Log(M_Main(game, &schema, &args), LOG_LEVEL_ERROR) ? 0 : 1;
}
