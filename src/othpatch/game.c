#include "othpatch/game.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static RESULT M_Run(const char *const path, char **const out, int *const status)
{
    const char *const args[] = { path, "--version", nullptr };
    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetPointerProperty(
        properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
    SDL_SetNumberProperty(
        properties, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,
        SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(
        properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,
        SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(
        properties, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,
        SDL_PROCESS_STDIO_NULL);
    SDL_Process *const process = SDL_CreateProcessWithProperties(properties);
    SDL_DestroyProperties(properties);
    FAIL_IF(process == nullptr, "cannot start it: %s", SDL_GetError());
    *out = SDL_ReadProcess(process, nullptr, status);
    SDL_DestroyProcess(process);
    FAIL_IF(*out == nullptr, "cannot read its output: %s", SDL_GetError());
    return OK;
}

void Game_GetProductName(const char *const game, char name[GAME_NAME_SIZE])
{
    snprintf(name, GAME_NAME_SIZE, "o%s", game);
}

RESULT Game_GetVersion(const char *const path, char version[GAME_VERSION_SIZE])
{
    version[0] = '\0';
    char *output = nullptr;
    int status = -1;
    MUST(M_Run(path, &output, &status));
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
    SDL_free(output);
    return result;
}
