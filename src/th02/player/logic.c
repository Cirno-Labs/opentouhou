#include "th02/background.h"
#include "th02/bomb.h"
#include "th02/miss.h"
#include "th02/player/private.h"
#include "th02/player/render.h"
#include "th02/shots.h"
#include "th02/sound_effects.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

#define BOMB_END_INVINCIBILITY_FRAMES 70

static PLAYER m_Player;
const PLAYER *const g_Player = &m_Player;
PLAYER *const p_Player = &m_Player;

static void M_Protect(void)
{
    if (m_Player.bomb_invincible != 0
        || (m_Player.invincibility != 0 && g_Miss->active == 0)) {
        m_Player.hit = 0;
    }
}

void Player_Init(void)
{
    memset(&m_Player, 0, sizeof(m_Player));
    m_Player.power = 1;
    m_Player.option_sprite = 72;
    Miss_Init();
    Shots_Init();
    Bomb_Init();
}

void Player_ResetStage(const uint8_t type)
{
    ASSERT(type < 3);
    m_Player.shot_type = type;
    Shots_ResetStage(type);
    m_Player.invincibility = 0;
    m_Player.bomb_invincible = 0;
    Miss_ResetStage();
    m_Player.hit = 0;
    m_Player.option_sprite = 72 + type;
    static const int8_t speeds[] = { 5, 4, 3 };
    static const int8_t diagonals[] = { 4, 3, 3 };
    m_Player.speed_x = m_Player.speed_y = speeds[type];
    m_Player.diagonal_x = m_Player.diagonal_y = diagonals[type];
}

void Player_ResetPosition(void)
{
    for (size_t page = 0; page < 2; page++) {
        m_Player.position[page] = (ENTITY_POINT) { 208, 336 };
    }
}

void Player_Move(const uint8_t back, const int16_t dx, const int16_t dy)
{
    ASSERT(back < 2);
    int16_t y = Math16_Wrap((int32_t)m_Player.position[back].y + dy);
    y = y < 16 ? 16 : y > 344 ? 344 : y;
    m_Player.current.y = m_Player.position[back].y = y;
    int16_t x = Math16_Wrap((int32_t)m_Player.position[back].x + dx);
    x = x >= 390 ? 390 : x <= 28 ? 28 : x;
    m_Player.current.x = m_Player.position[back].x = x;
}

void Player_Invalidate(const PLAYFIELD_PAGES pages, const bool reduce_effects)
{
    ASSERT(pages.back < 2 && pages.front < 2);
    Shots_Invalidate(pages, reduce_effects);
    Background_Mark(
        pages.scroll, m_Player.position[pages.back].x,
        m_Player.position[pages.back].y, 32, 48);
    Background_Mark(
        pages.scroll, m_Player.option[pages.back].x,
        m_Player.option[pages.back].y, 16, 16);
    const ENTITY_POINT right = {
        Math16_Wrap((int32_t)m_Player.option[pages.back].x + 48),
        m_Player.option[pages.back].y,
    };
    Background_Mark(pages.scroll, right.x, right.y, 16, 16);
    m_Player.position[pages.back] = m_Player.position[pages.front];
}

