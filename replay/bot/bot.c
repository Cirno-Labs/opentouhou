#include "bot.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/ini.h"
#include "core/io/file.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

#define M_THREAT_RADIUS 140.0f
#define M_FOCUS_DISTANCE 40.0f
#define M_BOMB_DISTANCE 10.0f
#define M_BOMB_COOLDOWN 300
#define M_DEAD_ZONE 3.0f
#define M_JITTER 6.0f
#define M_REPULSION 2.0e6f
#define M_HOME_PULL_X 0.015f
#define M_HOME_PULL_Y 0.01f

static const char *const m_RoleNames[] = {
#define M_ROLE_NAME(name) #name,
    BOT_ROLE_NAMES(M_ROLE_NAME)
#undef M_ROLE_NAME
};

static const BOT_STRATEGY *const m_Strategies[] = { &g_BotDodge, &g_BotWander,
                                                    &g_BotHold };

// Moves a hold strategy draws from: shoot, shoot+left, shoot+right, left,
// right, nothing, bomb.
static const BOT_ROLES m_HoldMoves[] = {
    { .shoot = true },
    { .shoot = true, .left = true },
    { .shoot = true, .right = true },
    { .left = true },
    { .right = true },
    { 0 },
    { .bomb = true },
};
static const int32_t m_HoldDurations[] = { 4, 10, 20, 40 };

static float M_Length(const VEC2 offset)
{
    return sqrtf(offset.x * offset.x + offset.y * offset.y);
}

static bool M_IsEdge(const VEC2 offset)
{
    return offset.x == 0.0f || offset.y == 0.0f;
}

static void M_Wander(
    BOT *const bot, const VEC2 *const threats, const int32_t count,
    BOT_ROLES *const out)
{
    if (bot->wander_frames <= 0) {
        bot->wander_frames = 15 + Rng_U16InRange(&bot->rng, 30);
        const uint16_t pick = Rng_U16InRange(&bot->rng, 9);
        bot->wander_up = pick == 0 || pick == 4 || pick == 5;
        bot->wander_down = pick == 1 || pick == 6 || pick == 7;
        bot->wander_left = pick == 2 || pick == 4 || pick == 6;
        bot->wander_right = pick == 3 || pick == 5 || pick == 7;
    }
    bot->wander_frames--;
    *out = (BOT_ROLES) {
        .shoot = true,
        .up = bot->wander_up,
        .down = bot->wander_down,
        .left = bot->wander_left,
        .right = bot->wander_right,
    };
}

static float M_NearestThreat(const VEC2 *const threats, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        if (!M_IsEdge(threats[i])) {
            return M_Length(threats[i]);
        }
    }
    return INFINITY;
}

// Sums the repulsion of every threat and edge within M_THREAT_RADIUS and
// the pull toward the home spot: the left and right edges sum to twice the
// offset to the center, and the bottom edge points straight down.
static VEC2 M_Field(const VEC2 *const threats, const int32_t count)
{
    VEC2 field = { 0.0f, 0.0f };
    VEC2 pull = { 0.0f, 0.0f };
    for (int32_t i = 0; i < count; i++) {
        const VEC2 offset = threats[i];
        const float distance = M_Length(offset);
        if (distance >= 0.01f && distance < M_THREAT_RADIUS) {
            const float scale = -M_REPULSION / (distance * distance * distance);
            field.x += offset.x * scale;
            field.y += offset.y * scale;
        }
        if (offset.y == 0.0f) {
            pull.x += offset.x;
        } else if (offset.x == 0.0f && offset.y > 0.0f) {
            pull.y += offset.y;
        }
    }
    field.x += pull.x * M_HOME_PULL_X;
    field.y += pull.y * M_HOME_PULL_Y;
    return field;
}

static void M_Dodge(
    BOT *const bot, const VEC2 *const threats, const int32_t count,
    BOT_ROLES *const out)
{
    if (bot->bomb_cooldown > 0) {
        bot->bomb_cooldown--;
    }
    VEC2 field = M_Field(threats, count);
    field.x += Rng_F32InRange(&bot->rng, 2.0f * M_JITTER) - M_JITTER;
    field.y += Rng_F32InRange(&bot->rng, 2.0f * M_JITTER) - M_JITTER;
    const float nearest = M_NearestThreat(threats, count);
    *out = (BOT_ROLES) {
        .shoot = true,
        .focus = nearest < M_FOCUS_DISTANCE,
        .up = field.y<-M_DEAD_ZONE, .down = field.y> M_DEAD_ZONE,
        .left = field.x<-M_DEAD_ZONE, .right = field.x> M_DEAD_ZONE,
    };
    if (nearest < M_BOMB_DISTANCE && bot->bomb_cooldown <= 0) {
        out->bomb = true;
        bot->bomb_cooldown = M_BOMB_COOLDOWN;
    }
}

static void M_Hold(
    BOT *const bot, const VEC2 *const threats, const int32_t count,
    BOT_ROLES *const out)
{
    (void)threats;
    (void)count;
    if (bot->hold_frames <= 0) {
        const uint16_t move =
            Rng_U16InRange(&bot->rng, ARRAY_SIZE(m_HoldMoves));
        const uint16_t duration =
            Rng_U16InRange(&bot->rng, ARRAY_SIZE(m_HoldDurations));
        bot->hold_roles = m_HoldMoves[move];
        bot->hold_frames = m_HoldDurations[duration];
    }
    bot->hold_frames--;
    *out = bot->hold_roles;
}

