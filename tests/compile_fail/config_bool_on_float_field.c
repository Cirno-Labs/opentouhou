// SPDX-License-Identifier: GPL-3.0-only

// Compiles as it is, and fails to compile with BREAK defined, because it
// then declares a CONFIG_BOOL option over a float field.
#include "core/config.h"

typedef struct {
    float volume;
} SETTINGS;

void M_Use(void);

void M_Use(void)
{
#ifdef BREAK
    [[maybe_unused]] static const CONFIG_OPTION options[] = {
        CONFIG_BOOL(SETTINGS, "audio", "volume", volume, true),
    };
#else
    [[maybe_unused]] static const CONFIG_OPTION options[] = {
        CONFIG_FLOAT(SETTINGS, "audio", "volume", volume, 1.0f, 0.0f, 1.0f),
    };
#endif
}
