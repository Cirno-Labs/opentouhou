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
    // TODO: read by the analog_speed and analog_direction features. Until
    // they exist, only the direction buttons move the player.
    float move_x;
    float move_y;
} TH06_INPUT;

extern const INPUT_SCHEMA g_InputSchema;

// Holds a frame's buttons, as the original derives them from the raw
// input: what is down, what is new this calc and what counts as held
// for a menu repeat.
typedef struct {
    TH06_INPUT held; // down this calc
    TH06_INPUT pressed; // down this calc, up the calc before
    TH06_INPUT repeated; // pressed, or held on a calc that repeats
    uint16_t word;
    uint16_t previous_word;
    int32_t held_frames;
} BUTTONS;

void Buttons_Update(BUTTONS *buttons, const TH06_INPUT *input);
// Replaces the gameplay bits with replay_word and repeats the input
// update against the previous word saved by Buttons_Update.
void Buttons_ApplyReplay(BUTTONS *buttons, uint16_t replay_word);
// Returns whether the masked word is nonzero and changed this calc.
bool Buttons_WasPressed(const BUTTONS *buttons, uint16_t mask);
// Returns whether Buttons_WasPressed holds for mask, or a bit of mask is
// down on a calc that repeats.
bool Buttons_WasPressedOrRepeated(const BUTTONS *buttons, uint16_t mask);
// Clears everything but held_frames, as a scene switch does.
void Buttons_Clear(BUTTONS *buttons);

// Converts to and from the button word that replays store.
uint16_t Buttons_ToWord(const TH06_INPUT *input);
TH06_INPUT Buttons_FromWord(uint16_t word);
