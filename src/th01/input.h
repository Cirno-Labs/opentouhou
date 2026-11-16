#pragma once

#include "core/input.h"

#define GAME_INPUT_BOMB_WINDOW 20

typedef struct {
    bool up, down, left, right;
    bool keypad_up, keypad_down, keypad_left, keypad_right;
    bool shot, strike, ok, cancel;
    bool memory_enter, memory_leave;
} TH01_INPUT;

extern const INPUT_SCHEMA g_InputSchema;

typedef enum {
    GAME_INPUT_NONE = 0,
    GAME_INPUT_RIGHT = 1,
    GAME_INPUT_LEFT = 2,
    GAME_INPUT_BOTH = 3,
} GAME_INPUT_HORIZONTAL;

typedef struct {
    TH01_INPUT previous;
    uint8_t shot_presses, strike_presses;
    int16_t bomb_frame;
    uint8_t horizontal;
    bool up, down, shot, strike, ok, paused, bomb;
    bool memory_enter, memory_leave;
} GAME_INPUT;

typedef enum {
    MENU_INPUT_MAIN,
    MENU_INPUT_OPTIONS,
} MENU_INPUT_KIND;

typedef struct {
    bool previous_up[2], previous_down[2];
    bool left, right, ok, shot, cancel;
} MENU_INPUT;

// Updates gameplay flags only when their physical sources change. Arrow
// and keypad sources stay separate so a change can reassert a consumed
// flag even while the combined direction stays held. Bomb stays set until
// consumed or reset. Returns whether Return was pressed while paused and
// shooting, which sets player_is_hit in the original.
//
// Debug mode updates memory-screen flags as events without entering the
// original recursive debug screens. A zero-initialized state is ready.
bool GameInput_SenseGame(GAME_INPUT *state, const TH01_INPUT *raw, bool debug);

// Clears repeat history and double-tap counts, keeping debug-key history,
// held flags, pause and bomb_frame. Clears the pending bomb flag.
void GameInput_ResetRepeat(GAME_INPUT *state);

// Clears held flags and pause, then resets repeat history. Keeps bomb_frame
// and the debug-key history, as input_reset_sense does.
void GameInput_ResetGame(GAME_INPUT *state);

// Clears only horizontal, shot and OK, keeping all repeat history.
void GameInput_ResetMenu(GAME_INPUT *state);

// Advances the signed 16-bit bomb window after a gameplay frame. Sensing
// input, menu waits and host ticks do not advance it automatically.
void GameInput_AdvanceBombFrame(GAME_INPUT *state);

// Updates menu selection on up/down presses, in source order. Main and
// options keep independent history; the music room shares options history.
// Options also update held left/right. Both update held OK, shot and cancel.
// Requires a positive choice_count and a selection inside its range.
int32_t GameInput_SenseMenu(
    MENU_INPUT *state, const TH01_INPUT *raw, MENU_INPUT_KIND kind,
    int32_t choice_count, int32_t selection);

// Latches a sampled keyboard press or a bound confirmation control until
// pending is cleared. Ignores modifier-only and lock-only keyboard presses.
// key_press may be nullptr. Clear pending where the original clears its
// BIOS buffer. Device polling does not reproduce BIOS key repeat or taps
// that begin and end between polls.
void GameInput_LatchIntro(
    bool *pending, const TH01_INPUT *raw, const INPUT_BINDING *key_press);
