#include "th01/hud_hp.h"

#include <string.h>

#define HUD_HP_CAPACITY 32

typedef struct {
    uint8_t *backgrounds;
    uint16_t first_redwhite, first_white;
} HUD_HP;

static HUD_HP m_Health;

static void M_SnapPoint(const int16_t point)
{
    Video_Read(
        g_Video.drawing_page, (GFX_RECT) { 128 + point * 16, 48, 16, 16 },
        m_Health.backgrounds + point * 256, 16);
}

static void M_Point(const int16_t point, const int16_t section_point)
{
    const int32_t left = 128 + point * 8;
    for (int32_t y = 48; y < 63; y++) {
        for (int32_t page = 1; page >= 0; page--) {
            Video_SelectDrawingPage(page);
            for (int32_t x = 0; x < 7; x++) {
                const uint8_t color = section_point < m_Health.first_redwhite
                    ? 10
                    : section_point >= m_Health.first_white ? 7
                    : x % 2 == 0                            ? 10
                                                            : 7;
                Video_Pixel(left + x, y, color);
            }
        }
    }
}

void HudHp_Init(const ALLOCATOR *const allocator)
{
    memset(&m_Health, 0, sizeof(m_Health));
    m_Health.backgrounds = Allocator_AllocZ(allocator, HUD_HP_CAPACITY * 256);
}

void HudHp_Begin(const uint16_t first_redwhite, const uint16_t first_white)
{
    m_Health.first_redwhite = first_redwhite;
    m_Health.first_white = first_white;
}

void HudHp_Reset(void)
{
    m_Health.first_redwhite = 0;
    m_Health.first_white = 0;
}

void HudHp_Rerender(const int16_t total)
{
    if (total < 0 || total > HUD_HP_CAPACITY) {
        return;
    }
    for (int16_t i = 0; i < total; i++) {
        M_SnapPoint(i);
        M_Point(i, i);
    }
}

void HudHp_Decrement(const int16_t new_hp)
{
    if (new_hp < 0 || new_hp > HUD_HP_CAPACITY) {
        return;
    }
    const VIDEO_IMAGE image = { .pixels =
                                    m_Health.backgrounds + new_hp / 2 * 256,
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
    M_SnapPoint(current);
    M_Point(current, current);
    return total - 1 == current;
}
