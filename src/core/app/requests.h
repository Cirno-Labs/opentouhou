#pragma once

#include "core/input.h"

// The functions below let a game read and change core's input bindings from
// a tick. The reads give values that core/app took before the ticks of the
// display frame. The requests are queued and applied after the tick, in
// order, so a tick never sees core change under it. core/app saves the
// results to config.ini with its own settings.

// Returns whether a gamepad or joystick control went down since the tick
// before, and writes that one control into out. core/app reads the pads
// once per display frame, and only the first tick after a press sees it.
bool App_GetPadPress(INPUT_BINDING *out);
// Writes up to capacity bindings of action, of player 1 in the game's input
// schema, into out, and returns how many the action has.
size_t App_GetBindings(size_t action, INPUT_BINDING *out, size_t capacity);
// Asks to add binding to action of player 1 in the game's input schema.
void App_RequestBind(size_t action, INPUT_BINDING binding);
// Asks to remove binding from action of player 1 in the game's input
// schema.
void App_RequestUnbind(size_t action, INPUT_BINDING binding);
// Asks to set the gamepad and joystick bindings of the game's actions back
// to the defaults. The keyboard bindings stay.
void App_RequestResetPadBindings(void);
