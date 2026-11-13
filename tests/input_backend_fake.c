#include "input_backend_fake.h"

#include "core/base/assert.h"
#include "core/base/memory.h"

#include <string.h>

#define FAKE_MAX_CONTROLLERS 16

struct INPUT_BACKEND {
    const ALLOCATOR *allocator;
    uint8_t keys[32];
    INPUT_CONTROLLER_STATE controllers[FAKE_MAX_CONTROLLERS];
    size_t controller_count;
};

static INPUT_BACKEND *m_Current;

INPUT_BACKEND *InputBackend_Create(const ALLOCATOR *const allocator)
{
    INPUT_BACKEND *const backend =
        Allocator_AllocZ(allocator, sizeof(INPUT_BACKEND));
    backend->allocator = allocator;
    m_Current = backend;
    return backend;
}

void InputBackend_Destroy(INPUT_BACKEND *const backend)
{
    if (backend == nullptr) {
        return;
    }
    if (m_Current == backend) {
        m_Current = nullptr;
    }
    Allocator_Free(backend->allocator, backend, sizeof(INPUT_BACKEND));
}

void InputBackend_Poll(INPUT_BACKEND *const backend, INPUT_DEVICES *const out)
{
    memcpy(out->keys, backend->keys, sizeof(out->keys));
    out->controllers = backend->controllers;
    out->controller_count = backend->controller_count;
}

void FakeInput_SetKey(const int code, const bool down)
{
    if (down) {
        m_Current->keys[code / 8] |= (uint8_t)(1u << (code % 8));
    } else {
        m_Current->keys[code / 8] &= (uint8_t)~(1u << (code % 8));
    }
}

static INPUT_CONTROLLER_STATE *M_Find(const uint32_t id)
{
    for (size_t i = 0; i < m_Current->controller_count; i++) {
        if (m_Current->controllers[i].id == id) {
            return &m_Current->controllers[i];
        }
    }
    return nullptr;
}

void FakeInput_Connect(
    const uint32_t id, const INPUT_DEVICE device, const char *const name,
    const char *const guid)
{
    if (M_Find(id) != nullptr) {
        return;
    }
    ASSERT(m_Current->controller_count < FAKE_MAX_CONTROLLERS);
    INPUT_CONTROLLER_STATE *const state =
        &m_Current->controllers[m_Current->controller_count++];
    *state =
        (INPUT_CONTROLLER_STATE) { .id = id, .device = device, .name = name };
    const size_t length = strlen(guid);
    memcpy(state->guid, guid, length < 32 ? length : 32);
    state->guid[length < 32 ? length : 32] = '\0';
}

void FakeInput_Disconnect(const uint32_t id)
{
    for (size_t i = 0; i < m_Current->controller_count; i++) {
        if (m_Current->controllers[i].id == id) {
            m_Current->controllers[i] =
                m_Current->controllers[m_Current->controller_count - 1];
            m_Current->controller_count--;
            return;
        }
    }
}

void FakeInput_SetButton(const uint32_t id, const int bit, const bool down)
{
    INPUT_CONTROLLER_STATE *const state = M_Find(id);
    if (state == nullptr) {
        return;
    }
    if (down) {
        state->buttons[bit / 8] |= (uint8_t)(1u << (bit % 8));
    } else {
        state->buttons[bit / 8] &= (uint8_t)~(1u << (bit % 8));
    }
}

void FakeInput_SetAxis(const uint32_t id, const int axis, const float value)
{
    INPUT_CONTROLLER_STATE *const state = M_Find(id);
    if (state == nullptr) {
        return;
    }
    state->axes[axis] = value;
}

void FakeInput_SetHatBits(const uint32_t id, const int hat, const uint8_t bits)
{
    INPUT_CONTROLLER_STATE *const state = M_Find(id);
    if (state == nullptr) {
        return;
    }
    state->hats[hat] = bits;
}
