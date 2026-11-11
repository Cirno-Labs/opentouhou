#include "core/app.h"

bool App_GetPadPress(INPUT_BINDING *const out)
{
    return false;
}

size_t App_GetBindings(
    const size_t action, INPUT_BINDING *const out, const size_t capacity)
{
    return 0;
}

void App_RequestBind(const size_t action, const INPUT_BINDING binding)
{
}

void App_RequestUnbind(const size_t action, const INPUT_BINDING binding)
{
}

void App_RequestResetPadBindings(void)
{
}

bool App_IsFullscreen(void)
{
    return false;
}

void App_RequestFullscreen(const bool fullscreen)
{
}
