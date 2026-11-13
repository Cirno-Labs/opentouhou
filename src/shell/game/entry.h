#pragma once

#include "shell/game/game.h"

// Runs a game from the SDL main callbacks. The entry file of each game
// defines SDL_MAIN_USE_CALLBACKS, includes <SDL3/SDL_main.h> and forwards
// SDL_AppInit, SDL_AppEvent, SDL_AppIterate and SDL_AppQuit to Shell_Init,
// Shell_Event, Shell_Iterate and Shell_Quit. Each SHELL_STATUS has the
// value of the SDL_AppResult of the same meaning, so a cast converts it.

typedef union SDL_Event SDL_Event;

typedef enum {
    SHELL_CONTINUE,
    SHELL_SUCCESS,
    SHELL_FAILURE,
} SHELL_STATUS;

// Starts the program that runs desc, with the command line of argc and
// argv. desc must stay valid until Shell_Quit returns.
SHELL_STATUS Shell_Init(const GAME_DESC *desc, int argc, char *argv[]);
// Handles one event of SDL.
SHELL_STATUS Shell_Event(const SDL_Event *event);
// Runs one display frame.
SHELL_STATUS Shell_Iterate(void);
// Shuts the game and the program down.
void Shell_Quit(void);
