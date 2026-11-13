// Compiles as it is, and fails to compile with BREAK defined, because it
// then declares an INPUT_BUTTON over a float field.
#include "core/input.h"

typedef struct {
    bool shoot;
    float volume;
} GAME_INPUT;

void M_Use(void);

void M_Use(void)
{
#ifdef BREAK
    [[maybe_unused]] static const INPUT_ACTION actions[] = {
        INPUT_BUTTON(GAME_INPUT, volume, "a", "south", "joy_button_0"),
    };
#else
    [[maybe_unused]] static const INPUT_ACTION actions[] = {
        INPUT_BUTTON(GAME_INPUT, shoot, "a", "south", "joy_button_0"),
    };
#endif
}
