#include "th06/item.h"

#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/bullet.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/player.h"
#include "th06/timer.h"

#include "core/base/utils.h"
#include "core/math/x87.h"

#include "stg/rng.h"

#include <stdint.h>

#define M_FIRST_SCRIPT 0x215
#define M_FULL_POWER 128
#define M_COLLECT_LINE 128
#define M_THROW_FRAMES 60
#define M_FALL_SPEED -2.2f
#define M_MAX_FALL_SPEED 3.0f
#define M_GRAVITY 0.03f
#define M_HOMING_SPEED 8.0f
#define M_MAX_BOMBS 8
#define M_MAX_LIVES 8

typedef struct {
    ANM_VM vm;
    VEC3 pos;
    // Holds the speed, except that an ITEM_THROWN item keeps where it
    // dropped here until it lands.
    VEC3 velocity;
    VEC3 target; // where an ITEM_THROWN item lands
    TIMER timer;
    int8_t type;
    bool active;
    int8_t state;
} M_ITEM;

// Holds the score of a point item on one difficulty.
typedef struct {
    int32_t high; // collected above the line
    int32_t base; // collected at the line, less step a pixel below it
    int32_t step;
} M_POINT_VALUE;

// Holds the score of a power item collected at full power, by
// power_item_combo.
static const int32_t m_PowerItemBonus[] = {
    10,   20,   30,   40,   50,   60,    70,    80,    90,    100,  200,
    300,  400,  500,  600,  700,  800,   900,   1000,  2000,  3000, 4000,
    5000, 6000, 7000, 8000, 9000, 10000, 11000, 12000, 51200,
};

static const int32_t m_PowerLevels[] = {
    8, 16, 32, 48, 64, 80, 96, 128, 999,
};

static const M_POINT_VALUE m_PointValues[] = {
    { 100000, 60000, 100 }, // easy
    { 100000, 60000, 100 }, // normal
    { 150000, 100000, 180 }, // hard
    { 200000, 150000, 270 }, // lunatic
    { 300000, 200000, 400 }, // extra
};

static M_ITEM m_Items[ITEM_COUNT];
static int32_t m_Next;

// Adds count to power_item_combo, up to the last bonus, and returns its
// bonus. The comparison is unsigned, so a negative combo also takes the
// last bonus.
static int32_t M_AddCombo(const int32_t count)
{
    g_GameManager.player.power_item_combo =
        (int8_t)(g_GameManager.player.power_item_combo + count);
    if ((uint32_t)g_GameManager.player.power_item_combo
        >= ARRAY_SIZE(m_PowerItemBonus)) {
        g_GameManager.player.power_item_combo =
            (int8_t)(ARRAY_SIZE(m_PowerItemBonus) - 1);
    }
    return m_PowerItemBonus[g_GameManager.player.power_item_combo];
}

static void M_AddPower(const VEC3 pos, const int32_t amount)
{
    int32_t level = 0;
    while (g_GameManager.player.power >= m_PowerLevels[level]) {
        level++;
    }
    const int32_t previous_level = level;
    g_GameManager.player.power =
        (uint16_t)(g_GameManager.player.power + amount);
    if (g_GameManager.player.power >= M_FULL_POWER) {
        g_GameManager.player.power = M_FULL_POWER;
        Bullet_ClearAllToItems();
        Gui_ShowFullPowerPopup();
    }
    g_GameManager.score.value += 10;
    while (g_GameManager.player.power >= m_PowerLevels[level]) {
        level++;
    }
    AsciiText_AddPopup(
        pos, level != previous_level ? -1 : 10,
        level != previous_level ? 0xff80c0ffu : 0xffffffffu);
}

static void M_CollectPower(const VEC3 pos)
{
    if (g_GameManager.player.power >= M_FULL_POWER) {
        const int32_t amount = M_AddCombo(1);
        g_GameManager.score.value += (uint32_t)amount;
        AsciiText_AddPopup(
            pos, amount, amount >= 12800 ? 0xffffff00u : 0xffffffffu);
    } else {
        g_GameManager.player.power_item_combo = 0;
        M_AddPower(pos, 1);
    }
    GameManager_AddRank(1);
}

// Leaves power_item_combo as it is below full power, and changes no rank,
// unlike a small power item.
static void M_CollectBigPower(const VEC3 pos)
{
    if (g_GameManager.player.power >= M_FULL_POWER) {
        const int32_t amount = M_AddCombo(8);
        g_GameManager.score.value += (uint32_t)amount;
        AsciiText_AddPopup(
            pos, amount, amount >= 12800 ? 0xffffff00u : 0xffffffffu);
    } else {
        M_AddPower(pos, 8);
    }
}

// Scores by the whole pixel of the height, but adds rank by the exact
// height.
static void M_CollectPoint(const VEC3 pos)
{
    const int32_t difficulty = g_GameManager.run.difficulty;
    if (difficulty >= 0 && difficulty < (int32_t)ARRAY_SIZE(m_PointValues)) {
        const M_POINT_VALUE *const value = &m_PointValues[difficulty];
        const int32_t y = X87_Ftol(pos.y);
        const int32_t amount = y < M_COLLECT_LINE
            ? value->high
            : value->base - (y - M_COLLECT_LINE) * value->step;
        AsciiText_AddPopup(
            pos, amount, amount >= value->high ? 0xffffff00u : 0xffffffffu);
        g_GameManager.score.value += (uint32_t)amount;
    }
    g_GameManager.player.point_items++;
    g_GameManager.player.point_items_total++;
    GameManager_AddRank(pos.y < M_COLLECT_LINE ? 30 : 3);
}

