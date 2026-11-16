#include "bot.h"

#include "core/base/log.h"

#include <string.h>

// Finds the byte offset of a button action by name; the bot's games all
// declare these seven, so a missing one is a programming error.
static size_t M_Offset(const INPUT_SCHEMA *const schema, const char *const name)
{
    for (size_t i = 0; i < schema->count; i++) {
        if (strcmp(schema->actions[i].name, name) == 0) {
            return schema->actions[i].offset;
        }
    }
    LOG_FATAL("recorder bot: input schema has no %s action", name);
}

void Bot_Init(
    BOT *const bot, const INPUT_SCHEMA *const schema, const uint16_t seed,
    const bool passive)
{
    *bot = (BOT) {
        .schema = schema,
        .shoot = M_Offset(schema, "shoot"),
        .bomb = M_Offset(schema, "bomb"),
        .focus = M_Offset(schema, "focus"),
        .up = M_Offset(schema, "up"),
        .down = M_Offset(schema, "down"),
        .left = M_Offset(schema, "left"),
        .right = M_Offset(schema, "right"),
        .passive = passive,
    };
    Rng_SetSeed(&bot->rng, seed);
}

// Picks a direction for 15 to 44 frames, from one of eight compass points
// or none; used only in passive mode.
static void M_PickDirection(BOT *const bot)
{
    bot->direction_frames = 15 + Rng_U16InRange(&bot->rng, 30);
    const uint16_t pick = Rng_U16InRange(&bot->rng, 9);
    bot->holds_up = pick == 0 || pick == 4 || pick == 5;
    bot->holds_down = pick == 1 || pick == 6 || pick == 7;
    bot->holds_left = pick == 2 || pick == 4 || pick == 6;
    bot->holds_right = pick == 3 || pick == 5 || pick == 7;
}

// True for an offset list_threats built from one of the 4 play area
// edges: by construction one coordinate is exactly 0.
static bool M_IsEdge(const VEC2 offset)
{
    return offset.x == 0.0f || offset.y == 0.0f;
}

// Returns the nearest non-edge threat's distance, or a large value when
// none is within threats[0..count).
static float M_NearestRealThreat(const VEC2 *const threats, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        if (!M_IsEdge(threats[i])) {
            return Vec2_Length(threats[i]);
        }
    }
    return 1.0e6f;
}

// Sums repulsion from every threat within BOT_THREAT_RADIUS, edges
// included, which keeps the bot off the walls.
static VEC2 M_Repulsion(const VEC2 *const threats, const int32_t count)
{
    VEC2 field = { 0.0f, 0.0f };
    for (int32_t i = 0; i < count; i++) {
        const VEC2 offset = threats[i];
        const float dist = Vec2_Length(offset);
        if (dist < 0.01f || dist >= BOT_THREAT_RADIUS) {
            continue;
        }
        const float scale = -BOT_REPULSION_SCALE / (dist * dist * dist);
        field.x += offset.x * scale;
        field.y += offset.y * scale;
    }
    return field;
}

// Pulls weakly toward a home spot low in the centre of the play area:
// the left and right edge offsets sum to twice the offset from the
// player to the horizontal centre, and the bottom edge offset points
// straight down from the player. Both pulls fade as the player nears
// the spot, and apply regardless of BOT_THREAT_RADIUS.
static VEC2 M_HomePull(const VEC2 *const threats, const int32_t count)
{
    VEC2 pull = { 0.0f, 0.0f };
    for (int32_t i = 0; i < count; i++) {
        const VEC2 offset = threats[i];
        if (offset.y == 0.0f) {
            pull.x += offset.x;
        } else if (offset.x == 0.0f && offset.y > 0.0f) {
            pull.y += offset.y;
        }
    }
    pull.x *= BOT_HOME_PULL_X;
    pull.y *= BOT_HOME_PULL_Y;
    return pull;
}

void Bot_NextInput(
    BOT *const bot, const VEC2 *const threats, const int32_t count,
    void *const out)
{
    if (bot->bomb_cooldown_frames > 0) {
        bot->bomb_cooldown_frames--;
    }

    bool holds_up = false, holds_down = false;
    bool holds_left = false, holds_right = false;
    bool focus = false;
    bool bomb = false;

    if (bot->passive) {
        if (bot->direction_frames <= 0) {
            M_PickDirection(bot);
        }
        bot->direction_frames--;
        holds_up = bot->holds_up;
        holds_down = bot->holds_down;
        holds_left = bot->holds_left;
        holds_right = bot->holds_right;
    } else {
        const VEC2 repulsion = M_Repulsion(threats, count);
        const VEC2 home = M_HomePull(threats, count);
        VEC2 field = { repulsion.x + home.x, repulsion.y + home.y };
        field.x += Rng_F32InRange(&bot->rng, 2.0f * BOT_JITTER) - BOT_JITTER;
        field.y += Rng_F32InRange(&bot->rng, 2.0f * BOT_JITTER) - BOT_JITTER;

        holds_up = field.y < -BOT_DEAD_ZONE;
        holds_down = field.y > BOT_DEAD_ZONE;
        holds_left = field.x < -BOT_DEAD_ZONE;
        holds_right = field.x > BOT_DEAD_ZONE;

        const float nearest = M_NearestRealThreat(threats, count);
        focus = nearest < BOT_FOCUS_DISTANCE;
        if (nearest < BOT_BOMB_DISTANCE && bot->bomb_cooldown_frames <= 0) {
            bomb = true;
            bot->bomb_cooldown_frames = BOT_BOMB_COOLDOWN_FRAMES;
        }
    }

    memset(out, 0, bot->schema->size);
    *(bool *)((uint8_t *)out + bot->shoot) = true;
    *(bool *)((uint8_t *)out + bot->bomb) = bomb;
    *(bool *)((uint8_t *)out + bot->focus) = focus;
    *(bool *)((uint8_t *)out + bot->up) = holds_up;
    *(bool *)((uint8_t *)out + bot->down) = holds_down;
    *(bool *)((uint8_t *)out + bot->left) = holds_left;
    *(bool *)((uint8_t *)out + bot->right) = holds_right;
}
