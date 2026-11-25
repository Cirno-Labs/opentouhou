#include "recorder.h"

#include "bot.h"

#include "core/args.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/utils.h"
#include "shell/null.h"

#include <stdio.h>
#include <string.h>

#define M_SEGMENT_FRAMES 150
#define M_LONGEST_SEGMENT 300
#define M_LOSING_SEGMENT 3000
#define M_IDLE_FRAMES 90
#define M_BATCHES 3
#define M_BATCH_SIZE 16
#define M_ELITE 4
#define M_PLATEAU 8
#define M_BUDGET 20000

typedef struct {
    const char *data;
    const char *out;
    ARG_ITEMS starts;
    int32_t seed;
    int32_t until;
    bool minimize;
} M_ARGS;

// Holds the input structs of a run, one per frame.
typedef struct {
    uint8_t *inputs;
    int32_t frames;
    int32_t capacity;
    size_t size;
} M_INPUTS;

// Holds a role combination, a mask of BOT_ROLE bits, for some frames.
typedef struct {
    uint32_t roles;
    int32_t frames;
} M_HOLD;

typedef struct {
    M_HOLD *holds;
    int32_t count;
    int32_t capacity;
} M_HOLDS;

// Holds a run prefix that survived: the holds that it adds after its
// parent's, the frame at which it ends and how far it got.
typedef struct {
    int32_t parent;
    int32_t first;
    int32_t count;
    int32_t frames;
    int32_t depth;
    int32_t demotions;
    RECORDER_SCORE score;
    bool ended;
} M_NODE;

typedef struct {
    const RECORDER_GAME *game;
    const M_ARGS *args;
    const BOT *bot;
    M_INPUTS script;
    M_HOLDS holds;
    M_HOLDS candidate;
    M_NODE *nodes;
    int32_t node_count;
    int32_t node_capacity;
    int32_t *frontier;
    int32_t frontier_count;
    int32_t frontier_capacity;
    int32_t *path;
    int32_t path_capacity;
    int32_t elite[M_ELITE];
    int32_t elite_count;
    bool improved;
    int32_t stalled;
    int32_t demotions;
    int32_t goal;
    int32_t evaluated;
    int32_t dead_ends;
    uint64_t rng;
    void *input;
    void *none;
} M_SEARCH;

static const int32_t m_HoldFrames[] = { 4, 10, 20, 40 };

