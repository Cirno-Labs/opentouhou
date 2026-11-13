#define SDL_MAIN_USE_CALLBACKS
#include "shell/game/entry.h"
#include "th06/main.h"

#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    return (SDL_AppResult)Shell_Init(&g_GameDesc, argc, argv);
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    return (SDL_AppResult)Shell_Event(event);
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    return (SDL_AppResult)Shell_Iterate();
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    Shell_Quit();
}
