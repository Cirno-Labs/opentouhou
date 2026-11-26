#include "shell/null.h"

#include "shell/game.h"

#include <string.h>

static size_t m_InputSize;

void NullShell_SetInputSize(const size_t size)
{
    m_InputSize = size;
}

bool Shell_GetPadPress(INPUT_BINDING *const out)
{
    return false;
}

bool Shell_GetKeyPress(INPUT_BINDING *const out)
{
    return false;
}

void Shell_GetDeviceInput(const INPUT_DEVICE device, void *const out)
{
    memset(out, 0, m_InputSize);
}

size_t Shell_GetBindings(
    const size_t action, INPUT_BINDING *const out, const size_t capacity)
{
    return 0;
}

void Shell_RequestBind(const size_t action, const INPUT_BINDING binding)
{
}

void Shell_RequestUnbind(const size_t action, const INPUT_BINDING binding)
{
}

void Shell_RequestResetPadBindings(void)
{
}

bool Shell_IsFullscreen(void)
{
    return false;
}

void Shell_RequestFullscreen(const bool fullscreen)
{
}

void Shell_RequestSaveSettings(void)
{
}
