#include "th02/items.h"

#include "th02/background.h"
#include "th02/miss.h"
#include "th02/player.h"
#include "th02/shots.h"
#include "th02/sound_effects.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

static ITEMS m_Items;
const ITEMS *const g_Items = &m_Items;

static const uint8_t m_Ring[] = { 0, 1, 0, 1, 1, 0, 0, 1, 1, 0 };
static const uint8_t m_Sprites[] = { 68, 69, 70, 71, 87 };
static const int16_t m_Bonus[] = {
    1,   1,   2,   3,   4,    5,    6,    7,    8,    9,    10,
    20,  30,  40,  50,  60,   70,   80,   90,   100,  150,  200,
    250, 300, 350, 400, 450,  500,  550,  600,  650,  700,  750,
    800, 850, 900, 950, 1000, 1050, 1100, 1200, 1250, 1280,
};

static void M_PointNumber(const uint16_t value)
{
    PointNumbers_Add(
        Math16_Wrap((int32_t)m_Items.current.x - 12), m_Items.current.y, value);
}

static void M_Score(const uint16_t value)
{
    m_Items.frame_score = (int32_t)((uint32_t)m_Items.frame_score + value);
}

static uint16_t M_Bonus(void)
{
    ASSERT(
        m_Items.overflow >= 0
        && m_Items.overflow < (int16_t)ARRAY_SIZE(m_Bonus));
    return (uint16_t)m_Bonus[m_Items.overflow];
}

static bool M_Collect(ITEM *const item)
{
    const int16_t top = m_Items.current.y;
    if (top < g_Player->current.y
        || top > Math16_Wrap((int32_t)g_Player->current.y + 24)) {
        return false;
    }
    switch (item->kind) {
    case ITEM_POWER:
        if (g_Player->power < 80) {
            Player_AddPower(1);
            Shots_SetWeaponLevel(Hud_Power(g_Player->power));
            m_Items.overflow = 0;
            M_Score(1);
        } else {
            if (m_Items.overflow < 42) {
                m_Items.overflow++;
            }
            M_Score(M_Bonus());
            Performance_AddCollection(1);
        }
        Performance_AddCollection(1);
        M_PointNumber(M_Bonus());
        break;
    case ITEM_POINT: {
        m_Items.point_count = Math16_Wrap((int32_t)m_Items.point_count + 1);
        uint16_t value;
        if (top <= 64) {
            Performance_AddCollection(8);
            value = 5120;
        } else {
            value = (uint16_t)(2800 - top * 7);
            Performance_AddCollection(
                value >= 2000       ? 4
                    : value >= 1000 ? 2
                                    : 1);
        }
        M_PointNumber(value);
        M_Score(value);
        break;
    }
    case ITEM_BOMB:
        if (g_Score->bombs < 5) {
            Score_GrantBomb();
            M_Score(1000);
            M_PointNumber(1000);
            Hud_Bombs(g_Score->bombs);
        } else {
            M_Score(6553);
            M_PointNumber(6553);
            Performance_AddCollection(16);
        }
        break;
    case ITEM_BIG_POWER:
        if (g_Player->power < 70) {
            Player_AddPower(10);
            M_Score(1);
            m_Items.overflow = 0;
        } else {
            m_Items.overflow = m_Items.overflow < 38
                ? Math16_Wrap((int32_t)m_Items.overflow + 5)
                : 42;
            Player_SetPower(80);
            M_Score(M_Bonus());
        }
        M_PointNumber(M_Bonus());
        Shots_SetWeaponLevel(Hud_Power(g_Player->power));
        break;
    case ITEM_LIFE:
        if (g_Score->lives < 5) {
            Score_GrantLife();
            Hud_Lives(g_Score->lives);
            SoundEffects_Play(SOUND_EFFECT_EXTEND);
        }
        break;
    }
    item->state = ENTITY_REMOVE;
    SoundEffects_Play(SOUND_EFFECT_ITEM);
    Performance_CommitCollection();
    return true;
}

void Items_Init(void)
{
    memset(&m_Items, 0, sizeof(m_Items));
}

void Items_ResetStage(const uint8_t rank)
{
    m_Items.ring_cursor = Random_RingU8Range(0, 8);
    for (size_t i = 0; i < ITEM_COUNT; i++) {
        m_Items.slots[i].state = ENTITY_FREE;
    }
    m_Items.point_count = 0;
    PointNumbers_ResetStage(rank);
    Miss_ClearGameOver();
}

void Items_Add(const int16_t left, const int16_t top, int16_t kind)
{
    if (m_Items.big_power != 0 && kind < ITEM_BOMB) {
        m_Items.big_power--;
        kind = ITEM_BIG_POWER;
    }
    if (g_Score->lives == 5 && kind == ITEM_LIFE) {
        kind = ITEM_BOMB;
    }
    const ENTITY_POINT position = { left, Math16_Wrap((uint16_t)top << 4) };
    for (size_t i = 0; i < ITEM_COUNT; i++) {
        ITEM *const item = &m_Items.slots[i];
        if (item->state == ENTITY_FREE) {
            item->state = ENTITY_ACTIVE;
            item->kind = (uint8_t)kind;
            item->position[0] = item->position[1] = position;
            item->speed_y = -48;
            item->drift_x = item->age = 0;
            break;
        }
    }
}

