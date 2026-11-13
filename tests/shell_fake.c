#include "shell_fake.h"

#include "core/base/assert.h"

FAKE_SHELL g_FakeShell;

static void M_Record(
    const char kind, const size_t action, const INPUT_BINDING binding)
{
    ASSERT(g_FakeShell.request_count < FAKE_SHELL_REQUEST_COUNT);
    g_FakeShell.requests[g_FakeShell.request_count++] =
        (FAKE_SHELL_REQUEST) { kind, action, binding };
}

void FakeShell_Reset(void)
{
    g_FakeShell = (FAKE_SHELL) {};
}

bool Shell_GetPadPress(INPUT_BINDING *const out)
{
    const bool pending = g_FakeShell.pad_pending;
    if (pending) {
        *out = g_FakeShell.pad_press;
    }
    g_FakeShell.pad_pending = false;
    return pending;
}

size_t Shell_GetBindings(
    const size_t action, INPUT_BINDING *const out, const size_t capacity)
{
    ASSERT(action < INPUT_MAX_ACTIONS);
    const size_t count = g_FakeShell.binding_counts[action];
    for (size_t i = 0; i < count && i < capacity; i++) {
        out[i] = g_FakeShell.bindings[action][i];
    }
    return count;
}

void Shell_RequestBind(const size_t action, const INPUT_BINDING binding)
{
    M_Record('b', action, binding);
}

void Shell_RequestUnbind(const size_t action, const INPUT_BINDING binding)
{
    M_Record('u', action, binding);
}

void Shell_RequestResetPadBindings(void)
{
    M_Record('r', 0, (INPUT_BINDING) {});
}

bool Shell_GetTickRate(float *const out)
{
    const bool measured = g_FakeShell.tick_rate_measured;
    if (measured) {
        *out = g_FakeShell.tick_rate;
    }
    g_FakeShell.tick_rate_measured = false;
    return measured;
}

bool Shell_IsFullscreen(void)
{
    return g_FakeShell.fullscreen;
}

void Shell_RequestFullscreen(const bool fullscreen)
{
    M_Record('f', fullscreen ? 1 : 0, (INPUT_BINDING) {});
}
