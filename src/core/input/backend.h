// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/input.h"

// Reads the devices of one platform. input/backend_sdl.c implements it for
// SDL, and tests/input_backend_fake.c for the tests. Meson picks the
// platform's file. Codes do not depend on the platform: keys are USB HID
// usage ids, gamepad buttons and axes are positions in the standard
// layout, and joystick codes are indices.

typedef struct {
    uint32_t id;
    INPUT_DEVICE device;
    const char *name;
    // Identifies the controller across runs, as 32 hex digits.
    char guid[33];
    // Holds a bit per button.
    uint8_t buttons[32];
    // Holds each axis from -1 to 1.
    float axes[8];
    // Holds the up, right, down and left bits of each hat.
    uint8_t hats[4];
} INPUT_CONTROLLER_STATE;

typedef struct {
    // Holds a bit per key.
    uint8_t keys[32];
    const INPUT_CONTROLLER_STATE *controllers;
    size_t controller_count;
} INPUT_DEVICES;

typedef struct INPUT_BACKEND INPUT_BACKEND;

INPUT_BACKEND *InputBackend_Create(const ALLOCATOR *allocator);
void InputBackend_Destroy(INPUT_BACKEND *backend);

// Reads the current state of the keyboard and of every connected
// controller, and opens and drops controllers as they connect and
// disconnect. The pointers in out stay valid until the next call.
void InputBackend_Poll(INPUT_BACKEND *backend, INPUT_DEVICES *out);