void Player_Controls(const PLAYFIELD_PAGES pages, const uint16_t keys)
{
    const uint8_t back = pages.back;
    ASSERT(back < 2);
    M_Protect();
    if (m_Player.hit != 0) {
        return;
    }
    uint16_t direction;
    switch (keys & 0xf000) {
    case GAME_KEY_UP_LEFT:
        direction = GAME_KEY_UP | GAME_KEY_LEFT;
        break;
    case GAME_KEY_UP_RIGHT:
        direction = GAME_KEY_UP | GAME_KEY_RIGHT;
        break;
    case GAME_KEY_DOWN_LEFT:
        direction = GAME_KEY_DOWN | GAME_KEY_LEFT;
        break;
    case GAME_KEY_DOWN_RIGHT:
        direction = GAME_KEY_DOWN | GAME_KEY_RIGHT;
        break;
    default:
        direction = keys & 15;
    }
    int16_t dx = 0;
    int16_t dy = 0;
    m_Player.sprite = 0;
    switch (direction) {
    case GAME_KEY_UP:
        dy = -m_Player.speed_y;
        break;
    case GAME_KEY_DOWN:
        dy = m_Player.speed_y;
        break;
    case GAME_KEY_LEFT:
        dx = -m_Player.speed_x;
        m_Player.sprite = 2;
        break;
    case GAME_KEY_RIGHT:
        dx = m_Player.speed_x;
        m_Player.sprite = 3;
        break;
    case GAME_KEY_UP | GAME_KEY_LEFT:
    case GAME_KEY_DOWN | GAME_KEY_LEFT:
        dx = -m_Player.diagonal_x;
        dy = direction == (GAME_KEY_UP | GAME_KEY_LEFT) ? -m_Player.diagonal_y
                                                        : m_Player.diagonal_y;
        m_Player.sprite = 2;
        break;
    case GAME_KEY_UP | GAME_KEY_RIGHT:
    case GAME_KEY_DOWN | GAME_KEY_RIGHT:
        dx = m_Player.diagonal_x;
        dy = direction == (GAME_KEY_UP | GAME_KEY_RIGHT) ? -m_Player.diagonal_y
                                                         : m_Player.diagonal_y;
        m_Player.sprite = 3;
        break;
    default:
        break;
    }
    if (m_Player.speed_x == 5) {
        dx += dx < 0 ? back : dx > 0 ? -back : 0;
        dy += dy < 0 ? back : dy > 0 ? -back : 0;
    }
    Player_Move(back, dx, dy);
    m_Player.option[back].x =
        Math16_Wrap((int32_t)m_Player.current.x - 16 - dx);
    m_Player.option[back].y =
        Math16_Wrap((int32_t)m_Player.current.y + 16 - dy);
    Shots_Fire(back, keys);
    if ((keys & GAME_KEY_BOMB) != 0) {
        Bomb_Activate(pages);
    }
}

bool Player_Update(
    const PLAYFIELD_PAGES pages, const uint32_t frame,
    const bool reduce_effects, const PATTERN_BANK *const patterns,
    const uint8_t start_bombs)
{
    ASSERT(pages.back < 2);
    M_Protect();
    if (m_Player.invincibility != 0) {
        m_Player.invincibility--;
    }
    Shots_Update(pages, reduce_effects, frame, patterns);
    if (m_Player.hit != 0) {
        Miss_Update(pages, patterns, start_bombs);
        if (m_Player.hit == 255) {
            m_Player.hit = 0;
            return true;
        }
        return false;
    }
    if ((m_Player.invincibility & 3u) < 3) {
        PlayerRender_Put(
            patterns, m_Player.current, pages.scroll, (uint16_t)m_Player.sprite,
            false);
    }
    if (m_Player.power >= 8) {
        PlayerRender_Put(
            patterns, m_Player.option[pages.back], pages.scroll,
            m_Player.option_sprite, true);
        const ENTITY_POINT right = {
            Math16_Wrap((int32_t)m_Player.option[pages.back].x + 48),
            m_Player.option[pages.back].y,
        };
        PlayerRender_Put(
            patterns, right, pages.scroll, m_Player.option_sprite, true);
    }
    return false;
}

void Player_Hit(void)
{
    m_Player.hit = 1;
}

void Player_ClearHit(void)
{
    m_Player.hit = 0;
}

void Player_Kill(void)
{
    m_Player.hit = 255;
}

void Player_BeginInvincibility(const uint8_t frames)
{
    m_Player.invincibility = frames;
}

void Player_SetPower(const uint8_t power)
{
    m_Player.power = power;
}

void Player_AddPower(const int16_t delta)
{
    m_Player.power = (uint8_t)(m_Player.power + delta);
}

void Player_Respawn(void)
{
    for (size_t page = 0; page < 2; page++) {
        m_Player.position[page] = (ENTITY_POINT) { 208, 366 };
    }
}

void Player_SyncOptions(const uint8_t from, const uint8_t to)
{
    m_Player.option[to] = m_Player.option[from];
}

void Player_BeginBomb(void)
{
    m_Player.bomb_invincible = 1;
}

void Player_EndBomb(void)
{
    m_Player.bomb_invincible = 0;
    m_Player.invincibility = BOMB_END_INVINCIBILITY_FRAMES;
}

void Player_SyncPositionY(const uint8_t page)
{
    m_Player.position[page].y = m_Player.current.y;
}

void Player_PlaceOption(const uint8_t page)
{
    m_Player.option[page] = (ENTITY_POINT) {
        Math16_Wrap((int32_t)m_Player.current.x - 16),
        Math16_Wrap((int32_t)m_Player.current.y + 16),
    };
}

void Player_SyncCurrent(const uint8_t back)
{
    m_Player.current = m_Player.position[back];
}

void Player_MoveMissY(const uint8_t back, const int16_t dy)
{
    m_Player.position[back].y =
        Math16_Wrap((int32_t)m_Player.position[back].y + dy);
}
