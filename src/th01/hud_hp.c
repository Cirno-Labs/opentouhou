#include "th01/hud_hp.h"

#include <string.h>

HUD_HP g_HudHp;

static void M_Snap(const int16_t point)
{
    Video_Read(
        g_Video.drawing_page, (GFX_RECT) { 128 + point * 16, 48, 16, 16 },
        g_HudHp.backgrounds + point * 256, 16);
}

static void M_Point(const int16_t point, const int16_t section_point)
{
    const int32_t left = 128 + point * 8;
    for (int32_t y = 48; y < 63; y++) {
        for (int32_t page = 1; page >= 0; page--) {
            Video_SelectDrawingPage(page);
            for (int32_t x = 0; x < 7; x++) {
                const uint8_t color = section_point < g_HudHp.first_redwhite
                    ? 10
                    : section_point >= g_HudHp.first_white ? 7
                    : x % 2 == 0                           ? 10
                                                           : 7;
                Video_Pixel(left + x, y, color);
            }
        }
    }
}

void HudHp_Init(const ALLOCATOR *const allocator)
{
    memset(&g_HudHp, 0, sizeof(g_HudHp));
    g_HudHp.backgrounds = Allocator_AllocZ(allocator, HUD_HP_CAPACITY * 256);
}

void HudHp_Reset(void)
{
    g_HudHp.first_redwhite = 0;
    g_HudHp.first_white = 0;
}

void HudHp_Rerender(const int16_t total)
{
    if (total < 0 || total > HUD_HP_CAPACITY) {
        return;
    }
    for (int16_t i = 0; i < total; i++) {
        M_Snap(i);
        M_Point(i, i);
    }
}

void HudHp_Decrement(const int16_t new_hp)
{
    if (new_hp < 0 || new_hp > HUD_HP_CAPACITY) {
        return;
    }
    const VIDEO_IMAGE image = { .pixels =
                                    g_HudHp.backgrounds + new_hp / 2 * 256,
                                .width = 16,
                                .height = 16,
                                .stride = 16,
                                .key = -1 };
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        Video_Blit(
            &image, (GFX_RECT) { 0, 0, 16, 16 }, 128 + new_hp / 2 * 16, 48);
    }
    if (new_hp % 2 != 0) {
        M_Point((int16_t)(new_hp - 1), new_hp);
    }
}

bool HudHp_Increment(const int16_t total, const int16_t current)
{
    if (total < 0 || total > HUD_HP_CAPACITY || current < 0
        || current >= HUD_HP_CAPACITY) {
        return false;
    }
    M_Snap(current);
    M_Point(current, current);
    return total - 1 == current;
}
