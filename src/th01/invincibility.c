#include "th01/invincibility.h"

#include "pc98/zun/master_rng.h"
#include "th01/sprite.h"

RESULT Invincibility_LoadAssets(
    READER *const reader, uint8_t *const scratch, const size_t scratch_size,
    INVINCIBILITY_ASSETS *const out)
{
    *out = (INVINCIBILITY_ASSETS) {};
    READER image;
    MUST(ReiidenImage_Open(reader, scratch, scratch_size, &image));
    Reader_Seek(&image, 0x39262);
    INVINCIBILITY_ASSETS assets;
    Reader_ReadBytes(&image, assets.masks, sizeof(assets.masks));
    MUST(Reader_Result(&image));
    *out = assets;
    return OK;
}

void Invincibility_Put(
    VIDEO *const video, const INVINCIBILITY_ASSETS *const assets,
    const int16_t left, const int16_t top, const int16_t image)
{
    if (left < 0 || image < 0 || image > 3) {
        return;
    }
    Video_SelectDrawingPage(video, 0);
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
                video->pages[0][at] = 10;
            }
        }
        offset = (int16_t)(offset + 80);
        if (offset > 32000) {
            break;
        }
    }
}

void Invincibility_UpdateAndRender(
    INVINCIBILITY *const state, VIDEO *const video,
    const INVINCIBILITY_ASSETS *const assets, const PLAYER *const player,
    uint32_t *const rng)
{
    if (!player->invincible) {
        if (state->frame[0] == 255) {
            return;
        }
        for (int32_t i = 0; i < 8; i++) {
            Sprite_Restore16(video, state->left[i], state->top[i], 16, 8);
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
                Sprite_Restore16(video, state->left[i], state->top[i], 16, 8);
            }
            state->left[i] =
                (int16_t)(MasterRng_Next(rng) % 48 + player->left - 8);
            state->top[i] =
                (int16_t)(MasterRng_Next(rng) % 48 + PLAYER_TOP - 16);
            state->frame[i] = (int16_t)(MasterRng_Next(rng) % 7 + 1);
        }
        if (state->top[i] >= 0 && state->top[i] < 632) {
            Invincibility_Put(
                video, assets, state->left[i], state->top[i],
                (int16_t)(state->frame[i] <= 4 ? state->frame[i] - 1
                                               : 8 - state->frame[i]));
        }
        state->frame[i]++;
    }
}