static const ARG_OPTION m_Options[] = {
    ARG_STRING_OF(
        M_ARGS, "--data", data, "<dir>", "the game's directory", ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "--out", out, "<file>", "the file to write", ARG_REQUIRED),
    ARG_LIST_OF(
        M_ARGS, "--start", starts, "<name=value>",
        "a start value of the run, such as stage=1; repeatable"),
    ARG_INT_OF(
        M_ARGS, "--seed", seed, "<n>", "the seed of the search",
        ARG_FALLBACK_INT(1)),
    ARG_INT_OF(
        M_ARGS, "--until", until, "<n>",
        "end the run once its score's first value reaches n",
        ARG_FALLBACK_INT(-1)),
    ARG_FLAG_OF(
        M_ARGS, "--minimize", minimize,
        "keep only segments that add setbacks, for a game over"),
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

// Makes room for one more item in an array of count items of size bytes.
static void *M_Grow(
    void *const items, int32_t *const capacity, const int32_t count,
    const size_t size)
{
    if (count < *capacity) {
        return items;
    }
    *capacity = *capacity == 0 ? 64 : *capacity * 2;
    return Mem_ReallocArr(items, (size_t)*capacity, size);
}

static void *M_Append(M_INPUTS *const run)
{
    run->inputs = M_Grow(run->inputs, &run->capacity, run->frames, run->size);
    void *const input = run->inputs + (size_t)run->frames * run->size;
    run->frames++;
    return input;
}

static void M_AddHold(M_HOLDS *const holds, const M_HOLD hold)
{
    holds->holds =
        M_Grow(holds->holds, &holds->capacity, holds->count, sizeof(M_HOLD));
    holds->holds[holds->count++] = hold;
}

// Returns a number below n from the splitmix64 sequence of state.
static uint32_t M_Random(uint64_t *const state, const uint32_t n)
{
    *state += 0x9e3779b97f4a7c15u;
    uint64_t z = *state;
    z = (z ^ z >> 30) * 0xbf58476d1ce4e5b9u;
    z = (z ^ z >> 27) * 0x94d049bb133111ebu;
    return (uint32_t)((z ^ z >> 31) % n);
}

// Draws each role that presses a button with odds of one in two, or one in
// eight for a role of several frames.
static uint32_t M_RandomRoles(const BOT *const bot, uint64_t *const rng)
{
    const uint32_t mapped = Bot_GetRoles(bot);
    uint32_t roles = 0;
    for (int32_t i = 0; i < BOT_ROLE_COUNT; i++) {
        const uint32_t odds = bot->roles[i].count > 1 ? 8 : 2;
        if ((mapped & 1u << i) != 0 && M_Random(rng, odds) == 0) {
            roles |= 1u << i;
        }
    }
    return roles;
}

// Keeps a hold within one segment and no shorter than its leading role.
static M_HOLD M_FitHold(const BOT *const bot, M_HOLD hold)
{
    hold.frames = MIN(hold.frames, M_SEGMENT_FRAMES);
    hold.frames = MAX(hold.frames, MAX(1, Bot_GetLeadFrames(bot, hold.roles)));
    return hold;
}

static M_HOLD M_RandomHold(const BOT *const bot, uint64_t *const rng)
{
    const M_HOLD hold = {
        .roles = M_RandomRoles(bot, rng),
        .frames = m_HoldFrames[M_Random(rng, ARRAY_SIZE(m_HoldFrames))],
    };
    return M_FitHold(bot, hold);
}

// Restarts the game with its files opened anew, since a game may mount
// files at each start that only closing them unmounts, and plays the
// script.
static RESULT M_Restart(
    const RECORDER_GAME *const game, const char *const data,
    const M_INPUTS *const script)
{
    game->stop();
    MUST(NullShell_OpenFiles(data, nullptr));
    Recorder_ResetClock();
    MUST(game->start());
    for (int32_t i = 0; i < script->frames; i++) {
        FAIL_IF(
            !game->tick(script->inputs + (size_t)i * script->size),
            "the game quit during the run");
    }
    return OK;
}

// Lists the nodes from the root's first child to node in search->path and
// returns their number.
static int32_t M_Path(M_SEARCH *const search, const int32_t node)
{
    const int32_t depth = search->nodes[node].depth;
    if (depth > search->path_capacity) {
        search->path_capacity = depth * 2;
        search->path = Mem_ReallocArr(
            search->path, (size_t)search->path_capacity, sizeof(int32_t));
    }
    int32_t at = node;
    for (int32_t i = depth - 1; i >= 0; i--) {
        search->path[i] = at;
        at = search->nodes[at].parent;
    }
    return depth;
}

// Plays the run of node from a fresh start.
static RESULT M_Replay(M_SEARCH *const search, const int32_t node)
{
    const RECORDER_GAME *const game = search->game;
    MUST(M_Restart(game, search->args->data, &search->script));
    const int32_t depth = M_Path(search, node);
    for (int32_t i = 0; i < depth; i++) {
        const M_NODE *const at = &search->nodes[search->path[i]];
        for (int32_t j = 0; j < at->count; j++) {
            const M_HOLD hold = search->holds.holds[at->first + j];
            for (int32_t frame = 0; frame < hold.frames; frame++) {
                Bot_WriteInput(search->bot, hold.roles, frame, search->input);
                FAIL_IF(
                    !game->tick(search->input), "the game quit during the run");
            }
        }
    }
    return OK;
}

static bool M_Better(const M_NODE *const a, const M_NODE *const b)
{
    const int order = Recorder_CompareScores(a->score, b->score);
    return order != 0 ? order > 0 : a->frames > b->frames;
}

// Orders the frontier: fewer demotions first, then the better score, then
// the longer run, then the earlier node.
static bool M_Ahead(
    const M_SEARCH *const search, const int32_t a, const int32_t b)
{
    const M_NODE *const first = &search->nodes[a];
    const M_NODE *const second = &search->nodes[b];
    if (first->demotions != second->demotions) {
        return first->demotions < second->demotions;
    }
    if (M_Better(first, second) || M_Better(second, first)) {
        return M_Better(first, second);
    }
    return a < b;
}

// Returns the frontier slot of the node to expand next.
static int32_t M_Top(const M_SEARCH *const search)
{
    int32_t top = 0;
    for (int32_t i = 1; i < search->frontier_count; i++) {
        if (M_Ahead(search, search->frontier[i], search->frontier[top])) {
            top = i;
        }
    }
    return top;
}

static void M_AddElite(M_SEARCH *const search, const int32_t node)
{
    int32_t slot = search->elite_count;
    if (slot == M_ELITE) {
        slot = M_ELITE - 1;
        if (!M_Better(
                &search->nodes[node], &search->nodes[search->elite[slot]])) {
            return;
        }
    } else {
        search->elite_count++;
    }
    for (; slot > 0
         && M_Better(
               &search->nodes[node], &search->nodes[search->elite[slot - 1]]);
         slot--) {
        search->elite[slot] = search->elite[slot - 1];
    }
    search->elite[slot] = node;
}

static bool M_IsGoal(const M_SEARCH *const search, const M_NODE *const node)
{
    const int32_t until = search->args->until;
    return node->ended || node->frames >= search->game->max_frames
        || (until >= 0 && node->score.values[0] >= until);
}

// Adds the first frames of the candidate as a child of parent, and clears
// every demotion when it is the best node so far.
static void M_AddNode(
    M_SEARCH *const search, const int32_t parent, const int32_t frames,
    const RECORDER_SCORE score, const bool ended)
{
    search->nodes = M_Grow(
        search->nodes, &search->node_capacity, search->node_count,
        sizeof(M_NODE));
    const int32_t index = search->node_count++;
    const M_NODE *const from = &search->nodes[parent];
    M_NODE *const node = &search->nodes[index];
    *node = (M_NODE) {
        .parent = parent,
        .first = search->holds.count,
        .frames = from->frames + frames,
        .depth = from->depth + 1,
        .demotions = from->demotions,
        .score = score,
        .ended = ended,
    };
    for (int32_t i = 0, left = frames; left > 0; i++) {
        M_HOLD hold = search->candidate.holds[i];
        hold.frames = MIN(hold.frames, left);
        left -= hold.frames;
        M_AddHold(&search->holds, hold);
        node->count++;
    }
    search->frontier = M_Grow(
        search->frontier, &search->frontier_capacity, search->frontier_count,
        sizeof(int32_t));
    search->frontier[search->frontier_count++] = index;
    M_AddElite(search, index);
    if (search->elite[0] == index) {
        search->improved = true;
        for (int32_t i = 0; i < search->node_count; i++) {
            search->nodes[i].demotions = 0;
        }
    }
    if (search->goal < 0 && M_IsGoal(search, node)) {
        search->goal = index;
    }
}

// Plays the candidate after the run of parent from a fresh start and adds
// it as a node when it survives. Without --minimize, the idle frames follow
// and a rise of the setbacks drops it. With --minimize, it stops at the
// first rise of the setbacks, and only a rise or the game's end keeps it.
static RESULT M_Try(
    M_SEARCH *const search, const int32_t parent, bool *const survived)
{
    *survived = false;
    const RECORDER_GAME *const game = search->game;
    const bool minimize = search->args->minimize;
    MUST(M_Replay(search, parent));
    const int32_t setbacks = game->setbacks();
    const int32_t limit =
        MIN(minimize ? M_LOSING_SEGMENT : M_LONGEST_SEGMENT,
            game->max_frames - search->nodes[parent].frames);
    int32_t frames = 0;
    bool ended = false;
    bool lost = false;
    for (int32_t i = 0; i < search->candidate.count && !ended && !lost; i++) {
        const M_HOLD hold = search->candidate.holds[i];
        for (int32_t frame = 0;
             frame < hold.frames && frames < limit && !ended && !lost;
             frame++) {
            Bot_WriteInput(search->bot, hold.roles, frame, search->input);
            FAIL_IF(!game->tick(search->input), "the game quit during the run");
            frames++;
            ended = game->has_ended();
            lost = game->setbacks() > setbacks;
        }
    }
    const RECORDER_SCORE score = game->score();
    if (minimize ? !lost && !ended : lost || frames == 0) {
        return OK;
    }
    for (int32_t i = 0; !minimize && i < M_IDLE_FRAMES && !game->has_ended();
         i++) {
        if (!game->tick(search->none)) {
            break;
        }
    }
    if (!minimize && game->setbacks() > setbacks) {
        return OK;
    }
    *survived = true;
    M_AddNode(search, parent, frames, score, ended);
    return OK;
}

// Fills the candidate with random holds of one segment, or of the longest
// segment with --minimize.
static void M_Explore(M_SEARCH *const search)
{
    M_HOLDS *const candidate = &search->candidate;
    const int32_t length =
        search->args->minimize ? M_LOSING_SEGMENT : M_SEGMENT_FRAMES;
    candidate->count = 0;
    for (int32_t frames = 0; frames < length;) {
        M_HOLD hold = M_RandomHold(search->bot, &search->rng);
        hold.frames = MIN(hold.frames, length - frames);
        frames += hold.frames;
        M_AddHold(candidate, hold);
    }
}
// Fills the candidate with the holds of one of the best nodes, with one to
// three of them changed, lengthened, shortened, swapped with the next or
// replaced.
static void M_Refine(M_SEARCH *const search)
{
    M_HOLDS *const candidate = &search->candidate;
    uint64_t *const rng = &search->rng;
    const M_NODE *const source = &search->nodes[search->elite[M_Random(
        rng, (uint32_t)search->elite_count)]];
    candidate->count = 0;
    for (int32_t i = 0; i < source->count; i++) {
        M_AddHold(candidate, search->holds.holds[source->first + i]);
    }
    const int32_t changes = 1 + (int32_t)M_Random(rng, 3);
    for (int32_t i = 0; i < changes; i++) {
        M_HOLD *const holds = candidate->holds;
        const int32_t at = (int32_t)M_Random(rng, (uint32_t)candidate->count);
        switch (M_Random(rng, 5)) {
        case 0:
            holds[at].roles = M_RandomRoles(search->bot, rng);
            break;
        case 1:
            holds[at].frames *= 2;
            break;
        case 2:
            holds[at].frames /= 2;
            break;
        case 3:
            if (at + 1 < candidate->count) {
                const M_HOLD swap = holds[at];
                holds[at] = holds[at + 1];
                holds[at + 1] = swap;
            }
            break;
        default:
            holds[at] = M_RandomHold(search->bot, rng);
            break;
        }
        holds[at] = M_FitHold(search->bot, holds[at]);
    }
}

// Moves the nodes under the ancestor search->demotions steps above the next
// node to expand, but below the root, behind all others.
static void M_Demote(M_SEARCH *const search)
{
    const M_NODE *const nodes = search->nodes;
    int32_t ancestor = search->frontier[M_Top(search)];
    for (int32_t i = 0; i < search->demotions && nodes[ancestor].depth > 1;
         i++) {
        ancestor = nodes[ancestor].parent;
    }
    if (nodes[ancestor].depth == 0) {
        return;
    }
    const int32_t depth = nodes[ancestor].depth;
    for (int32_t i = 0; i < search->frontier_count; i++) {
        int32_t at = search->frontier[i];
        while (nodes[at].depth > depth) {
            at = nodes[at].parent;
        }
        if (at == ancestor) {
            search->nodes[search->frontier[i]].demotions++;
        }
    }
    printf(
        "plateau, demoted %d nodes back, now at %d frames\n", search->demotions,
        nodes[search->frontier[M_Top(search)]].frames);
    fflush(stdout);
}

// Expands the frontier's next node with batches of candidates, half random
// and half refined, until a batch leaves a survivor.
static RESULT M_Expand(M_SEARCH *const search)
{
    const int32_t slot = M_Top(search);
    const int32_t node = search->frontier[slot];
    search->frontier[slot] = search->frontier[--search->frontier_count];
    search->improved = false;
    bool any = false;
    for (int32_t batch = 0; batch < M_BATCHES && !any; batch++) {
        for (int32_t i = 0; i < M_BATCH_SIZE && search->goal < 0; i++) {
            FAIL_IF(
                search->evaluated >= M_BUDGET,
                "no run found in %d candidate segments", M_BUDGET);
            search->evaluated++;
            if (i % 2 == 0 || search->elite_count == 0) {
                M_Explore(search);
            } else {
                M_Refine(search);
            }
            bool survived;
            MUST(M_Try(search, node, &survived));
            any = any || survived;
        }
    }
    if (!any) {
        search->dead_ends++;
    }
    if (search->improved) {
        search->stalled = 0;
        search->demotions = 0;
        const M_NODE *const best = &search->nodes[search->elite[0]];
        printf(
            "best %d frames, score %lld, %d candidates\n", best->frames,
            (long long)best->score.values[0], search->evaluated);
        fflush(stdout);
    } else if (++search->stalled == M_PLATEAU && search->frontier_count > 0) {
        search->stalled = 0;
        search->demotions++;
        M_Demote(search);
    }
    return OK;
}

// Searches from the root, the end of the script, until a node reaches the
// goal.
static RESULT M_RunSearch(M_SEARCH *const search)
{
    const RECORDER_GAME *const game = search->game;
    search->nodes = M_Grow(nullptr, &search->node_capacity, 0, sizeof(M_NODE));
    search->nodes[0] = (M_NODE) {
        .parent = -1,
        .frames = search->script.frames,
        .score = game->score(),
        .ended = game->has_ended(),
    };
    search->node_count = 1;
    search->goal = M_IsGoal(search, &search->nodes[0]) ? 0 : -1;
    search->frontier =
        M_Grow(nullptr, &search->frontier_capacity, 0, sizeof(int32_t));
    search->frontier[search->frontier_count++] = 0;
    while (search->goal < 0) {
        FAIL_IF(
            search->frontier_count == 0,
            "every run met a dead end after %d "
            "candidate segments",
            search->evaluated);
        MUST(M_Expand(search));
    }
    return OK;
}

// Writes the inputs of the run of node into run, after the script's.
static void M_WriteRun(
    M_SEARCH *const search, const int32_t node, M_INPUTS *const run)
{
    const int32_t depth = M_Path(search, node);
    for (int32_t i = 0; i < depth; i++) {
        const M_NODE *const at = &search->nodes[search->path[i]];
        for (int32_t j = 0; j < at->count; j++) {
            const M_HOLD hold = search->holds.holds[at->first + j];
            for (int32_t frame = 0; frame < hold.frames; frame++) {
                Bot_WriteInput(search->bot, hold.roles, frame, M_Append(run));
            }
        }
    }
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
        .script = *run,
        .rng = (uint64_t)args->seed,
        .input = Mem_AllocZ(run->size),
        .none = Mem_AllocZ(run->size),
    };
    // Keeps the replays from repeating the first run's messages.
    const LOG_LEVEL level = Log_GetMinLevel();
    Log_SetMinLevel(LOG_LEVEL_ERROR);
    RESULT result = M_RunSearch(&search);
    Log_SetMinLevel(level);
    if (IS_OK(result)) {
        M_WriteRun(&search, search.goal, run);
        printf(
            "searched %d candidates, met %d dead ends\n", search.evaluated,
            search.dead_ends);
        result = M_Restart(game, args->data, run);
    }
    Mem_Free(search.holds.holds);
    Mem_Free(search.candidate.holds);
    Mem_Free(search.nodes);
    Mem_Free(search.frontier);
    Mem_Free(search.path);
    Mem_Free(search.input);
    Mem_Free(search.none);
    return result;
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
    MUST(Bot_Load(&bot, game->bot_ini, game->input_schema));
    M_INPUTS run = { .size = game->input_schema->size };
    RESULT result = game->start();
    if (IS_OK(result)) {
        result = M_Search(game, args, &bot, &run);
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
