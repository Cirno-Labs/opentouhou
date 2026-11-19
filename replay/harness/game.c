#include "game.h"

#include <string.h>

const BINDING *Game_FindBinding(
    const HARNESS_GAME *const game, const char *const name)
{
    for (size_t i = 0; i < game->binding_count; i++) {
        if (strcmp(game->bindings[i].name, name) == 0) {
            return &game->bindings[i];
        }
    }
    return nullptr;
}
