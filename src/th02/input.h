#pragma once

// The input that one frame samples: the PC-98 key groups the game reads,
// the keys it maps them to, the date and the clock samples.

#include "th02/clock.h"

#include "core/input/schema.h"

typedef enum {
    GAME_KEY_UP = 0x0001,
    GAME_KEY_DOWN = 0x0002,
    GAME_KEY_LEFT = 0x0004,
    GAME_KEY_RIGHT = 0x0008,
    GAME_KEY_SHOT = 0x0010,
    GAME_KEY_BOMB = 0x0020,
    GAME_KEY_CANCEL = 0x0040,
    GAME_KEY_OK = 0x0080,
    GAME_KEY_Q = 0x0100,
    GAME_KEY_UP_LEFT = 0x1000,
    GAME_KEY_UP_RIGHT = 0x2000,
    GAME_KEY_DOWN_LEFT = 0x4000,
    GAME_KEY_DOWN_RIGHT = 0x8000,
} GAME_KEY;

// One frame's key state, with arrows and keypad apart.
typedef struct {
    bool up;
    bool down;
    bool left;
    bool right;
    bool up_left;
    bool up_right;
    bool down_left;
    bool down_right;
    bool shot;
    bool space;
    bool bomb;
    bool cancel;
    bool ok;
    bool q;
    bool extra_group_0;
    bool extra_group_3;
    bool extra_group_5;
    bool keypad_up;
    bool keypad_down;
    bool keypad_left;
    bool keypad_right;
    bool extra_group_1;
    bool extra_group_2;
    bool extra_group_4;
    bool extra_group_6;
    bool extra_group_7;
    bool logo_aliases;
    // Local date as YYYYMMDD.
    uint32_t date;
    // The frame's clock samples (th02/clock.h): the times at which the
    // frame reached its audio points, from the host or a replay. Valid
    // until the tick returns.
    const TH02_CLOCK_SAMPLE *clocks;
    size_t clock_count;
} TH02_INPUT;

typedef enum {
    KEY_WAIT_RELEASE,
    KEY_WAIT_PRESS,
    KEY_WAIT_DONE,
} KEY_WAIT_PHASE;

typedef struct {
    KEY_WAIT_PHASE phase;
    uint8_t frames_left;
    bool pending;
} KEY_WAIT;

extern const INPUT_SCHEMA g_Th02InputSchema;

// Returns the key mask, keeping contradictory bits; Space and Z both shoot.
uint16_t GameInput_Sense(const TH02_INPUT *sample);

// Reports a key that skips the logo; keypad directions do not.
bool GameInput_LogoKey(const TH02_INPUT *sample);

// Reports a key in BIOS groups 5, 3 or 0, other than Space and movement.
bool GameInput_AnyWaitKey(const TH02_INPUT *sample);

// Starts a release-then-press wait that samples every second frame.
void GameInput_BeginWait(KEY_WAIT *wait, const TH02_INPUT *sample);
bool GameInput_AdvanceWait(KEY_WAIT *wait, const TH02_INPUT *sample);
