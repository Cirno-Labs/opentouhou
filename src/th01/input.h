#pragma once

#include "core/input.h"

#define GAME_INPUT_BOMB_WINDOW 20

typedef struct {
    bool up;
    bool down;
    bool left;
    bool right;
    bool keypad_up;
    bool keypad_down;
    bool keypad_left;
    bool keypad_right;
    bool shot;
    bool strike;
    bool ok;
    bool cancel;
    bool memory_enter;
    bool memory_leave;
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
    uint8_t shot_presses;
    uint8_t strike_presses;
    int16_t bomb_frame;
    uint8_t horizontal;
    bool up;
    bool down;
    bool shot;
    bool strike;
    bool ok;
    bool paused;
    bool bomb;
    bool memory_enter;
    bool memory_leave;
} GAME_INPUT;

extern const GAME_INPUT *const g_GameInput;

typedef enum {
    GAME_INPUT_PRESS_UP,
    GAME_INPUT_PRESS_DOWN,
    GAME_INPUT_PRESS_HORIZONTAL,
    GAME_INPUT_PRESS_SHOT,
    GAME_INPUT_PRESS_STRIKE,
    GAME_INPUT_PRESS_OK,
    GAME_INPUT_PRESS_PAUSE,
    GAME_INPUT_PRESS_BOMB,
} GAME_INPUT_PRESS;

typedef enum {
    MENU_INPUT_MAIN,
    MENU_INPUT_OPTIONS,
} MENU_INPUT_KIND;

typedef struct {
    bool previous_up[2];
    bool previous_down[2];
    bool left;
    bool right;
    bool ok;
    bool shot;
    bool cancel;
} MENU_INPUT;

// Updates the gameplay flags, and in debug mode the memory-screen flags; bomb
// stays set until consumed. Returns whether Return was pressed while paused.
bool GameInput_SenseGame(const TH01_INPUT *raw, bool debug);

// Senses the ending's arrows, Z/X, Return and Escape, without keypad, debug
// keys or bomb double taps.
void GameInput_SenseEnding(const TH01_INPUT *raw);

// Clears every flag, count and history.
void GameInput_Init(void);

// Clears held flags, pause, repeat history, double taps and the pending bomb.
void GameInput_ResetGame(void);

// Clears only horizontal, shot and ok, keeping the repeat history.
void GameInput_ResetMenu(void);

// Clears the flag of one press after it took effect. The flag is set again
// only when its physical source changes.
void GameInput_Consume(GAME_INPUT_PRESS press);

// Sets shot and ok as held, so that a wait for both ends when either key
// changes.
void GameInput_HoldConfirm(void);

// Advances the signed 16-bit bomb window after a gameplay frame.
void GameInput_AdvanceBombFrame(void);

// Moves selection on up/down presses; options also update held left/right.
// Requires a positive choice_count and a selection inside it.
int32_t GameInput_SenseMenu(
    MENU_INPUT *state, const TH01_INPUT *raw, MENU_INPUT_KIND kind,
    int32_t choice_count, int32_t selection);

// Pushes the bomb window past its edge, so no double tap bombs for frames.
void GameInput_DisableBomb(int16_t frames);

// Sets the bomb window at its edge, so the next double tap counts afresh.
void GameInput_OpenBombWindow(void);

// Sets pending on a key press or bound confirmation, ignoring modifiers.
// Misses taps that begin and end between polls.
void GameInput_LatchIntro(
    bool *pending, const TH01_INPUT *raw, const INPUT_BINDING *key_press);
