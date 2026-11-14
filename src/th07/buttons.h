#pragma once

#include "core/input.h"

#include <stdint.h>

typedef struct {
    bool shoot;
    bool bomb;
    bool focus;
    bool menu;
    bool up;
    bool down;
    bool left;
    bool right;
    bool skip;
    bool quit;
    bool unlock_end;
    bool unlock_start;
    bool confirm;
    // Enters the score screen's cheat sequence.
    bool cheat;
    // Restarts from the pause menu.
    bool retry;
    // TODO: read by the analog_speed and analog_direction features. Until
    // they exist, only the direction buttons move the player.
    float move_x;
    float move_y;
} TH07_INPUT;

extern const INPUT_SCHEMA g_InputSchema;

// Holds a frame's buttons, as the original derives them from the raw
// input: what is down, what is new this calc and what counts as held
// for a menu repeat.
typedef struct {
    TH07_INPUT held; // down this calc
    TH07_INPUT pressed; // down this calc, up the calc before
    TH07_INPUT repeated; // pressed, or held on a calc that repeats
    uint16_t word;
    uint16_t previous_word;
    int32_t held_frames;
    bool repeat; // this calc repeats
} BUTTONS;

// Reads input into the word, then updates what is pressed and repeated.
void Buttons_Update(BUTTONS *buttons, const TH07_INPUT *input);

// Adds the buttons of input to the word, without a new previous word or
// repeat, as the original does on the calc after one that slow mode drops.
void Buttons_Merge(BUTTONS *buttons, const TH07_INPUT *input);

// Returns whether the masked word is nonzero and changed this calc.
bool Buttons_WasPressed(const BUTTONS *buttons, uint16_t mask);

// Returns whether Buttons_WasPressed holds for mask, or a bit of mask is
// down on a calc that repeats.
bool Buttons_WasPressedOrRepeated(const BUTTONS *buttons, uint16_t mask);

// Clears the words, the repeat and what derives from them, as a scene
// switch does. held_frames stays.
void Buttons_Clear(BUTTONS *buttons);

// Converts to and from the button word that replays store.
uint16_t Buttons_ToWord(const TH07_INPUT *input);
TH07_INPUT Buttons_FromWord(uint16_t word);
