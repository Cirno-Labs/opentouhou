#include "recorder.h"

#include "bot.h"

#include "core/args.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "shell/null.h"

#include <stdio.h>
#include <string.h>

#define M_SEGMENT_FRAMES 150
#define M_IDLE_FRAMES 90
#define M_BATCHES 3
#define M_BATCH_SIZE 16
#define M_BEAM 4
#define M_BUDGET 20000

typedef struct {
    const char *data;
    const char *out;
    ARG_ITEMS starts;
    int32_t seed;
    int32_t until;
    bool lose;
} M_ARGS;

// Holds the input structs of a run, one per frame.
typedef struct {
    uint8_t *inputs;
    int32_t frames;
    int32_t capacity;
    size_t size;
} M_INPUTS;

// Holds a candidate segment that survived and how far its run got.
typedef struct {
    M_INPUTS segment;
    RECORDER_SCORE score;
    bool ended;
} M_NODE;

// Holds one step of the search: its survivors, best first, the one in the
// kept run, the frame at which the kept run ends after it, and the
// searches made and the dead ends met at its depth.
typedef struct {
    M_NODE nodes[M_BEAM];
    int32_t count;
    int32_t kept;
    int32_t end;
    int32_t tries;
    int32_t dead_ends;
} M_STEP;

typedef struct {
    const RECORDER_GAME *game;
    const M_ARGS *args;
    const BOT *bot;
    M_INPUTS kept;
    M_INPUTS candidate;
    M_STEP *steps;
    int32_t step_capacity;
    int32_t depth;
    int32_t script_frames;
    int32_t evaluated;
    int32_t dead_ends;
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
        M_ARGS, "--seed", seed, "<n>", "the seed of the bot's own RNGs",
        ARG_FALLBACK_INT(1)),
    ARG_INT_OF(
        M_ARGS, "--until", until, "<n>",
        "end the run once its score's first value reaches n",
        ARG_FALLBACK_INT(-1)),
    ARG_FLAG_OF(
        M_ARGS, "--lose", lose,
        "wander straight to the game's end, for a game over"),
};

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

static void M_AppendAll(M_INPUTS *const to, const M_INPUTS *const from)
{
    for (int32_t i = 0; i < from->frames; i++) {
        memcpy(M_Append(to), from->inputs + (size_t)i * from->size, from->size);
    }
}

