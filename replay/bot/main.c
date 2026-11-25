#include "recorder.h"

#include "bot.h"

#include "core/args.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "shell/null.h"

#include <stdio.h>
#include <string.h>

#define M_IDLE_FRAMES 90
#define M_BATCHES 3
#define M_BATCH_SIZE 16

typedef struct {
    const char *data;
    const char *out;
    ARG_ITEMS starts;
    int32_t seed;
    int32_t max_frames;
    const char *strategy;
    const char *bot;
    int32_t until_progress;
    int32_t segment;
    bool no_miss;
    int32_t budget;
} M_ARGS;

// Holds the input structs of a run, one per frame.
typedef struct {
    uint8_t *inputs;
    int32_t frames;
    int32_t capacity;
    size_t size;
} M_INPUTS;

// Holds the outcome of one candidate segment.
typedef struct {
    bool survived;
    bool ended;
    RECORDER_SCORE score;
} M_CANDIDATE;

// Holds a search: the kept run, the frame at which each kept segment ends,
// and per depth the searches made and the dead ends met there.
typedef struct {
    const RECORDER_GAME *game;
    const M_ARGS *args;
    const BOT *bot;
    M_INPUTS kept;
    M_INPUTS candidate;
    M_INPUTS best;
    int32_t *ends;
    int32_t *tries;
    int32_t *dead_ends;
    int32_t depth;
    int32_t script_frames;
    int32_t evaluated;
    void *none;
} M_SEARCH;

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
        "the frames after which the run fails, by default the game's",
        ARG_FALLBACK_INT(0)),
    ARG_STRING_OF(
        M_ARGS, "--strategy", strategy, "<name>",
        "the bot's strategy: dodge, or wander for a game over",
        ARG_FALLBACK("dodge")),
    ARG_STRING_OF(
        M_ARGS, "--bot", bot, "<file>",
        "the bot.ini that maps the bot's roles to the game's buttons"),
    ARG_INT_OF(
        M_ARGS, "--until-progress", until_progress, "<n>",
        "end the run once its score's first value reaches n",
        ARG_FALLBACK_INT(-1)),
    ARG_INT_OF(
        M_ARGS, "--segment", segment, "<n>",
        "grow the run n frames at a time with the best of several bots",
        ARG_FALLBACK_INT(0)),
    ARG_FLAG_OF(
        M_ARGS, "--no-miss", no_miss,
        "with --segment, drop each segment that has a setback"),
    ARG_INT_OF(
        M_ARGS, "--budget", budget, "<n>",
        "with --segment, the candidate segments after which the run fails",
        ARG_FALLBACK_INT(20000)),
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

int Recorder_CompareScores(const RECORDER_SCORE a, const RECORDER_SCORE b)
{
    const int32_t count = a.count < b.count ? a.count : b.count;
    for (int32_t i = 0; i < count; i++) {
        if (a.values[i] != b.values[i]) {
            return a.values[i] < b.values[i] ? -1 : 1;
        }
    }
    return 0;
}

