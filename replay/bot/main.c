#include "recorder.h"

#include "bot.h"

#include "core/args.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/utils.h"
#include "shell/null.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define M_SEGMENT_FRAMES 150
#define M_LONGEST_SEGMENT 300
#define M_LOSING_SEGMENT 3000
#define M_IDLE_FRAMES 90
#define M_BRANCHES 16
#define M_EXPLORATION 0.02
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
    int32_t child;
    int32_t sibling;
    int32_t first;
    int32_t count;
    int32_t frames;
    int32_t depth;
    int32_t tries;
    int32_t visits;
    int32_t improved;
    int32_t top;
    RECORDER_SCORE score;
    bool ended;
    bool exhausted;
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
    int32_t *path;
    int32_t path_capacity;
    int32_t best;
    int32_t tip;
    int32_t goal;
    int32_t evaluated;
    int32_t dead;
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

static bool M_IsGoal(const M_SEARCH *const search, const M_NODE *const node)
{
    const int32_t until = search->args->until;
    return node->ended || node->frames >= search->game->max_frames
        || (until >= 0 && node->score.values[0] >= until);
}

// Adds the first frames of the candidate as the last child of parent and
// returns its index.
static int32_t M_AddNode(
    M_SEARCH *const search, const int32_t parent, const int32_t frames,
    const RECORDER_SCORE score, const bool ended)
{
    search->nodes = M_Grow(
        search->nodes, &search->node_capacity, search->node_count,
        sizeof(M_NODE));
    const int32_t index = search->node_count++;
    M_NODE *const nodes = search->nodes;
    M_NODE *const node = &nodes[index];
    *node = (M_NODE) {
        .parent = parent,
        .child = -1,
        .sibling = -1,
        .top = index,
        .first = search->holds.count,
        .frames = nodes[parent].frames + frames,
        .depth = nodes[parent].depth + 1,
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
    int32_t *link = &nodes[parent].child;
    while (*link >= 0) {
        link = &nodes[*link].sibling;
    }
    *link = index;
    if (M_Better(node, &nodes[search->best])) {
        search->best = index;
        printf(
            "best %d frames, score %lld, %d candidates\n", node->frames,
            (long long)node->score.values[0], search->evaluated);
        fflush(stdout);
    }
    if (search->goal < 0 && M_IsGoal(search, node)) {
        search->goal = index;
    }
    return index;
}

// Plays the candidate after the run of parent from a fresh start and adds
// it as a child when it survives, or gives -1. Without --minimize, the idle
// frames follow and a rise of the setbacks drops it. With --minimize, it
// stops at the first rise of the setbacks, and only a rise or the game's
// end keeps it.
static RESULT M_Try(
    M_SEARCH *const search, const int32_t parent, int32_t *const child)
{
    *child = -1;
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
    *child = M_AddNode(search, parent, frames, score, ended);
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

// Fills the candidate with the holds of source, with one to three of them
// changed, lengthened, shortened, swapped with the next or replaced.
static void M_Refine(M_SEARCH *const search, const int32_t source)
{
    M_HOLDS *const candidate = &search->candidate;
    uint64_t *const rng = &search->rng;
    const M_NODE *const from = &search->nodes[source];
    candidate->count = 0;
    for (int32_t i = 0; i < from->count; i++) {
        M_AddHold(candidate, search->holds.holds[from->first + i]);
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

// Returns the child of node with the best score, or -1 when it has none.
static int32_t M_BestChild(const M_SEARCH *const search, const int32_t node)
{
    const M_NODE *const nodes = search->nodes;
    int32_t best = -1;
    for (int32_t at = nodes[node].child; at >= 0; at = nodes[at].sibling) {
        if (best < 0 || M_Better(&nodes[at], &nodes[best])) {
            best = at;
        }
    }
    return best;
}

// Adds a visit to node and each node above it, and makes child, when not
// -1, the best node of each that it beats.
static void M_Backpropagate(
    M_SEARCH *const search, const int32_t node, const int32_t child)
{
    M_NODE *const nodes = search->nodes;
    for (int32_t at = node; at >= 0; at = nodes[at].parent) {
        nodes[at].visits++;
        if (at == child
            || (child >= 0 && M_Better(&nodes[child], &nodes[nodes[at].top]))) {
            nodes[at].top = child;
            nodes[at].improved = nodes[at].visits;
        }
    }
}

// Marks node exhausted once it has all its children and none of them can
// grow, then does the same for each node above it.
static void M_Settle(M_SEARCH *const search, const int32_t node)
{
    M_NODE *const nodes = search->nodes;
    for (int32_t at = node; at >= 0; at = nodes[at].parent) {
        if (nodes[at].tries < M_BRANCHES) {
            return;
        }
        for (int32_t child = nodes[at].child; child >= 0;
             child = nodes[child].sibling) {
            if (!nodes[child].exhausted) {
                return;
            }
        }
        nodes[at].exhausted = true;
    }
}

// Returns the share of the other open siblings of node whose best node
// does not beat node's, times the share of node's visits that came before
// its best node last changed.
static double M_Value(const M_SEARCH *const search, const int32_t node)
{
    const M_NODE *const nodes = search->nodes;
    const M_NODE *const top = &nodes[nodes[node].top];
    int32_t others = 0;
    int32_t beaten = 0;
    for (int32_t at = nodes[nodes[node].parent].child; at >= 0;
         at = nodes[at].sibling) {
        if (at != node && !nodes[at].exhausted) {
            others++;
            beaten += !M_Better(&nodes[nodes[at].top], top);
        }
    }
    const double rank = others == 0 ? 1.0 : (double)beaten / others;
    return rank * nodes[node].improved / nodes[node].visits;
}

// Descends from the root by UCB1 to the first node with fewer than
// M_BRANCHES children.
static int32_t M_Select(const M_SEARCH *const search)
{
    const M_NODE *const nodes = search->nodes;
    int32_t at = 0;
    while (nodes[at].tries >= M_BRANCHES) {
        const double log_visits = log(nodes[at].visits);
        int32_t pick = -1;
        double most = 0.0;
        for (int32_t child = nodes[at].child; child >= 0;
             child = nodes[child].sibling) {
            const M_NODE *const node = &nodes[child];
            const double bound = M_Value(search, child)
                + M_EXPLORATION * sqrt(log_visits / node->visits);
            if (!node->exhausted && (pick < 0 || bound > most)) {
                pick = child;
                most = bound;
            }
        }
        at = pick;
    }
    return at;
}

// Plays one new segment at the node that M_Select gives, at random or
// refined from the node's best child, and backpropagates the visit.
static RESULT M_Expand(M_SEARCH *const search)
{
    const int32_t node = M_Select(search);
    const int32_t source = M_BestChild(search, node);
    if (search->nodes[node].tries % 2 == 0 || source < 0) {
        M_Explore(search);
    } else {
        M_Refine(search, source);
    }
    search->nodes[node].tries++;
    search->evaluated++;
    int32_t child;
    MUST(M_Try(search, node, &child));
    if (child < 0) {
        search->dead++;
        M_Backpropagate(search, node, -1);
        M_Settle(search, node);
    } else {
        M_Backpropagate(search, child, child);
    }
    return OK;
}

// Plays one random segment at the tip and moves the tip to it when it
// survives.
static RESULT M_Descend(M_SEARCH *const search)
{
    M_Explore(search);
    search->evaluated++;
    int32_t child;
    MUST(M_Try(search, search->tip, &child));
    if (child < 0) {
        search->dead++;
    } else {
        search->tip = child;
    }
    return OK;
}

// Searches from the root, the end of the script, and returns the node
// whose run to save.
static RESULT M_RunSearch(M_SEARCH *const search, int32_t *const result)
{
    const RECORDER_GAME *const game = search->game;
    search->nodes = M_Grow(nullptr, &search->node_capacity, 0, sizeof(M_NODE));
    search->nodes[0] = (M_NODE) {
        .parent = -1,
        .child = -1,
        .sibling = -1,
        .frames = search->script.frames,
        .score = game->score(),
        .ended = game->has_ended(),
    };
    search->node_count = 1;
    search->goal = M_IsGoal(search, &search->nodes[0]) ? 0 : -1;
    const bool minimize = search->args->minimize;
    while (search->goal < 0 && !search->nodes[0].exhausted
           && search->evaluated < M_BUDGET) {
        MUST(minimize ? M_Descend(search) : M_Expand(search));
    }
    if (search->goal >= 0) {
        const M_NODE *const goal = &search->nodes[search->goal];
        *result = search->goal;
        if (goal->ended) {
            printf("the game ended\n");
        } else if (goal->frames >= game->max_frames) {
            printf("the run reached the frame cap\n");
        } else {
            printf("the score reached --until\n");
        }
    } else if (search->nodes[0].exhausted) {
        *result = search->best;
        printf("every segment met a dead end\n");
    } else {
        *result = minimize ? search->tip : search->best;
        printf("the budget of %d candidates ran out\n", M_BUDGET);
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
    int32_t node;
    RESULT result = M_RunSearch(&search, &node);
    Log_SetMinLevel(level);
    if (IS_OK(result)) {
        M_WriteRun(&search, node, run);
        printf(
            "searched %d candidates, %d of them dead\n", search.evaluated,
            search.dead);
        result = M_Restart(game, args->data, run);
    }
    Mem_Free(search.holds.holds);
    Mem_Free(search.candidate.holds);
    Mem_Free(search.nodes);
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
