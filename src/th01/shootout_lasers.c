#include "th01/shootout_lasers.h"

#include "pc98/zun/math16.h"
#include "pc98/zun/sprite_sheet.h"

#include "core/base/assert.h"

#include <string.h>

typedef struct {
    int32_t origin_left, origin_y, start_left, start_y;
    int32_t cursor_left, cursor_y, step_x, step_y;
    int16_t length, tail_speed, extend_speed, age, detach_age;
    uint8_t color, width_mask;
    bool alive, damaging;
} SHOOTOUT_LASER;

typedef struct {
    SHOOTOUT_LASER slots[SHOOTOUT_LASERS_COUNT];
} SHOOTOUT_LASERS;

static SHOOTOUT_LASERS m_Lasers;

static int32_t M_Floor(const int32_t value, const int32_t divisor)
{
    return value / divisor - (value % divisor < 0);
}

static int32_t M_AddSubpixels(const int32_t value, const int32_t step)
{
    const uint32_t sum = (uint32_t)value + (uint32_t)step;
    return (int32_t)(sum >= 0x80000000u ? (int64_t)sum - 0x100000000ll : sum);
}

static void M_PutDots(
    const int16_t left, const int16_t y, const uint16_t dots,
    const uint8_t color)
{
    const uint16_t offset = (uint16_t)(M_Floor(left, 8) + y * 80);
    for (int32_t byte = 0; byte < 2; byte++) {
        const uint32_t address = ((uint32_t)offset + byte) & 0x7fffu;
        if (address >= 32000) {
            continue;
        }
        for (int32_t bit = 0; bit < 8; bit++) {
            if ((dots & (0x80u << (byte * 8) >> bit)) != 0) {
                g_Video.pages[g_Video.drawing_page][address * 8 + bit] =
                    color & 15;
            }
        }
    }
}

static void M_TraceBeam(
    SHOOTOUT_LASER *const laser, const SHOOTOUT_LASERS_ASSETS *const assets,
    const bool put)
{
    int16_t left = Math16_Wrap(M_Floor(laser->cursor_left, 256));
    int16_t y = Math16_Wrap(M_Floor(laser->cursor_y, 256));
    uint16_t dots = 0;
    const uint16_t count = (uint16_t)(put ? laser->length : laser->tail_speed);
    for (uint32_t i = 0; i < count; i++) {
        if (M_Floor(left, 8) != M_Floor(laser->cursor_left, 2048)
            || y != M_Floor(laser->cursor_y, 256)) {
            if (put) {
                M_PutDots(left, y, dots, laser->color);
                dots = 0;
                if (!g_Player.invincible && laser->damaging && y > 376
                    && left >= g_Player.left + 4 && left < g_Player.left + 24) {
                    Player_Hit();
                }
            }
            left = Math16_Wrap(M_Floor(laser->cursor_left, 256));
            y = Math16_Wrap(M_Floor(laser->cursor_y, 256));
            if (left > 638 || left < 0 || y < 64 || y > 399) {
                break;
            }
        }
        const uint8_t *const mask =
            assets->masks[laser->width_mask]
                         [(uint32_t)M_Floor(laser->cursor_left, 256) & 7];
        dots |= (uint16_t)(mask[0] | ((uint16_t)mask[1] << 8));
        if (!put) {
            Sprite_Restore16(
                Math16_Wrap(M_Floor(laser->cursor_left, 256)),
                Math16_Wrap(M_Floor(laser->cursor_y, 256)), 16, 1);
        }
        laser->cursor_left = M_AddSubpixels(laser->cursor_left, laser->step_x);
        laser->cursor_y = M_AddSubpixels(laser->cursor_y, laser->step_y);
    }
}

void ShootoutLasers_Init(void)
{
    memset(&m_Lasers, 0, sizeof(m_Lasers));
}

RESULT ShootoutLasers_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const allocator,
    SHOOTOUT_LASERS_ASSETS **const out)
{
    *out = nullptr;
    uint8_t *masks;
    size_t size;
    MUST(SpriteSheet_Load(vfs, "laser_s.bmp", 16, 8, allocator, &masks, &size));
    FAIL_IF(size != sizeof(SHOOTOUT_LASERS_ASSETS), "Invalid laser_s.bmp size");
    *out = (SHOOTOUT_LASERS_ASSETS *)masks;
    return OK;
}

void ShootoutLasers_Spawn(
    const int32_t slot, const int16_t origin_left, const int16_t origin_y,
    const int16_t target_left, const int16_t target_y,
    const int16_t speed_times_eight, const uint8_t color,
    const int16_t detach_age, const int16_t width)
{
    ASSERT(slot >= 0 && slot < SHOOTOUT_LASERS_COUNT);
    SHOOTOUT_LASER *const laser = &m_Lasers.slots[slot];
    if (laser->alive) {
        return;
    }
    ASSERT(width >= 1 && width <= 8);
    laser->origin_left = (int32_t)origin_left * 256;
    laser->origin_y = (int32_t)origin_y * 256;
    laser->start_left = laser->origin_left;
    laser->start_y = laser->origin_y;
    const uint8_t angle = Math16_Angle(
        Math16_Wrap(target_y - origin_y),
        Math16_Wrap(target_left - origin_left));
    laser->step_x = Math16_Cos(angle);
    laser->step_y = Math16_Sin(angle);
    laser->extend_speed = speed_times_eight / 8;
    laser->width_mask = (uint8_t)(width - 1);
    laser->alive = true;
    laser->damaging = true;
    laser->detach_age = detach_age;
    laser->age = 0;
    laser->color = color;
    laser->tail_speed = laser->extend_speed;
    laser->length = 0;
}

void ShootoutLasers_Update(
    const int32_t slot, const SHOOTOUT_LASERS_ASSETS *const assets)
{
    ASSERT(slot >= 0 && slot < SHOOTOUT_LASERS_COUNT);
    SHOOTOUT_LASER *const laser = &m_Lasers.slots[slot];
    if (!laser->alive) {
        return;
    }
    if (laser->age >= laser->detach_age) {
        laser->cursor_left = laser->start_left;
        laser->cursor_y = laser->start_y;
        M_TraceBeam(laser, assets, false);
        laser->start_left = laser->cursor_left;
        laser->start_y = laser->cursor_y;
        if (laser->start_left >= 640 * 256 || laser->start_left < 0
            || laser->start_y < 64 * 256 || laser->start_y >= 400 * 256) {
            laser->alive = false;
            return;
        }
    } else {
        laser->length = Math16_Wrap(laser->length + laser->extend_speed);
    }
    laser->age = Math16_Wrap(laser->age + 1);
    laser->cursor_left = laser->start_left;
    laser->cursor_y = laser->start_y;
    M_TraceBeam(laser, assets, true);
}

void ShootoutLasers_UpdateAll(const SHOOTOUT_LASERS_ASSETS *const assets)
{
    for (int32_t i = 0; i < SHOOTOUT_LASERS_COUNT; i++) {
        ShootoutLasers_Update(i, assets);
    }
}

void ShootoutLasers_EraseAndReset(const int32_t count)
{
    ASSERT(count >= 0 && count <= SHOOTOUT_LASERS_COUNT);
    for (int32_t i = 0; i < count; i++) {
        SHOOTOUT_LASER *const laser = &m_Lasers.slots[i];
        if (laser->alive) {
            Video_LineErase(
                Math16_Wrap(laser->start_left), Math16_Wrap(laser->start_y),
                Math16_Wrap(laser->origin_left), Math16_Wrap(laser->origin_y));
            laser->alive = false;
        }
    }
}