static bool M_Reached(const RECORDER_GAME *const game, const M_ARGS *const args)
{
    return args->until_progress >= 0
        && game->score().values[0] >= args->until_progress;
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

// Derives the bot seed of one candidate, the same for the same arguments.
static uint16_t M_CandidateSeed(
    const int32_t seed, const int32_t depth, const int32_t attempt)
{
    uint32_t hash = (uint32_t)seed * 0x9e3779b1u;
    hash ^= (uint32_t)depth * 0x85ebca77u + 0x165667b1u;
    hash = (hash ^ hash >> 15) * 0x2c1b3c6du;
    hash ^= (uint32_t)attempt * 0xc2b2ae3du + 0x27d4eb2fu;
    hash = (hash ^ hash >> 12) * 0x297a2d39u;
    return (uint16_t)(hash ^ hash >> 16);
}

static RESULT M_Replay(const RECORDER_GAME *const game, const M_INPUTS *run)
{
    game->stop();
    Recorder_ResetClock();
    MUST(game->start());
    for (int32_t i = 0; i < run->frames; i++) {
        FAIL_IF(
            !game->tick(run->inputs + (size_t)i * run->size),
            "the game quit during the run");
    }
    return OK;
}

// Plays one candidate segment after the kept run from a fresh start, then
// the idle frames.
static RESULT M_Try(
    M_SEARCH *const search, const uint16_t seed, M_CANDIDATE *const out)
{
    const RECORDER_GAME *const game = search->game;
    MUST(M_Replay(game, &search->kept));
    const int32_t setbacks = game->setbacks();
    BOT bot = *search->bot;
    Rng_SetSeed(&bot.rng, seed);
    search->candidate.frames = 0;
    *out = (M_CANDIDATE) { .survived = true };
    VEC2 threats[BOT_MAX_THREATS];
    for (int32_t i = 0; i < search->args->segment && !out->ended; i++) {
        void *const input = M_Append(&search->candidate);
        const int32_t count = game->list_threats(threats, BOT_MAX_THREATS);
        Bot_NextInput(&bot, threats, count, input);
        FAIL_IF(!game->tick(input), "the game quit during the run");
        if (search->args->no_miss && game->setbacks() > setbacks) {
            out->survived = false;
            return OK;
        }
        out->ended = game->has_ended();
    }
    out->score = game->score();
    for (int32_t i = 0; i < M_IDLE_FRAMES && !game->has_ended(); i++) {
        if (!game->tick(search->none)) {
            break;
        }
    }
    out->survived = !search->args->no_miss || game->setbacks() <= setbacks;
    return OK;
}

static void M_Copy(M_INPUTS *const to, const M_INPUTS *const from)
{
    to->frames = 0;
    for (int32_t i = 0; i < from->frames; i++) {
        memcpy(M_Append(to), from->inputs + (size_t)i * from->size, from->size);
    }
}

// Searches the segment after the kept run into search->best. out->survived
// tells whether one survived.
static RESULT M_SearchSegment(M_SEARCH *const search, M_CANDIDATE *const out)
{
    const M_ARGS *const args = search->args;
    const int32_t depth = search->depth;
    *out = (M_CANDIDATE) {};
    for (int32_t batch = 0; batch < M_BATCHES && !out->survived; batch++) {
        for (int32_t i = 0; i < M_BATCH_SIZE; i++) {
            FAIL_IF(
                search->evaluated >= args->budget,
                "no run found in %d candidate segments", args->budget);
            search->evaluated++;
            const int32_t attempt =
                (search->tries[depth] * M_BATCHES + batch) * M_BATCH_SIZE + i;
            M_CANDIDATE candidate;
            MUST(M_Try(
                search, M_CandidateSeed(args->seed, depth, attempt),
                &candidate));
            if (candidate.survived
                && (!out->survived
                    || Recorder_CompareScores(candidate.score, out->score)
                        > 0)) {
                *out = candidate;
                M_Copy(&search->best, &search->candidate);
            }
        }
    }
    search->tries[depth]++;
    return OK;
}

// Grows the kept run, which holds the script's frames, by the search.
static RESULT M_RunSearch(M_SEARCH *const search)
{
    const RECORDER_GAME *const game = search->game;
    const M_ARGS *const args = search->args;
    M_CANDIDATE last = {
        .ended = game->has_ended(),
        .score = game->score(),
    };
    while (!last.ended
           && (args->until_progress < 0
               || last.score.values[0] < args->until_progress)) {
        FAIL_IF(
            search->kept.frames >= args->max_frames,
            "the run did not end in %d frames", args->max_frames);
        const int32_t depth = search->depth;
        M_CANDIDATE best;
        MUST(M_SearchSegment(search, &best));
        if (best.survived) {
            for (int32_t i = 0; i < search->best.frames; i++) {
                memcpy(
                    M_Append(&search->kept),
                    search->best.inputs + (size_t)i * search->best.size,
                    search->best.size);
            }
            search->ends[search->depth++] = search->kept.frames;
            last = best;
            printf(
                "kept %d frames, score %lld, %d candidates\n",
                search->kept.frames, (long long)last.score.values[0],
                search->evaluated);
            fflush(stdout);
            continue;
        }
        search->dead_ends[depth]++;
        const int32_t back =
            search->dead_ends[depth] < depth ? search->dead_ends[depth] : depth;
        search->depth -= back;
        search->kept.frames = search->depth > 0
            ? search->ends[search->depth - 1]
            : search->script_frames;
        printf("dead end, back to %d frames\n", search->kept.frames);
        fflush(stdout);
    }
    return OK;
}

static RESULT M_Search(
    const RECORDER_GAME *const game, const M_ARGS *const args,
    const BOT *const bot, M_INPUTS *const run)
{
    while (!game->has_ended()) {
        FAIL_IF(
            run->frames >= args->max_frames, "the script ran past %d frames",
            args->max_frames);
        void *const input = M_Append(run);
        if (!game->script_input(run->frames - 1, input)) {
            run->frames--;
            break;
        }
        FAIL_IF(!game->tick(input), "the game quit during the run");
    }
    const int32_t depths = args->max_frames / args->segment + 2;
    M_SEARCH search = {
        .game = game,
        .args = args,
        .bot = bot,
        .kept = *run,
        .candidate = { .size = run->size },
        .best = { .size = run->size },
        .ends = Mem_AllocArrZ((size_t)depths, sizeof(int32_t)),
        .tries = Mem_AllocArrZ((size_t)depths, sizeof(int32_t)),
        .dead_ends = Mem_AllocArrZ((size_t)depths, sizeof(int32_t)),
        .script_frames = run->frames,
        .none = Mem_AllocZ(run->size),
    };
    // Keeps the replays from repeating the first run's messages.
    const LOG_LEVEL level = Log_GetMinLevel();
    Log_SetMinLevel(LOG_LEVEL_ERROR);
    const RESULT result = M_RunSearch(&search);
    Log_SetMinLevel(level);
    *run = search.kept;
    Mem_Free(search.candidate.inputs);
    Mem_Free(search.best.inputs);
    Mem_Free(search.ends);
    Mem_Free(search.tries);
    Mem_Free(search.dead_ends);
    Mem_Free(search.none);
    MUST(result);
    return M_Replay(game, run);
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
        result = args->segment > 0 ? M_Search(game, args, &bot, &run)
                                   : M_RunStraight(game, args, &bot, &run);
    }
    if (IS_OK(result)) {
        result = game->save(args->out, run.inputs, run.frames);
    }
    if (IS_OK(result)) {
        printf(
            "saved %s after %d frames, %s, %d setbacks, score %lld\n",
            args->out, run.frames, game->has_ended() ? "ended" : "not ended",
            game->setbacks(), (long long)game->score().values[0]);
    }
    Mem_Free(run.inputs);
    return result;
}

static RESULT M_Main(const RECORDER_GAME *const game, M_ARGS *const args)
{
    if (args->max_frames <= 0) {
        args->max_frames = game->max_frames;
    }
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
