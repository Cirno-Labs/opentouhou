#include "th01/bomb.h"

#include "pc98/zun/math16.h"

#include "core/base/assert.h"

static RESULT M_Load(
    VFS *const vfs, const char *const name, const ALLOCATOR *const allocator,
    GRC *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Grc_Load(&reader, allocator, out);
    Reader_Close(&reader);
    return result;
}

static void M_KujiPut(
    VIDEO *const video, const GRC *const sprites, const int32_t image,
    const int16_t left, const int16_t top)
{
    if (left < -sprites->width || left >= 640 || top < -sprites->height
        || top >= 400) {
        return;
    }
    const int32_t byte_left = left >= 0 ? left / 8 : (left - 7) / 8;
    int32_t row = top * 80 + byte_left;
    const uint8_t *const coverage =
        sprites->coverage + image * sprites->width * sprites->height;
    for (int32_t y = 0; y < sprites->height; y++, row += 80) {
        if (row >= 32000) {
            break;
        }
        if (row < 0) {
            continue;
        }
        for (int32_t x = 0; x < sprites->width; x++) {
            const int32_t offset = row + x / 8;
            const bool same_row = offset / 80 == row / 80;
            if (((left >= 0 && same_row) || (left < 0 && !same_row))
                && offset < 32000 && coverage[y * sprites->width + x] != 0) {
                video->pages[video->drawing_page][offset * 8 + x % 8] = 7;
            }
        }
    }
}

static BOMB_UPDATE M_Finish(BOMB *const bomb, const BOMB_CONTEXT *const context)
{
    VIDEO *const video = context->video;
    const int16_t frame = bomb->frame;
    if (frame == 140) {
        Video_Copy(video, 1, (GFX_RECT) { 0, 0, 640, 400 }, 0, 0, 0);
        Video_SelectDrawingPage(video, 0);
        Palette_Set(context->palette, video, context->stage_palette);
        context->player->invincibility_time = 100;
        context->player->bomb_damaging = false;
        return BOMB_UPDATE_DONE;
    }
    for (int32_t angle = 0; angle < 256; angle += 16) {
        for (int16_t factor = 6; factor <= 8; factor++) {
            const int16_t radius = Math16_Wrap((frame - 50) * factor);
            bomb->left[0] = Math16_PolarX(304, radius, angle);
            bomb->top[0] = Math16_PolarY(184, radius, angle);
            Video_Invert32(video, bomb->left[0], bomb->top[0], 1);
        }
    }
    if (frame % 20 == 0) {
        for (int32_t color = 0; color < 16; color++) {
            for (int32_t component = 0; component < 3; component++) {
                if (bomb->bright.colors[color][component] < 15) {
                    bomb->bright.colors[color][component]++;
                }
            }
        }
    }
    return BOMB_UPDATE_RUNNING;
}

RESULT Bomb_ValidateAssets(const BOMB_ASSETS *const assets)
{
    for (int32_t slot = 0; slot < 2; slot++) {
        const GRC *const kuji = &assets->kuji[slot];
        FAIL_IF(
            kuji->coverage == nullptr || kuji->width != 32 || kuji->height != 32
                || kuji->count < (slot == 0 ? 8 : 1),
            "Invalid bomb kuji sprites");
    }
    return OK;
}

RESULT Bomb_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const allocator, BOMB_ASSETS *const out)
{
    *out = (BOMB_ASSETS) {};
    BOMB_ASSETS assets = {};
    MUST(M_Load(vfs, "KUZI1.GRC", allocator, &assets.kuji[0]));
    MUST(M_Load(vfs, "KUZI2.GRC", allocator, &assets.kuji[1]));
    MUST(Bomb_ValidateAssets(&assets));
    *out = assets;
    return OK;
}

