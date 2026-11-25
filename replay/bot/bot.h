#pragma once

#include "core/base/result.h"
#include "core/input/schema.h"

#include <stddef.h>
#include <stdint.h>

// Translates the roles below into the buttons of a game's INPUT_SCHEMA, as
// the game's bot.ini gives them in its [roles] section:
//
//     shoot = shot
//     bomb = shot . shot . strike . strike
//
// A value holds one word per frame: a button name, several joined by '+'
// to hold them in one frame, or '.' for a frame with none. A role that
// bot.ini leaves out presses nothing.

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

// Holds the buttons of one role: one mask of schema action indices per
// frame.
typedef struct {
    uint32_t frames[BOT_MAX_FRAMES];
    int32_t count;
} BOT_SEQUENCE;

typedef struct {
    const INPUT_SCHEMA *schema;
    BOT_SEQUENCE roles[BOT_ROLE_COUNT];
} BOT;

// Sets bot up from the bot.ini at path, with the buttons of schema. Fails
// when the file is missing, or names a role or button that does not exist.
RESULT Bot_Load(BOT *bot, const char *path, const INPUT_SCHEMA *schema);

// Sets bot up from size bytes of bot.ini text, as Bot_Load does.
RESULT Bot_LoadText(
    BOT *bot, const char *text, size_t size, const INPUT_SCHEMA *schema);

// Returns the mask, one bit per BOT_ROLE, of the roles that press a button.
uint32_t Bot_GetRoles(const BOT *bot);

// Returns the frames that the first role of several frames in roles takes,
// or 0 when there is none.
int32_t Bot_GetLeadFrames(const BOT *bot, uint32_t roles);

// Writes the input of the frame-th frame of a hold of roles, a mask of
// BOT_ROLE bits, into out, a struct of the schema's size. The first role
// of several frames in BOT_ROLE order plays its frames alone at the start
// of the hold; the buttons of the other roles of one frame fill the rest.
void Bot_WriteInput(const BOT *bot, uint32_t roles, int32_t frame, void *out);
