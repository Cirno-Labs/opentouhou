#pragma once

#include "core/input.h"

// The functions below let a game read the shell's tick rate and read and
// change the shell's input bindings and window mode from a tick. The reads
// give values that shell/game took before the ticks of the display frame.
// The requests are queued and applied after the tick, in order, so a tick
// never sees the shell change under it. shell/game saves the results to
// config.ini with its own settings.

// Returns whether a gamepad or joystick control went down since the tick
// before, and writes that one control into out. shell/game reads the pads
// once per display frame, and only the first tick after a press sees it.
bool Shell_GetPadPress(INPUT_BINDING *out);
// Writes up to capacity bindings of action, of player 1 in the game's input
// schema, into out, and returns how many the action has.
size_t Shell_GetBindings(size_t action, INPUT_BINDING *out, size_t capacity);
// Asks to add binding to action of player 1 in the game's input schema.
void Shell_RequestBind(size_t action, INPUT_BINDING binding);
// Asks to remove binding from action of player 1 in the game's input
// schema.
void Shell_RequestUnbind(size_t action, INPUT_BINDING binding);
// Asks to set the gamepad and joystick bindings of the game's actions back
// to the defaults. The keyboard bindings stay.
void Shell_RequestResetPadBindings(void);
// Returns whether the window is fullscreen.
bool Shell_IsFullscreen(void);
// Asks for the fullscreen or the windowed mode, as the fullscreen hotkey
// toggles it.
void Shell_RequestFullscreen(bool fullscreen);
// Returns whether shell/game measured the tick rate since the tick before,
// and writes the rate, in ticks per second, into out. shell/game counts the
// ticks of each period of half a second or more, outside the ticks, and
// only the first tick after a measurement sees it. The rate is
// SHELL_TICK_RATE at full speed and falls when the computer cannot keep up,
// because shell/game drops the ticks that it cannot catch up.
bool Shell_GetTickRate(float *out);