static RESULT M_ParseButtons(
    const INPUT_SCHEMA *const schema, const char *const role, const char *word,
    const size_t length, uint32_t *const out)
{
    *out = 0;
    if (length == 1 && word[0] == '.') {
        return OK;
    }
    const char *const end = word + length;
    while (true) {
        const char *plus = memchr(word, '+', (size_t)(end - word));
        if (plus == nullptr) {
            plus = end;
        }
        const size_t size = (size_t)(plus - word);
        size_t found = schema->count;
        for (size_t i = 0; i < schema->count; i++) {
            const char *const name = schema->actions[i].name;
            if (schema->actions[i].type == INPUT_ACTION_BUTTON
                && strlen(name) == size && memcmp(name, word, size) == 0) {
                found = i;
                break;
            }
        }
        FAIL_IF(
            found == schema->count, "role %s names no button %.*s", role,
            (int)size, word);
        *out |= 1u << found;
        if (plus == end) {
            return OK;
        }
        word = plus + 1;
    }
}

static RESULT M_ParseRole(
    const INPUT_SCHEMA *const schema, const char *const role,
    const char *const value, BOT_SEQUENCE *const out)
{
    memset(out, 0, sizeof(*out));
    const char *cursor = value;
    while (true) {
        while (isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        const char *end = cursor;
        while (*end != '\0' && !isspace((unsigned char)*end)) {
            end++;
        }
        FAIL_IF(
            out->count == BOT_MAX_FRAMES, "role %s has more than %d frames",
            role, BOT_MAX_FRAMES);
        MUST(M_ParseButtons(
            schema, role, cursor, (size_t)(end - cursor),
            &out->frames[out->count]));
        out->count++;
        cursor = end;
    }
    FAIL_IF(out->count == 0, "role %s has no frames", role);
    return OK;
}

static RESULT M_LoadIni(
    BOT *const bot, const INI *const ini, const INPUT_SCHEMA *const schema,
    const BOT_STRATEGY *const strategy, const uint16_t seed)
{
    memset(bot, 0, sizeof(*bot));
    bot->schema = schema;
    bot->strategy = strategy;
    Rng_SetSeed(&bot->rng, seed);
    const char *key;
    for (size_t i = 0; (key = Ini_GetKey(ini, "roles", i)) != nullptr; i++) {
        size_t role = BOT_ROLE_COUNT;
        for (size_t j = 0; j < BOT_ROLE_COUNT; j++) {
            if (Str_EqualIgnoreCase(m_RoleNames[j], key)) {
                role = j;
            }
        }
        FAIL_IF(role == BOT_ROLE_COUNT, "there is no role %s", key);
        MUST(M_ParseRole(
            schema, m_RoleNames[role], Ini_Get(ini, "roles", key),
            &bot->roles[role]));
    }
    return OK;
}

const BOT_STRATEGY g_BotDodge = { .name = "dodge", .next = M_Dodge };
const BOT_STRATEGY g_BotWander = { .name = "wander", .next = M_Wander };
const BOT_STRATEGY g_BotHold = { .name = "hold", .next = M_Hold };

const BOT_STRATEGY *Bot_FindStrategy(const char *const name)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_Strategies); i++) {
        if (strcmp(m_Strategies[i]->name, name) == 0) {
            return m_Strategies[i];
        }
    }
    return nullptr;
}

RESULT Bot_LoadText(
    BOT *const bot, const char *const text, const size_t size,
    const INPUT_SCHEMA *const schema, const BOT_STRATEGY *const strategy,
    const uint16_t seed)
{
    INI *ini;
    MUST(Ini_Parse(text, size, &g_HeapAllocator, &ini));
    const RESULT result = M_LoadIni(bot, ini, schema, strategy, seed);
    Ini_Free(ini);
    return result;
}

RESULT Bot_Load(
    BOT *const bot, const char *const path, const INPUT_SCHEMA *const schema,
    const BOT_STRATEGY *const strategy, const uint16_t seed)
{
    FAIL_IF(!File_Exists(path), "%s does not exist", path);
    INI *ini;
    MUST(Ini_Load(path, &g_HeapAllocator, &ini));
    const RESULT result = M_LoadIni(bot, ini, schema, strategy, seed);
    Ini_Free(ini);
    MUST(result, "%s", path);
    return OK;
}

static void M_Write(const BOT *const bot, const uint32_t buttons, void *out)
{
    uint8_t *const input = out;
    memset(input, 0, bot->schema->size);
    for (size_t i = 0; i < bot->schema->count; i++) {
        if ((buttons & 1u << i) != 0) {
            *(bool *)(input + bot->schema->actions[i].offset) = true;
        }
    }
}

void Bot_NextInput(
    BOT *const bot, const VEC2 *const threats, const int32_t count,
    void *const out)
{
    if (bot->running != nullptr) {
        M_Write(bot, bot->running->frames[bot->running_frame], out);
        bot->running_frame++;
        if (bot->running_frame == bot->running->count) {
            bot->running = nullptr;
        }
        return;
    }
    BOT_ROLES roles;
    bot->strategy->next(bot, threats, count, &roles);
    const bool picked[BOT_ROLE_COUNT] = {
#define M_ROLE_PICKED(name) roles.name,
        BOT_ROLE_NAMES(M_ROLE_PICKED)
#undef M_ROLE_PICKED
    };
    uint32_t buttons = 0;
    for (size_t i = 0; i < BOT_ROLE_COUNT; i++) {
        const BOT_SEQUENCE *const sequence = &bot->roles[i];
        if (!picked[i] || sequence->count == 0) {
            continue;
        }
        if (sequence->count > 1) {
            bot->running = sequence;
            bot->running_frame = 1;
            M_Write(bot, sequence->frames[0], out);
            return;
        }
        buttons |= sequence->frames[0];
    }
    M_Write(bot, buttons, out);
}
