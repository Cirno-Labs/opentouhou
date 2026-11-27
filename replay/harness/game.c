#include "replay/harness/game.h"

#include "core/base/string.h"

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

int32_t Game_FindStart(const HARNESS_GAME *const game, const char *const name)
{
    for (size_t i = 0; i < game->start_count; i++) {
        if (strcmp(game->starts[i].name, name) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

bool Game_ParseValue(
    const COLUMN_KIND kind, const char *const text, uint32_t *const out)
{
    if (kind == COLUMN_I32 && text[0] == '-') {
        uint32_t magnitude;
        if (!Str_ParseU32(text + 1, &magnitude) || magnitude > 0x80000000u) {
            return false;
        }
        *out = (uint32_t)0 - magnitude;
        return true;
    }
    uint32_t value;
    if (!Str_ParseU32(text, &value)
        || (kind == COLUMN_I32 && value > INT32_MAX)) {
        return false;
    }
    *out = value;
    return true;
}
