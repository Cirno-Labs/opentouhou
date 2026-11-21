#pragma once

#include "fields.h"
#include "game.h"

// Describes th01 to the harness: its button word and the state columns
// of fields.h.

#define TH01_COLUMN_ENUM(name, kind) TH01_COLUMN_##name,
enum { TH01_COLUMNS(TH01_COLUMN_ENUM) TH01_COLUMN_COUNT };
#undef TH01_COLUMN_ENUM

// Lists one bit per action of th01's input schema, named as the action.
#define TH01_BUTTONS(X)                                                        \
    X(up, 0x1)                                                                 \
    X(down, 0x2)                                                               \
    X(left, 0x4)                                                               \
    X(right, 0x8)                                                              \
    X(keypad_up, 0x10)                                                         \
    X(keypad_down, 0x20)                                                       \
    X(keypad_left, 0x40)                                                       \
    X(keypad_right, 0x80)                                                      \
    X(shot, 0x100)                                                             \
    X(strike, 0x200)                                                           \
    X(ok, 0x400)                                                               \
    X(cancel, 0x800)                                                           \
    X(memory_enter, 0x1000)                                                    \
    X(memory_leave, 0x2000)

// Gives id "th01", version "1.10", 60 frames per second, the 640 by 400
// screen and draw_frames 1, since every frame shows whole pages.
extern const HARNESS_GAME g_Th01Harness;
