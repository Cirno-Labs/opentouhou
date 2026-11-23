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

extern GAME_INPUT g_GameInput;

typedef enum {
    MENU_INPUT_MAIN,
    MENU_INPUT_OPTIONS,
} MENU_INPUT_KIND;

typedef struct {
    bool previous_up[2], previous_down[2];
    bool left, right, ok, shot, cancel;
} MENU_INPUT;

// Updates the gameplay flags only when their physical sources change. Arrow
// and keypad sources stay separate, so a change can reassert a consumed
// flag while the combined direction stays held. bomb stays set until
// consumed or reset. Returns whether Return was pressed while paused and
// shooting, which sets player_is_hit in the original.
//
// In debug mode, sets the memory-screen flags as events instead of entering
// the original's recursive debug screens.
bool GameInput_SenseGame(const TH01_INPUT *raw, bool debug);

// Senses the ending's arrows, Z/X, Return and Escape, without keypad, debug
// keys or bomb double taps.
void GameInput_SenseEnding(const TH01_INPUT *raw);

// Clears every flag, count and history.
void GameInput_Init(void);

// Clears held flags, pause, the repeat history, the double-tap counts and
// the pending bomb. Keeps bomb_frame and the debug-key history, as
// input_reset_sense does.
void GameInput_ResetGame(void);

// Clears only horizontal, shot and ok, keeping the repeat history.
void GameInput_ResetMenu(void);

// Advances the signed 16-bit bomb window after a gameplay frame.
void GameInput_AdvanceBombFrame(void);

// Moves selection on up/down presses, in the original's order. Main and
// options keep separate history; the music room shares the options history.
// Options also update held left/right. Requires a positive choice_count and
// a selection inside it.
int32_t GameInput_SenseMenu(
    MENU_INPUT *state, const TH01_INPUT *raw, MENU_INPUT_KIND kind,
    int32_t choice_count, int32_t selection);

// Sets pending on a keyboard press or a bound confirmation control, ignoring
// modifier-only and lock-only presses. key_press may be nullptr. Misses the
// BIOS key repeat and taps that begin and end between polls.
void GameInput_LatchIntro(
    bool *pending, const TH01_INPUT *raw, const INPUT_BINDING *key_press);
