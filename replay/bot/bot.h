#pragma once

#include "core/base/result.h"
#include "core/input/schema.h"
#include "core/math/vec.h"

#include "stg/rng.h"

#include <stddef.h>
#include <stdint.h>

// Plays a game deterministically through the roles below, which a
// strategy picks each frame. The game's bot.ini translates each role into
// the buttons of its INPUT_SCHEMA, in its [roles] section:
//
//     shoot = shot
//     bomb = shot . shot . strike . strike
//
// A value holds one word per frame: a button name, several joined by '+'
// to hold them in one frame, or '.' for a frame with none. A role that
// bot.ini leaves out presses nothing. Uses an RNG of its own, so that one
// seed always gives the same inputs and the gameplay RNG is never touched.
//
// A threat offset with x or y exactly 0 is one of the play area's edges,
// which the game lists beside its bullets, lasers and enemies.

#define BOT_MAX_THREATS 32
#define BOT_MAX_FRAMES 32

#define BOT_ROLE_NAMES(X)                                                      \
    X(shoot)                                                                   \
    X(focus)                                                                   \
    X(bomb)                                                                    \
    X(strike)                                                                  \
    X(up)                                                                      \
    X(down)                                                                    \
    X(left)                                                                    \
    X(right)

#define BOT_ROLE_ENUM(name) BOT_ROLE_##name,
typedef enum { BOT_ROLE_NAMES(BOT_ROLE_ENUM) BOT_ROLE_COUNT } BOT_ROLE;
#undef BOT_ROLE_ENUM

// Says which roles a strategy plays in one frame.
typedef struct {
#define BOT_ROLE_FIELD(name) bool name;
    BOT_ROLE_NAMES(BOT_ROLE_FIELD)
#undef BOT_ROLE_FIELD
} BOT_ROLES;

// Holds the buttons of one role: one mask of schema action indices per
// frame.
typedef struct {
    uint32_t frames[BOT_MAX_FRAMES];
    int32_t count;
} BOT_SEQUENCE;

typedef struct BOT BOT;

typedef struct {
    const char *name;
    // Writes the roles of the next frame into out. threats holds the
    // offsets from the player to count threats and the edges, nearest
    // first.
    void (*next)(BOT *bot, const VEC2 *threats, int32_t count, BOT_ROLES *out);
} BOT_STRATEGY;

// Shoots always, dodges along a potential field of the threats near the
// player, with a weak pull toward a spot low in the center of the play
// area, and focuses and bombs by the nearest threat's distance.
extern const BOT_STRATEGY g_BotDodge;

// Shoots always and wanders in one of eight directions or none for 15 to
// 44 frames at a time, without dodging, focusing or bombing.
extern const BOT_STRATEGY g_BotWander;

struct BOT {
    const INPUT_SCHEMA *schema;
    const BOT_STRATEGY *strategy;
    BOT_SEQUENCE roles[BOT_ROLE_COUNT];
    RNG rng;
    // Points to the sequence that plays over the next frames, or nullptr.
    const BOT_SEQUENCE *running;
    int32_t running_frame;
    int32_t wander_frames;
    bool wander_up, wander_down, wander_left, wander_right;
    int32_t bomb_cooldown;
};

// Returns the strategy named name, or nullptr.
const BOT_STRATEGY *Bot_FindStrategy(const char *name);

// Sets bot up from the bot.ini at path, with the buttons of schema. Fails
// when the file is missing, or names a role or button that does not exist.
RESULT Bot_Load(
    BOT *bot, const char *path, const INPUT_SCHEMA *schema,
    const BOT_STRATEGY *strategy, uint16_t seed);

// Sets bot up from size bytes of bot.ini text, as Bot_Load does.
RESULT Bot_LoadText(
    BOT *bot, const char *text, size_t size, const INPUT_SCHEMA *schema,
    const BOT_STRATEGY *strategy, uint16_t seed);

// Writes the next input into out, a struct of the schema's size. A role
// whose value spans several frames plays them all over the next frames,
// with nothing else, once the strategy picks it; when the strategy picks
// several such roles at once, the first in BOT_ROLE order plays.
void Bot_NextInput(BOT *bot, const VEC2 *threats, int32_t count, void *out);
