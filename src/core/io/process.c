#include "core/io/process.h"

#include "core/base/memory.h"

#include <SDL3/SDL.h>
#include <string.h>

RESULT Process_Run(
    const char *const *const args, char **const output, int *const status)
{
    *output = nullptr;
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
    FAIL_IF(process == nullptr, "cannot start the program: %s", SDL_GetError());
    size_t size = 0;
    char *const text = SDL_ReadProcess(process, &size, status);
    SDL_DestroyProcess(process);
    FAIL_IF(text == nullptr, "cannot read the output: %s", SDL_GetError());
    *output = Mem_Alloc(size + 1);
    memcpy(*output, text, size);
    (*output)[size] = '\0';
    SDL_free(text);
    return OK;
}
