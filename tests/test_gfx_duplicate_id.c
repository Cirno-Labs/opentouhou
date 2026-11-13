// SPDX-License-Identifier: GPL-3.0-only

#include "core/gfx/private.h"

#include <stdlib.h>

int main(void)
{
    if (!Result_Log(
            GfxFrontend_Start(&g_GfxBackendNull, nullptr, 8, 8),
            LOG_LEVEL_ERROR)) {
        return EXIT_FAILURE;
    }
    const GFX_STATE state = {};
    const GFX_VERTEX vertices[4] = {};
    Gfx_BeginTick((GFX_COLOR) {});
    Gfx_DrawQuad(&state, nullptr, vertices, 1);
    Gfx_DrawQuad(&state, nullptr, vertices, 1);
    Gfx_EndTick();
    return EXIT_SUCCESS;
}
