#include "th01/invincibility.h"

#include "th01/rng.h"
#include "th01/sprite.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

RESULT Invincibility_LoadAssets(VFS *const vfs, INVINCIBILITY_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    uint8_t *shapes;
    size_t size;
    MUST(SpriteSheet_Load(
        vfs, "shape8x8.bmp", 8, 8, &g_HeapAllocator, &shapes, &size));
    if (size != 7 * 8) {
        Mem_Free(shapes);
        return FAIL("Invalid shape8x8.bmp size");
    }
    memcpy(out->masks, shapes + 3 * 8, sizeof(out->masks));
    Mem_Free(shapes);
    return OK;
}

void Invincibility_Put(
    const INVINCIBILITY_ASSETS *const assets, const int16_t left,
    const int16_t top, const int16_t image)
{
    if (left < 0 || image < 0 || image > 3) {
        return;
    }
    Video_SelectDrawingPage(0);
    int16_t offset = (int16_t)(top * 80 + left / 8);
    const int32_t first = left % 8;
    for (int32_t y = 0; y < 8; y++) {
        const uint8_t bits = assets->masks[image][y];
        for (int32_t x = 0; x < 8; x++) {
            if ((bits & (0x80 >> x)) == 0) {
                continue;
            }
            const int32_t at =
                ((uint16_t)(offset + (first + x) / 8) & 32767) * 8
                + (first + x) % 8;
            if (at < VIDEO_PAGE_SIZE) {
                g_Video->pages[0][at] = 10;
            }
        }
        offset = (int16_t)(offset + 80);
        if (offset > 32000) {
            break;
        }
    }
}

void Invincibility_UpdateAndRender(
    INVINCIBILITY *const state, const INVINCIBILITY_ASSETS *const assets)
{
    if (!g_Player->invincible) {
        if (state->frame[0] == 255) {
            return;
        }
        for (int32_t i = 0; i < 8; i++) {
            Sprite_Restore16(state->left[i], state->top[i], 16, 8);
        }
        state->frame[0] = 255;
        return;
    }
    if (state->frame[0] == 255) {
        for (int32_t i = 0; i < 8; i++) {
            state->frame[i] = 0;
        }
    }
    for (int32_t i = 0; i < 8; i++) {
        if (state->frame[i] == 9 || state->frame[i] == 0) {
            if (state->frame[i] == 9) {
                Sprite_Restore16(state->left[i], state->top[i], 16, 8);
            }
            state->left[i] =
                (int16_t)(MasterRng_Next(&g_Rng) % 48 + g_Player->left - 8);
            state->top[i] =
                (int16_t)(MasterRng_Next(&g_Rng) % 48 + PLAYER_TOP - 16);
            state->frame[i] = (int16_t)(MasterRng_Next(&g_Rng) % 7 + 1);
        }
        if (state->top[i] >= 0 && state->top[i] < 632) {
            Invincibility_Put(
                assets, state->left[i], state->top[i],
                (int16_t)(state->frame[i] <= 4 ? state->frame[i] - 1
                                               : 8 - state->frame[i]));
        }
        state->frame[i]++;
    }
}
