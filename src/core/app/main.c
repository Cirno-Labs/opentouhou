// Defines the program's entry point, which hands control to the SDL main
// callbacks in app.c. The executable links this file whole, because the
// entry point that Windows starts from (WinMain) is looked up only after
// the libraries are searched.
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