BOMB_UPDATE Bomb_BeginUpdate(
    BOMB *const bomb, const BOMB_CONTEXT *const context, const int16_t frame,
    const FRAME_VSYNC *const vsync)
{
    ASSERT(!bomb->waiting);
    bomb->frame = frame;
    context->player->invincible = true;
    VIDEO *const video = context->video;
    if (frame == 0) {
        context->pellets_decay(context->user);
        Sprite_PtnPut(
            video, context->miko, 3, -1, context->player->left, PLAYER_TOP);
        bomb->white = context->palette->saved;
        for (int32_t color = 0; color < 16; color++) {
            for (int32_t component = 0; component < 3; component++) {
                const int8_t value = bomb->white.colors[color][component];
                bomb->bright.colors[color][component] =
                    (int8_t)(value > 10 ? (component == 2 ? 15 : 13)
                                        : value + component);
            }
        }
        bomb->tint_offset = -1;
    } else if (frame < 50) {
        if (frame % 2 == 0) {
            Palette_SetColor(
                context->palette, video, 7,
                bomb->white.colors[7][0] + bomb->tint_offset,
                bomb->white.colors[7][1] + bomb->tint_offset,
                bomb->white.colors[7][2]);
        }
        if (frame % 4 == 0) {
            bomb->tint_offset += frame % 80 / 40 == 0 ? -1 : 1;
        }
        if (frame % 2 == 0) {
            uint8_t angle = (uint8_t)frame;
            for (int16_t ring = 0; ring <= 9; ring += 9) {
                for (int16_t i = ring; i < ring + 9; i++) {
                    Sprite_Restore16(
                        video, bomb->left[i], bomb->top[i], 48, 32);
                    const int16_t radius =
                        Math16_Wrap(500 - ring * 10 - (frame + 60) * 4);
                    bomb->left[i] = Math16_PolarX(304, radius, angle);
                    bomb->top[i] = Math16_PolarY(184, radius, angle);
                    M_KujiPut(
                        video, &context->assets->kuji[i - ring == 8 ? 1 : 0],
                        i - ring == 8 ? 0 : i - ring, bomb->left[i],
                        bomb->top[i]);
                    angle = (uint8_t)(angle + 28);
                }
                angle = (uint8_t)-frame;
            }
        }
        if (frame % 30 == 0) {
            context->effect(context->user, 8);
        }
        if (frame % 10 == 0) {
            context->pellets_decay(context->user);
        }
    } else if (frame == 50) {
        for (int32_t i = 0; i < 18; i++) {
            Sprite_Restore16(video, bomb->left[i], bomb->top[i], 48, 32);
        }
        context->player->bomb_damaging = true;
    } else if (frame < 140) {
        if (frame % 10 == 0) {
            context->effect(context->user, 9);
        }
        if (frame % 4 == 0) {
            PALETTE peak;
            for (int32_t color = 0; color < 16; color++) {
                peak.colors[color][0] = peak.colors[color][1] =
                    (int8_t)(color == 0 || color == 7 ? 15
                                 : color == 15        ? 6
                                                      : 13);
                peak.colors[color][2] = 15;
            }
            Palette_Set(context->palette, video, &peak);
        } else if (frame % 4 == 2) {
            Palette_Set(context->palette, video, &bomb->bright);
        }
        if (frame % 4 != 0) {
            return M_Finish(bomb, context);
        }
        const int16_t offsets[4] = { 392, 400, 384, 400 };
        bomb->scroll = offsets[frame % 16 / 4];
        bomb->waiting = true;
        Frame_BeginWait(&bomb->wait, vsync);
        return BOMB_UPDATE_WAIT;
    } else if (frame == 140) {
        bomb->scroll = 0;
        bomb->waiting = true;
        Frame_BeginWait(&bomb->wait, vsync);
        return BOMB_UPDATE_WAIT;
    }
    return BOMB_UPDATE_RUNNING;
}

BOMB_UPDATE Bomb_ResumeUpdate(
    BOMB *const bomb, const BOMB_CONTEXT *const context,
    const FRAME_VSYNC *const vsync)
{
    ASSERT(bomb->waiting);
    if (!Frame_WaitReady(&bomb->wait, vsync)) {
        return BOMB_UPDATE_WAIT;
    }
    bomb->waiting = false;
    Video_SetScroll(context->video, bomb->scroll);
    return M_Finish(bomb, context);
}

bool Bomb_DealsDamage(
    const PLAYER *const player, const int16_t remaining_lives,
    const uint32_t frame)
{
    return remaining_lives == 0 && player->bomb_damaging && frame % 50 == 0;
}
