#include "th01/bomb.h"

#include "th01/pellets.h"
#include "th01/sound.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

typedef struct {
    PALETTE white;
    PALETTE bright;
    int16_t left[18];
    int16_t top[18];
    int16_t frame;
    int16_t scroll;
    int8_t tint_offset;
    VSYNC_WAIT wait;
    bool waiting;
} BOMB;

static BOMB m_Bomb;

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
    const GRC *const sprites, const int32_t image, const int16_t left,
    const int16_t top)
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
                g_Video->pages[g_Video->drawing_page][offset * 8 + x % 8] = 7;
            }
        }
    }
}

static BOMB_UPDATE M_Finish(void)
{
    const int16_t frame = m_Bomb.frame;
    if (frame == 140) {
        Video_Copy(1, (GFX_RECT) { 0, 0, 640, 400 }, 0, 0, 0);
        Video_SelectDrawingPage(0);
        Palette_SetGlobal(g_StagePalette);
        Player_EndBomb();
        return BOMB_UPDATE_DONE;
    }
    for (int32_t angle = 0; angle < 256; angle += 16) {
        for (int16_t factor = 6; factor <= 8; factor++) {
            const int16_t radius = Math16_Wrap((frame - 50) * factor);
            m_Bomb.left[0] = Math16_PolarX(304, radius, angle);
            m_Bomb.top[0] = Math16_PolarY(184, radius, angle);
            Video_Invert32(m_Bomb.left[0], m_Bomb.top[0], 1);
        }
    }
    if (frame % 20 == 0) {
        for (int32_t color = 0; color < 16; color++) {
            for (int32_t component = 0; component < 3; component++) {
                if (m_Bomb.bright.colors[color][component] < 15) {
                    m_Bomb.bright.colors[color][component]++;
                }
            }
        }
    }
    return BOMB_UPDATE_RUNNING;
}

void Bomb_Init(void)
{
    memset(&m_Bomb, 0, sizeof(m_Bomb));
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
    const PTN *const miko, const BOMB_ASSETS *const assets, const int16_t frame)
{
    ASSERT(!m_Bomb.waiting);
    m_Bomb.frame = frame;
    Player_BecomeInvincible();
    if (frame == 0) {
        Pellets_Decay();
        Sprite_PtnPut(miko, 3, -1, g_Player->left, PLAYER_TOP);
        m_Bomb.white = g_Palette->saved;
        for (int32_t color = 0; color < 16; color++) {
            for (int32_t component = 0; component < 3; component++) {
                const int8_t value = m_Bomb.white.colors[color][component];
                m_Bomb.bright.colors[color][component] =
                    (int8_t)(value > 10 ? (component == 2 ? 15 : 13)
                                        : value + component);
            }
        }
        m_Bomb.tint_offset = -1;
    } else if (frame < 50) {
        if (frame % 2 == 0) {
            Palette_SetGlobalColor(
                7, m_Bomb.white.colors[7][0] + m_Bomb.tint_offset,
                m_Bomb.white.colors[7][1] + m_Bomb.tint_offset,
                m_Bomb.white.colors[7][2]);
        }
        if (frame % 4 == 0) {
            m_Bomb.tint_offset += frame % 80 / 40 == 0 ? -1 : 1;
        }
        if (frame % 2 == 0) {
            uint8_t angle = (uint8_t)frame;
            for (int16_t ring = 0; ring <= 9; ring += 9) {
                for (int16_t i = ring; i < ring + 9; i++) {
                    Sprite_Restore16(m_Bomb.left[i], m_Bomb.top[i], 48, 32);
                    const int16_t radius =
                        Math16_Wrap(500 - ring * 10 - (frame + 60) * 4);
                    m_Bomb.left[i] = Math16_PolarX(304, radius, angle);
                    m_Bomb.top[i] = Math16_PolarY(184, radius, angle);
                    M_KujiPut(
                        &assets->kuji[i - ring == 8 ? 1 : 0],
                        i - ring == 8 ? 0 : i - ring, m_Bomb.left[i],
                        m_Bomb.top[i]);
                    angle = (uint8_t)(angle + 28);
                }
                angle = (uint8_t)-frame;
            }
        }
        if (frame % 30 == 0) {
            Sound_Effect(SOUND_EFFECT_TELEGRAPH);
        }
        if (frame % 10 == 0) {
            Pellets_Decay();
        }
    } else if (frame == 50) {
        for (int32_t i = 0; i < 18; i++) {
            Sprite_Restore16(m_Bomb.left[i], m_Bomb.top[i], 48, 32);
        }
        Player_StartBombDamage();
    } else if (frame < 140) {
        if (frame % 10 == 0) {
            Sound_Effect(SOUND_EFFECT_SHAKE);
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
            Palette_SetGlobal(&peak);
        } else if (frame % 4 == 2) {
            Palette_SetGlobal(&m_Bomb.bright);
        }
        if (frame % 4 != 0) {
            return M_Finish();
        }
        const int16_t offsets[4] = { 392, 400, 384, 400 };
        m_Bomb.scroll = offsets[frame % 16 / 4];
        m_Bomb.waiting = true;
        VSyncIrq_BeginWait(&m_Bomb.wait);
        return BOMB_UPDATE_WAIT;
    } else if (frame == 140) {
        m_Bomb.scroll = 0;
        m_Bomb.waiting = true;
        VSyncIrq_BeginWait(&m_Bomb.wait);
        return BOMB_UPDATE_WAIT;
    }
    return BOMB_UPDATE_RUNNING;
}

BOMB_UPDATE Bomb_ResumeUpdate(void)
{
    ASSERT(m_Bomb.waiting);
    if (!VSyncIrq_WaitReady(&m_Bomb.wait)) {
        return BOMB_UPDATE_WAIT;
    }
    m_Bomb.waiting = false;
    Video_SetScroll(m_Bomb.scroll);
    return M_Finish();
}

bool Bomb_DealsDamage(const int16_t remaining_lives, const uint32_t frame)
{
    return remaining_lives == 0 && g_Player->bomb_damaging && frame % 50 == 0;
}
