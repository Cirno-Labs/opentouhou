// SPDX-License-Identifier: GPL-3.0-only

#include "core/input/backend.h"

#include "core/base/assert.h"
#include "core/base/memory.h"
#include "core/input/codes.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#define BACKEND_MAX_CONTROLLERS 16
#define BACKEND_NAME_CAPACITY 64

typedef struct {
    uint32_t id;
    INPUT_DEVICE device;
    SDL_Gamepad *gamepad;
    SDL_Joystick *joystick;
    char name[BACKEND_NAME_CAPACITY];
} OPEN_CONTROLLER;

struct INPUT_BACKEND {
    const ALLOCATOR *allocator;
    OPEN_CONTROLLER open[BACKEND_MAX_CONTROLLERS];
    size_t open_count;
    INPUT_CONTROLLER_STATE states[BACKEND_MAX_CONTROLLERS];
};

// Maps a GAMEPAD_CODE button (see codes.h) to the SDL_GamepadButton that
// reports it. The two enums use different orders.
static const SDL_GamepadButton m_GamepadButtonForCode[] = {
    [GAMEPAD_SOUTH] = SDL_GAMEPAD_BUTTON_SOUTH,
    [GAMEPAD_EAST] = SDL_GAMEPAD_BUTTON_EAST,
    [GAMEPAD_WEST] = SDL_GAMEPAD_BUTTON_WEST,
    [GAMEPAD_NORTH] = SDL_GAMEPAD_BUTTON_NORTH,
    [GAMEPAD_LEFT_SHOULDER] = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    [GAMEPAD_RIGHT_SHOULDER] = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    [GAMEPAD_BACK] = SDL_GAMEPAD_BUTTON_BACK,
    [GAMEPAD_START] = SDL_GAMEPAD_BUTTON_START,
    [GAMEPAD_GUIDE] = SDL_GAMEPAD_BUTTON_GUIDE,
    [GAMEPAD_LEFT_STICK_PRESS] = SDL_GAMEPAD_BUTTON_LEFT_STICK,
    [GAMEPAD_RIGHT_STICK_PRESS] = SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    [GAMEPAD_DPAD_UP] = SDL_GAMEPAD_BUTTON_DPAD_UP,
    [GAMEPAD_DPAD_DOWN] = SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    [GAMEPAD_DPAD_LEFT] = SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    [GAMEPAD_DPAD_RIGHT] = SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
};

static float M_NormalizeAxis(const Sint16 value)
{
    const float normalized =
        value < 0 ? (float)value / 32768.0f : (float)value / 32767.0f;
    return normalized < -1.0f ? -1.0f : normalized > 1.0f ? 1.0f : normalized;
}

static void M_SetBit(
    uint8_t *const bits, const size_t capacity, const int index)
{
    if (index >= 0 && (size_t)index < capacity * 8) {
        bits[index / 8] |= (uint8_t)(1u << (index % 8));
    }
}

static void M_CloseController(OPEN_CONTROLLER *const open)
{
    if (open->gamepad != nullptr) {
        SDL_CloseGamepad(open->gamepad);
    }
    if (open->joystick != nullptr) {
        SDL_CloseJoystick(open->joystick);
    }
}

static OPEN_CONTROLLER *M_Find(INPUT_BACKEND *const backend, const uint32_t id)
{
    for (size_t i = 0; i < backend->open_count; i++) {
        if (backend->open[i].id == id) {
            return &backend->open[i];
        }
    }
    return nullptr;
}

static void M_OpenGamepad(INPUT_BACKEND *const backend, const SDL_JoystickID id)
{
    if (backend->open_count >= BACKEND_MAX_CONTROLLERS) {
        return;
    }
    SDL_Gamepad *const gamepad = SDL_OpenGamepad(id);
    if (gamepad == nullptr) {
        return;
    }
    OPEN_CONTROLLER *const open = &backend->open[backend->open_count++];
    open->id = (uint32_t)id;
    open->device = INPUT_DEVICE_GAMEPAD;
    open->gamepad = gamepad;
    open->joystick = nullptr;
    const char *const name = SDL_GetGamepadName(gamepad);
    snprintf(open->name, sizeof(open->name), "%s", name != nullptr ? name : "");
}

static void M_OpenJoystick(
    INPUT_BACKEND *const backend, const SDL_JoystickID id)
{
    if (backend->open_count >= BACKEND_MAX_CONTROLLERS) {
        return;
    }
    SDL_Joystick *const joystick = SDL_OpenJoystick(id);
    if (joystick == nullptr) {
        return;
    }
    OPEN_CONTROLLER *const open = &backend->open[backend->open_count++];
    open->id = (uint32_t)id;
    open->device = INPUT_DEVICE_JOYSTICK;
    open->gamepad = nullptr;
    open->joystick = joystick;
    const char *const name = SDL_GetJoystickName(joystick);
    snprintf(open->name, sizeof(open->name), "%s", name != nullptr ? name : "");
}

