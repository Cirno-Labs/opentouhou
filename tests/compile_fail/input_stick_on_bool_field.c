// SPDX-License-Identifier: GPL-3.0-only

// Compiles as it is, and fails to compile with BREAK defined, because it
// then declares an INPUT_STICK over bool fields.
#include "core/input.h"

typedef struct {
    bool move_x;
    bool move_y;
    float aim_x;
    float aim_y;
} GAME_INPUT;

void M_Use(void);

void M_Use(void)
{
#ifdef BREAK
    [[maybe_unused]] static const INPUT_ACTION actions[] = {
        INPUT_STICK(GAME_INPUT, move, "left_stick", "joy_axis_0, joy_axis_1"),
    };
#else
    [[maybe_unused]] static const INPUT_ACTION actions[] = {
        INPUT_STICK(GAME_INPUT, aim, "left_stick", "joy_axis_0, joy_axis_1"),
    };
#endif
}
