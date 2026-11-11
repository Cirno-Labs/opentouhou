#include "app_fake.h"

#include "core/base/assert.h"

FAKE_APP g_FakeApp;

static void M_Record(
    const char kind, const size_t action, const INPUT_BINDING binding)
{
    ASSERT(g_FakeApp.request_count < FAKE_APP_REQUEST_COUNT);
    g_FakeApp.requests[g_FakeApp.request_count++] =
        (FAKE_APP_REQUEST) { kind, action, binding };
}

void FakeApp_Reset(void)
{
    g_FakeApp = (FAKE_APP) {};
}

bool App_GetPadPress(INPUT_BINDING *const out)
{
    const bool pending = g_FakeApp.pad_pending;
    if (pending) {
        *out = g_FakeApp.pad_press;
    }
    g_FakeApp.pad_pending = false;
    return pending;
}

size_t App_GetBindings(
    const size_t action, INPUT_BINDING *const out, const size_t capacity)
{
    ASSERT(action < INPUT_MAX_ACTIONS);
    const size_t count = g_FakeApp.binding_counts[action];
    for (size_t i = 0; i < count && i < capacity; i++) {
        out[i] = g_FakeApp.bindings[action][i];
    }
    return count;
}

void App_RequestBind(const size_t action, const INPUT_BINDING binding)
{
    M_Record('b', action, binding);
}

void App_RequestUnbind(const size_t action, const INPUT_BINDING binding)
{
    M_Record('u', action, binding);
}

void App_RequestResetPadBindings(void)
{
    M_Record('r', 0, (INPUT_BINDING) {});
}

bool App_GetTickRate(float *const out)
{
    const bool measured = g_FakeApp.tick_rate_measured;
    if (measured) {
        *out = g_FakeApp.tick_rate;
    }
    g_FakeApp.tick_rate_measured = false;
    return measured;
}

bool App_IsFullscreen(void)
{
    return g_FakeApp.fullscreen;
}

void App_RequestFullscreen(const bool fullscreen)
{
    M_Record('f', fullscreen ? 1 : 0, (INPUT_BINDING) {});
}