static void M_Collect(const M_ITEM *const item)
{
    switch (item->type) {
    case ITEM_POWER:
        M_CollectPower(item->pos);
        break;
    case ITEM_POINT:
        M_CollectPoint(item->pos);
        break;
    case ITEM_BIG_POWER:
        M_CollectBigPower(item->pos);
        break;
    case ITEM_BOMB:
        if (g_GameManager.player.bombs < M_MAX_BOMBS) {
            g_GameManager.player.bombs++;
        }
        GameManager_AddRank(5);
        break;
    case ITEM_LIFE:
        if (g_GameManager.player.lives < M_MAX_LIVES) {
            g_GameManager.player.lives++;
        }
        GameManager_AddRank(200);
        break;
    case ITEM_FULL_POWER:
        if (g_GameManager.player.power < M_FULL_POWER) {
            Bullet_ClearAllToItems();
            Gui_ShowFullPowerPopup();
            AsciiText_AddPopup(item->pos, -1, 0xff80c0ffu);
        }
        g_GameManager.player.power = M_FULL_POWER;
        g_GameManager.score.value += 1000;
        AsciiText_AddPopup(item->pos, 1000, 0xffffffffu);
        break;
    case ITEM_STAR: {
        const int32_t amount = g_Player.bomb.running
            ? 100
            : g_GameManager.player.graze / 3 * 10 + 500;
        g_GameManager.score.value += (uint32_t)amount;
        AsciiText_AddPopup2(item->pos, amount, 0xffffffffu);
    } break;
    default:
        break;
    }
}

static void M_Home(M_ITEM *const item)
{
    const float angle = Player_AngleFrom(item->pos);
    X87_SinCosScale(
        angle, M_HOMING_SPEED, &item->velocity.x, &item->velocity.y);
    item->state = ITEM_HOMING;
}

// Moves item and returns false when it fell out of the bottom of the play
// area, which frees it.
static bool M_Move(M_ITEM *const item)
{
    item->pos = Vec3_Add(item->pos, item->velocity);
    if (g_GameManager.play_area.height + 16.0f <= item->pos.y) {
        item->active = false;
        GameManager_SubRank(3);
        return false;
    }
    if (item->velocity.y < M_MAX_FALL_SPEED) {
        item->velocity.y += M_GRAVITY;
    } else {
        item->velocity.y = M_MAX_FALL_SPEED;
    }
    return true;
}

// Moves item for this frame. Returns false when it fell out.
static bool M_Update(M_ITEM *const item)
{
    // A thrown item stays thrown after it lands, so it falls and never
    // homes at full power. Only Item_CollectAll makes it home.
    if (item->state == ITEM_THROWN) {
        if (item->timer.current < M_THROW_FRAMES) {
            const float t = (float)item->timer.current / (float)M_THROW_FRAMES;
            const VEC3 from = Vec3_Scale(item->velocity, 1.0f - t);
            const VEC3 to = Vec3_Scale(item->target, t);
            item->pos = Vec3_Add(to, from);
            return true;
        }
        if (item->timer.current == M_THROW_FRAMES) {
            item->velocity = (VEC3) {};
        }
        return M_Move(item);
    }
    if (item->state == ITEM_HOMING
        || (g_GameManager.player.power >= M_FULL_POWER
            && g_Player.pos.y < M_COLLECT_LINE)) {
        M_Home(item);
    } else {
        item->velocity.x = 0.0f;
        item->velocity.z = 0.0f;
        if (item->velocity.y < M_FALL_SPEED) {
            item->velocity.y = M_FALL_SPEED;
        }
    }
    return M_Move(item);
}

void Item_Reset(void)
{
    for (int32_t i = 0; i < ITEM_COUNT; i++) {
        m_Items[i] = (M_ITEM) {};
    }
    m_Next = 0;
}

void Item_Update(void)
{
    const VEC3 size = { 16.0f, 16.0f, 16.0f };
    for (int32_t i = 0; i < ITEM_COUNT; i++) {
        M_ITEM *const item = &m_Items[i];
        if (!item->active || !M_Update(item)) {
            continue;
        }
        if (Player_TouchesGrazeBox(item->pos, size)) {
            M_Collect(item);
            item->active = false;
            continue;
        }
        Timer_Tick(&item->timer);
        Anm_Step(&item->vm);
    }
}

void Item_Spawn(const VEC3 pos, const ITEM_TYPE type, const ITEM_STATE state)
{
    M_ITEM *item = &m_Items[m_Next];
    for (int32_t tries = 0; tries < ITEM_COUNT; tries++) {
        m_Next++;
        if (item->active) {
            if (m_Next >= ITEM_COUNT) {
                m_Next = 0;
                item = m_Items;
            } else {
                item++;
            }
            continue;
        }
        if (m_Next >= ITEM_COUNT) {
            m_Next = 0;
        }
        item->active = true;
        item->pos = pos;
        item->velocity = (VEC3) { 0.0f, M_FALL_SPEED, 0.0f };
        item->type = (int8_t)type;
        item->state = (int8_t)state;
        Timer_Set(&item->timer, 0);
        if (state == ITEM_THROWN) {
            item->target.x = Rng_F32(&g_Rng) * 288.0f + 48.0f;
            item->target.y = Rng_F32(&g_Rng) * 192.0f - 64.0f;
            item->target.z = 0.0f;
            item->velocity = item->pos;
        }
        Anm_StartScript(&item->vm, (int32_t)type + M_FIRST_SCRIPT);
        item->vm.color = 0xffffffffu;
        return;
    }
}

void Item_CollectAll(void)
{
    for (int32_t i = 0; i < ITEM_COUNT; i++) {
        if (m_Items[i].active) {
            m_Items[i].state = ITEM_HOMING;
        }
    }
}