void Items_AddRandom(const int16_t left, const int16_t top)
{
    if (m_Items.big_power != 0) {
        m_Items.big_power--;
        Items_Add(left, top, ITEM_BIG_POWER);
        return;
    }
    const uint8_t cycle = m_Items.cycle++;
    if (cycle % 3 != 0) {
        return;
    }
    int16_t kind;
    if (Random_RingU16Range(0, 512) == 18) {
        kind = ITEM_BOMB;
    } else {
        ASSERT(m_Items.ring_cursor < ARRAY_SIZE(m_Ring));
        kind = m_Ring[m_Items.ring_cursor];
    }
    Items_Add(left, top, kind);
    if (++m_Items.ring_cursor >= ARRAY_SIZE(m_Ring)) {
        m_Items.ring_cursor = 0;
    }
}

void Items_Drop(const int16_t left, const int16_t top)
{
    static const int8_t side_y[] = { -80, -68, -56, -44, -32 };
    static const int8_t center_x[] = { -2, -1, 0, 1, 2 };
    static const int8_t center_y[] = { -56, -68, -80, -68, -56 };
    const int zone = g_Player->current.x <= 128 ? -1
        : g_Player->current.x <= 256            ? 0
                                                : 1;
    const ENTITY_POINT position = { left, Math16_Wrap((uint16_t)top << 4) };
    size_t added = 0;
    bool big = true;
    for (size_t i = 0; i < ITEM_COUNT; i++) {
        ITEM *const item = &m_Items.slots[i];
        if (item->state != ENTITY_FREE) {
            continue;
        }
        item->state = ENTITY_ACTIVE;
        item->position[0] = item->position[1] = position;
        item->age = 0;
        uint8_t kind;
        if (g_Miss->item_gameover != 0) {
            kind = ITEM_BIG_POWER;
        } else if (big && Random_RingU8() % (5 - added) == 0) {
            kind = ITEM_BIG_POWER;
            big = false;
        } else {
            kind = Random_RingU8() % 2;
        }
        item->kind = kind;
        item->speed_y = zone != 0 ? side_y[added] : center_y[added];
        item->drift_x = zone != 0 ? zone * -2 : center_x[added];
        if (++added == 5) {
            break;
        }
    }
}

void Items_Invalidate(const PLAYFIELD_PAGES pages)
{
    ASSERT(pages.back < 2 && pages.front < 2);
    for (size_t i = 0; i < ITEM_COUNT; i++) {
        ITEM *const item = &m_Items.slots[i];
        if (item->state == ENTITY_FREE) {
            continue;
        }
        const ENTITY_POINT position = item->position[pages.back];
        Background_Mark(
            pages.scroll, position.x, Math16_ToPixel(position.y), 16, 16);
        item->position[pages.back] = item->position[pages.front];
        if (item->state == ENTITY_REMOVE) {
            item->state = ENTITY_FREE;
        }
    }
    PointNumbers_Invalidate(pages);
}

void Items_Update(
    const PLAYFIELD_PAGES pages, const uint8_t rank,
    const PATTERN_BANK *const patterns, const POINT_NUMBER_FONT *const font)
{
    ASSERT(pages.back < 2);
    m_Items.frame = Math16_Wrap((int32_t)m_Items.frame + 1);
    m_Items.frame_score = 0;
    for (size_t i = 0; i < ITEM_COUNT; i++) {
        ITEM *const item = &m_Items.slots[i];
        if (item->state != ENTITY_ACTIVE) {
            continue;
        }
        ENTITY_POINT *const position = &item->position[pages.back];
        if (item->speed_y <= 0) {
            position->x = Math16_Wrap((int32_t)position->x + item->drift_x);
        }
        position->y = Math16_Wrap((int32_t)position->y + item->speed_y);
        m_Items.current =
            (ENTITY_POINT) { position->x, Math16_ToPixel(position->y) };
        if (m_Items.current.y < 0) {
            m_Items.current.y = position->y = 0;
        } else if (m_Items.current.y >= 384) {
            item->state = ENTITY_REMOVE;
            Performance_Drop();
            continue;
        }
        if (m_Items.current.x <= 16 || m_Items.current.x >= 416) {
            item->state = ENTITY_REMOVE;
            continue;
        }
        item->age = Math16_Wrap((int32_t)item->age + 1);
        item->speed_y = Math16_Wrap((int32_t)item->speed_y + 1);
        if (g_Player->hit == 0
            && m_Items.current.x
                >= Math16_Wrap((int32_t)g_Player->current.x - 8)
            && m_Items.current.x
                <= Math16_Wrap((int32_t)g_Player->current.x + 24)
            && M_Collect(item)) {
            continue;
        }
        m_Items.current.y =
            Math16_Wrap((int32_t)m_Items.current.y + pages.scroll);
        if (m_Items.current.y >= 400) {
            m_Items.current.y -= 400;
        }
        ASSERT(item->kind < ARRAY_SIZE(m_Sprites));
        const PATTERN *const pattern =
            &patterns->entries[m_Sprites[item->kind]];
        ASSERT(pattern->data != nullptr);
        Draw_SpriteRollTiny(pattern, m_Items.current.x, m_Items.current.y);
    }
    if (m_Items.frame_score != 0) {
        ASSERT(rank <= 4);
        if (rank >= 2) {
            m_Items.frame_score =
                (int32_t)((uint32_t)m_Items.frame_score << (rank - 1));
        }
        Score_AddPoints((uint32_t)m_Items.frame_score);
    }
    PointNumbers_Update(pages, font);
}

void Items_BeginSession(const uint8_t stage)
{
    m_Items.big_power = (stage % 5) + 2;
}

void Items_ResetOverflow(void)
{
    m_Items.overflow = 10;
}
