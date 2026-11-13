// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/game.h"

#include "core/base/memory.h"
#include "core/io/process.h"

#include <stdio.h>
#include <string.h>

void Game_GetProductName(const char *const game, char name[GAME_NAME_SIZE])
{
    snprintf(name, GAME_NAME_SIZE, "o%s", game);
}

RESULT Game_GetVersion(const char *const path, char version[GAME_VERSION_SIZE])
{
    version[0] = '\0';
    const char *const args[] = { path, "--version", nullptr };
    char *output = nullptr;
    int status = -1;
    MUST(Process_Run(args, &output, &status));
    output[strcspn(output, "\r\n")] = '\0';
    const char *const space = strrchr(output, ' ');
    const char *const word = space != nullptr ? space + 1 : "";
    const size_t length = strlen(word);
    RESULT result = OK;
    if (status != 0) {
        result = FAIL("it ended with status %d", status);
    } else if (space == nullptr || length == 0 || length >= GAME_VERSION_SIZE) {
        result = FAIL("it printed no version");
    } else {
        memcpy(version, word, length + 1);
    }
    Mem_Free(output);
    return result;
}