// Plays the run from the game's start until it ends, with the script's
// frames first and the bot's after them.
static RESULT M_RunStraight(
    const RECORDER_GAME *const game, BOT *const bot, M_INPUTS *const run)
{
    VEC2 threats[BOT_MAX_THREATS];
    while (!game->has_ended()) {
        FAIL_IF(
            run->frames >= game->max_frames, "the run did not end in %d frames",
            game->max_frames);
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

static bool M_Better(const M_NODE *const a, const M_NODE *const b)
{
    return Recorder_CompareScores(a->score, b->score) > 0;
}

// Plays one candidate segment after the kept run from a fresh start, then
// the idle frames, and adds it to the beam of step when it survives.
static RESULT M_Try(
    M_SEARCH *const search, M_STEP *const step,
    const BOT_STRATEGY *const strategy, const uint16_t seed)
{
    const RECORDER_GAME *const game = search->game;
    MUST(M_Replay(game, &search->kept));
    const int32_t setbacks = game->setbacks();
    BOT bot = *search->bot;
    bot.strategy = strategy;
    Rng_SetSeed(&bot.rng, seed);
    M_INPUTS *const candidate = &search->candidate;
    candidate->frames = 0;
    const int32_t frames =
        game->max_frames - search->kept.frames < M_SEGMENT_FRAMES
        ? game->max_frames - search->kept.frames
        : M_SEGMENT_FRAMES;
    M_NODE node = {};
    VEC2 threats[BOT_MAX_THREATS];
    for (int32_t i = 0; i < frames && !node.ended; i++) {
        void *const input = M_Append(candidate);
        const int32_t count = game->list_threats(threats, BOT_MAX_THREATS);
        Bot_NextInput(&bot, threats, count, input);
        FAIL_IF(!game->tick(input), "the game quit during the run");
        if (game->setbacks() > setbacks) {
            return OK;
        }
        node.ended = game->has_ended();
    }
    node.score = game->score();
    for (int32_t i = 0; i < M_IDLE_FRAMES && !game->has_ended(); i++) {
        if (!game->tick(search->none)) {
            break;
        }
    }
    if (game->setbacks() > setbacks) {
        return OK;
    }
    int32_t slot = step->count;
    if (slot == M_BEAM) {
        slot = M_BEAM - 1;
        if (!M_Better(&node, &step->nodes[slot])) {
            return OK;
        }
    } else {
        step->count++;
    }
    M_NODE *const nodes = step->nodes;
    nodes[slot].score = node.score;
    nodes[slot].ended = node.ended;
    nodes[slot].segment.frames = 0;
    M_AppendAll(&nodes[slot].segment, candidate);
    for (; slot > 0 && M_Better(&nodes[slot], &nodes[slot - 1]); slot--) {
        const M_NODE swap = nodes[slot];
        nodes[slot] = nodes[slot - 1];
        nodes[slot - 1] = swap;
    }
    return OK;
}

// Fills the beam of the step after the kept run, half with dodge and half
// with hold, one batch at a time until a batch leaves a survivor.
static RESULT M_SearchStep(M_SEARCH *const search)
{
    const int32_t depth = search->depth;
    M_STEP *const step = &search->steps[depth];
    step->count = 0;
    step->kept = 0;
    for (int32_t batch = 0; batch < M_BATCHES && step->count == 0; batch++) {
        for (int32_t i = 0; i < M_BATCH_SIZE; i++) {
            FAIL_IF(
                search->evaluated >= M_BUDGET,
                "no run found in %d candidate segments", M_BUDGET);
            search->evaluated++;
            const int32_t attempt =
                (step->tries * M_BATCHES + batch) * M_BATCH_SIZE + i;
            MUST(M_Try(
                search, step, i % 2 == 0 ? &g_BotDodge : &g_BotHold,
                M_CandidateSeed(search->args->seed, depth, attempt)));
        }
    }
    step->tries++;
    return OK;
}

// Puts the kept node of the step at depth into the kept run, after the
// steps before it.
static void M_Keep(M_SEARCH *const search, const int32_t depth)
{
    M_STEP *const step = &search->steps[depth];
    search->kept.frames =
        depth > 0 ? search->steps[depth - 1].end : search->script_frames;
    M_AppendAll(&search->kept, &step->nodes[step->kept].segment);
    step->end = search->kept.frames;
    search->depth = depth + 1;
}

static void M_GrowSteps(M_SEARCH *const search)
{
    if (search->depth < search->step_capacity) {
        return;
    }
    const int32_t capacity =
        search->step_capacity == 0 ? 64 : search->step_capacity * 2;
    search->steps =
        Mem_Realloc(search->steps, (size_t)capacity * sizeof(M_STEP));
    memset(
        search->steps + search->step_capacity, 0,
        (size_t)(capacity - search->step_capacity) * sizeof(M_STEP));
    for (int32_t i = search->step_capacity; i < capacity; i++) {
        for (int32_t j = 0; j < M_BEAM; j++) {
            search->steps[i].nodes[j].segment.size = search->kept.size;
        }
    }
    search->step_capacity = capacity;
}

// Backs up after a dead end: to the next survivor of the step before, or
// when that beam is spent, by more steps each time this depth fails again.
static void M_BackUp(M_SEARCH *const search)
{
    search->dead_ends++;
    const int32_t depth = search->depth;
    if (depth > 0) {
        M_STEP *const before = &search->steps[depth - 1];
        if (before->kept + 1 < before->count) {
            before->kept++;
            M_Keep(search, depth - 1);
            printf(
                "dead end, survivor %d instead, %d frames\n", before->kept + 1,
                search->kept.frames);
            fflush(stdout);
            return;
        }
    }
    M_STEP *const step = &search->steps[depth];
    step->dead_ends++;
    const int32_t back = step->dead_ends < depth ? step->dead_ends : depth;
    search->depth -= back;
    search->kept.frames = search->depth > 0
        ? search->steps[search->depth - 1].end
        : search->script_frames;
    printf("dead end, back to %d frames\n", search->kept.frames);
    fflush(stdout);
}

// Grows the kept run, which holds the script's frames, by the search.
static RESULT M_RunSearch(M_SEARCH *const search)
{
    const RECORDER_GAME *const game = search->game;
    const int32_t until = search->args->until;
    const M_NODE first = { .score = game->score(), .ended = game->has_ended() };
    while (search->kept.frames < game->max_frames) {
        const M_NODE *node = &first;
        if (search->depth > 0) {
            const M_STEP *const last = &search->steps[search->depth - 1];
            node = &last->nodes[last->kept];
        }
        if (node->ended || (until >= 0 && node->score.values[0] >= until)) {
            break;
        }
        M_GrowSteps(search);
        MUST(M_SearchStep(search));
        const M_STEP *const step = &search->steps[search->depth];
        if (step->count == 0) {
            M_BackUp(search);
            continue;
        }
        M_Keep(search, search->depth);
        printf(
            "kept %d frames, score %lld, %d candidates\n", search->kept.frames,
            (long long)step->nodes[0].score.values[0], search->evaluated);
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
            run->frames >= game->max_frames, "the script ran past %d frames",
            game->max_frames);
        void *const input = M_Append(run);
        if (!game->script_input(run->frames - 1, input)) {
            run->frames--;
            break;
        }
        FAIL_IF(!game->tick(input), "the game quit during the run");
    }
    M_SEARCH search = {
        .game = game,
        .args = args,
        .bot = bot,
        .kept = *run,
        .candidate = { .size = run->size },
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
    for (int32_t i = 0; i < search.step_capacity; i++) {
        for (int32_t j = 0; j < M_BEAM; j++) {
            Mem_Free(search.steps[i].nodes[j].segment.inputs);
        }
    }
    Mem_Free(search.steps);
    Mem_Free(search.none);
    MUST(result);
    printf(
        "searched %d candidates, met %d dead ends\n", search.evaluated,
        search.dead_ends);
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
    BOT bot;
    MUST(Bot_Load(
        &bot, game->bot_ini, game->input_schema,
        args->lose ? &g_BotWander : &g_BotDodge, (uint16_t)args->seed));
    M_INPUTS run = { .size = game->input_schema->size };
    RESULT result = game->start();
    if (IS_OK(result)) {
        result = args->lose ? M_RunStraight(game, &bot, &run)
                            : M_Search(game, args, &bot, &run);
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
