#include "bot.h"

#include "core/base/log.h"

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

static size_t M_Offset(const INPUT_SCHEMA *const schema, const char *const name)
{
    for (size_t i = 0; i < schema->count; i++) {
        if (strcmp(schema->actions[i].name, name) == 0) {
            return schema->actions[i].offset;
        }
    }
    LOG_FATAL("The input schema has no %s button", name);
}

static float M_Length(const VEC2 offset)
{
    return sqrtf(offset.x * offset.x + offset.y * offset.y);
}

static bool M_IsEdge(const VEC2 offset)
{
    return offset.x == 0.0f || offset.y == 0.0f;
}

void Bot_Init(
    BOT *const bot, const INPUT_SCHEMA *const schema, const uint16_t seed,
    const bool passive)
{
    memset(bot, 0, sizeof(*bot));
    bot->schema = schema;
    bot->shoot = M_Offset(schema, "shoot");
    bot->bomb = M_Offset(schema, "bomb");
    bot->focus = M_Offset(schema, "focus");
    bot->up = M_Offset(schema, "up");
    bot->down = M_Offset(schema, "down");
    bot->left = M_Offset(schema, "left");
    bot->right = M_Offset(schema, "right");
    bot->passive = passive;
    Rng_SetSeed(&bot->rng, seed);
}

// Picks one of eight directions or none for 15 to 44 frames.
static void M_Wander(BOT *const bot)
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

void Bot_NextInput(
    BOT *const bot, const VEC2 *const threats, const int32_t count,
    void *const out)
{
    if (bot->bomb_cooldown > 0) {
        bot->bomb_cooldown--;
    }
    bool up, down, left, right;
    bool focus = false;
    bool bomb = false;
    if (bot->passive) {
        M_Wander(bot);
        up = bot->wander_up;
        down = bot->wander_down;
        left = bot->wander_left;
        right = bot->wander_right;
    } else {
        VEC2 field = M_Field(threats, count);
        field.x += Rng_F32InRange(&bot->rng, 2.0f * M_JITTER) - M_JITTER;
        field.y += Rng_F32InRange(&bot->rng, 2.0f * M_JITTER) - M_JITTER;
        up = field.y < -M_DEAD_ZONE;
        down = field.y > M_DEAD_ZONE;
        left = field.x < -M_DEAD_ZONE;
        right = field.x > M_DEAD_ZONE;
        const float nearest = M_NearestThreat(threats, count);
        focus = nearest < M_FOCUS_DISTANCE;
        if (nearest < M_BOMB_DISTANCE && bot->bomb_cooldown <= 0) {
            bomb = true;
            bot->bomb_cooldown = M_BOMB_COOLDOWN;
        }
    }
    uint8_t *const input = out;
    memset(input, 0, bot->schema->size);
    *(bool *)(input + bot->shoot) = true;
    *(bool *)(input + bot->bomb) = bomb;
    *(bool *)(input + bot->focus) = focus;
    *(bool *)(input + bot->up) = up;
    *(bool *)(input + bot->down) = down;
    *(bool *)(input + bot->left) = left;
    *(bool *)(input + bot->right) = right;
}
