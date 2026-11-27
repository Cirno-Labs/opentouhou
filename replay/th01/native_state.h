#pragma once

#include "replay/th01/harness.h"

#include <stdint.h>

typedef struct DRIVER DRIVER;

// Holds the native port's context: the driver of the current frame, the
// buttons that the frame applies and whether a Start has entered gameplay.
typedef struct {
    DRIVER *driver;
    uint32_t buttons;
    bool started;
} TH01_NATIVE;

// Samples the native game's state at a VSync, one value per column of
// g_Th01Harness, from context, a TH01_NATIVE. A HASH column folds one
// kind's slots as fields.h gives them.
void Th01Native_Sample(void *context, uint32_t *row);