// Drops every open controller whose id is not in ids, and opens every
// gamepad in gamepad_ids and every id in joystick_ids that is not open yet.
static void M_SyncControllers(
    INPUT_BACKEND *const backend, const SDL_JoystickID *const gamepad_ids,
    const int gamepad_count, const SDL_JoystickID *const joystick_ids,
    const int joystick_count)
{
    size_t write = 0;
    for (size_t i = 0; i < backend->open_count; i++) {
        bool keep = false;
        for (int j = 0; j < gamepad_count; j++) {
            if ((SDL_JoystickID)backend->open[i].id == gamepad_ids[j]) {
                keep = true;
                break;
            }
        }
        for (int j = 0; !keep && j < joystick_count; j++) {
            if ((SDL_JoystickID)backend->open[i].id == joystick_ids[j]) {
                keep = true;
                break;
            }
        }
        if (keep) {
            backend->open[write++] = backend->open[i];
        } else {
            M_CloseController(&backend->open[i]);
        }
    }
    backend->open_count = write;

    for (int i = 0; i < gamepad_count; i++) {
        if (M_Find(backend, (uint32_t)gamepad_ids[i]) == nullptr) {
            M_OpenGamepad(backend, gamepad_ids[i]);
        }
    }
    for (int i = 0; i < joystick_count; i++) {
        if (M_Find(backend, (uint32_t)joystick_ids[i]) == nullptr) {
            M_OpenJoystick(backend, joystick_ids[i]);
        }
    }
}

static void M_ReadGuid(const SDL_GUID guid, char *const out)
{
    SDL_GUIDToString(guid, out, 33);
}

static void M_ReadGamepad(
    const OPEN_CONTROLLER *const open, INPUT_CONTROLLER_STATE *const state)
{
    *state = (INPUT_CONTROLLER_STATE) {
        .id = open->id,
        .device = INPUT_DEVICE_GAMEPAD,
        .name = open->name,
    };
    M_ReadGuid(SDL_GetGamepadGUIDForID((SDL_JoystickID)open->id), state->guid);
    for (size_t i = 0; i < ARRAY_SIZE(m_GamepadButtonForCode); i++) {
        if (SDL_GetGamepadButton(open->gamepad, m_GamepadButtonForCode[i])) {
            M_SetBit(state->buttons, sizeof(state->buttons), (int)i);
        }
    }
    for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; axis++) {
        state->axes[axis] =
            M_NormalizeAxis(SDL_GetGamepadAxis(open->gamepad, axis));
    }
}

static void M_ReadJoystick(
    const OPEN_CONTROLLER *const open, INPUT_CONTROLLER_STATE *const state)
{
    *state = (INPUT_CONTROLLER_STATE) {
        .id = open->id,
        .device = INPUT_DEVICE_JOYSTICK,
        .name = open->name,
    };
    M_ReadGuid(SDL_GetJoystickGUID(open->joystick), state->guid);
    const int button_count = SDL_GetNumJoystickButtons(open->joystick);
    for (int i = 0; i < button_count; i++) {
        if (SDL_GetJoystickButton(open->joystick, i)) {
            M_SetBit(state->buttons, sizeof(state->buttons), i);
        }
    }
    const int axis_count = SDL_GetNumJoystickAxes(open->joystick);
    for (int i = 0; i < axis_count && (size_t)i < ARRAY_SIZE(state->axes);
         i++) {
        state->axes[i] =
            M_NormalizeAxis(SDL_GetJoystickAxis(open->joystick, i));
    }
    const int hat_count = SDL_GetNumJoystickHats(open->joystick);
    for (int i = 0; i < hat_count && (size_t)i < ARRAY_SIZE(state->hats); i++) {
        state->hats[i] = SDL_GetJoystickHat(open->joystick, i);
    }
}

INPUT_BACKEND *InputBackend_Create(const ALLOCATOR *const allocator)
{
    SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    INPUT_BACKEND *const backend =
        Allocator_AllocZ(allocator, sizeof(INPUT_BACKEND));
    backend->allocator = allocator;
    return backend;
}

void InputBackend_Destroy(INPUT_BACKEND *const backend)
{
    if (backend == nullptr) {
        return;
    }
    for (size_t i = 0; i < backend->open_count; i++) {
        M_CloseController(&backend->open[i]);
    }
    Allocator_Free(backend->allocator, backend, sizeof(INPUT_BACKEND));
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

void InputBackend_Poll(INPUT_BACKEND *const backend, INPUT_DEVICES *const out)
{
    memset(out->keys, 0, sizeof(out->keys));
    int key_count;
    const bool *const sdl_keys = SDL_GetKeyboardState(&key_count);
    for (int code = 0; code < key_count && (size_t)code < sizeof(out->keys) * 8;
         code++) {
        if (sdl_keys[code]) {
            M_SetBit(out->keys, sizeof(out->keys), code);
        }
    }

    int gamepad_count = 0;
    SDL_JoystickID *const gamepad_ids = SDL_GetGamepads(&gamepad_count);
    int all_joystick_count = 0;
    SDL_JoystickID *const all_joystick_ids =
        SDL_GetJoysticks(&all_joystick_count);
    SDL_JoystickID joystick_ids[BACKEND_MAX_CONTROLLERS];
    int joystick_count = 0;
    for (int i = 0;
         i < all_joystick_count && joystick_count < BACKEND_MAX_CONTROLLERS;
         i++) {
        if (!SDL_IsGamepad(all_joystick_ids[i])) {
            joystick_ids[joystick_count++] = all_joystick_ids[i];
        }
    }

    M_SyncControllers(
        backend, gamepad_ids, gamepad_count, joystick_ids, joystick_count);

    SDL_free(gamepad_ids);
    SDL_free(all_joystick_ids);

    for (size_t i = 0; i < backend->open_count; i++) {
        backend->states[i] = (INPUT_CONTROLLER_STATE) { 0 };
        if (backend->open[i].device == INPUT_DEVICE_GAMEPAD) {
            M_ReadGamepad(&backend->open[i], &backend->states[i]);
        } else {
            M_ReadJoystick(&backend->open[i], &backend->states[i]);
        }
    }
    out->controllers = backend->states;
    out->controller_count = backend->open_count;
}
