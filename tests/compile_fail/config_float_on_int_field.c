// SPDX-License-Identifier: GPL-3.0-only

// Compiles as it is, and fails to compile with BREAK defined, because it
// then declares a CONFIG_FLOAT option over an int32_t field.
#include "core/config.h"

typedef struct {
    int32_t width;
} SETTINGS;

void M_Use(void);

void M_Use(void)
{
#ifdef BREAK
    [[maybe_unused]] static const CONFIG_OPTION options[] = {
        CONFIG_FLOAT(SETTINGS, "video", "width", width, 640.0f, 0.0f, 1.0f),
    };
#else
    [[maybe_unused]] static const CONFIG_OPTION options[] = {
        CONFIG_INT(SETTINGS, "video", "width", width, 640, 320, 7680),
    };
#endif
}
