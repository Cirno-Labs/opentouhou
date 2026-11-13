// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/input/backend.h"

// Lets a test set the state tests/input_backend_fake.c's InputBackend_Poll
// reports for the fake backend Input_Create most recently created, instead
// of reading real devices. core/input creates exactly one backend per
// INPUT_STATE, so a test using one INPUT_STATE at a time needs no handle to it.

void FakeInput_SetKey(int code, bool down);

// Connects a controller, or does nothing when id is already connected.
void FakeInput_Connect(
    uint32_t id, INPUT_DEVICE device, const char *name, const char *guid);
void FakeInput_Disconnect(uint32_t id);

void FakeInput_SetButton(uint32_t id, int bit, bool down);
void FakeInput_SetAxis(uint32_t id, int axis, float value);
void FakeInput_SetHatBits(uint32_t id, int hat, uint8_t bits);
