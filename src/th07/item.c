#include "th07/item.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/effect.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/sound.h"
#include "th07/stubs.h"

#include "core/base/utils.h"
#include "core/math/x87.h"

#include "stg/rng.h"
#include "stg/timer.h"

#include <stdint.h>

#define M_FIRST_SCRIPT 0x2c4
#define M_FIRST_SPRITE 0x2ac
#define M_FIRST_MARKER 0x2b6
#define M_FULL_POWER 128
#define M_RANK_LINE 128.0f
#define M_THROW_FRAMES 60
#define M_FALL_SPEED -2.2f
#define M_MAX_FALL_SPEED 3.0f
#define M_GRAVITY 0.03f
#define M_RISE_SPEED -0.5f
#define M_DROP_SPEED -0.9f
#define M_MAX_BOMBS 8
#define M_MAX_LIVES 8
#define M_POINT_VALUE 50000
#define M_WHITE 0xffffffffu
#define M_YELLOW 0xffffff00u
#define M_LEVEL_UP_COLOR 0xffffc0a0u
#define M_CHERRY_COLOR 0xffff4040u

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

static ITEM m_Items[ITEM_COUNT];
static int32_t m_Next;

static int32_t M_Power(void)
{
    return X87_Ftol(g_GameManager.player.power);
}

static int32_t M_Cherry(void)
{
    return g_GameManager.cherry.value - g_GameManager.cherry.offset;
}

static bool M_IsCherryFull(void)
{
    return g_GameManager.cherry.value >= g_GameManager.cherry.max;
}

static void M_ConvertPowerItems(const int32_t except)
{
    for (int32_t i = 0; i < ITEM_COUNT; i++) {
        ITEM *const item = &m_Items[i];
        if (!item->active || i == except) {
            continue;
        }
        if (item->type == ITEM_POWER || item->type == ITEM_BIG_POWER) {
            // A thrown item loses where it dropped, so it jumps.
            if (item->velocity.y > M_RISE_SPEED) {
                item->velocity = (VEC3) { 0.0f, M_RISE_SPEED, 0.0f };
            }
            Effect_Spawn(EFFECT_EXPLOSION, item->pos, 1, M_WHITE);
            item->type = ITEM_CHERRY;
            Anm_StartScript(&item->vm, M_FIRST_SCRIPT + ITEM_CHERRY);
        }
    }
}

// Fills the power: clears the bullets outside a spell card, and turns the
// power items but the one in slot into cherry items.
static void M_FillPower(const int32_t slot)
{
    g_GameManager.player.power = (float)M_FULL_POWER;
    GameManager_DrawChecksumKeys();
    if (!g_Enemies.spell_card.active) {
        Bullet_ClearAllToItems();
    }
    Gui_ShowFullPowerPopup();
    M_ConvertPowerItems(slot);
}

static void M_AddPower(
    const ITEM *const item, const int32_t slot, const int32_t amount)
{
    int32_t level = 0;
    while (M_Power() >= m_PowerLevels[level]) {
        level++;
    }
    const int32_t previous_level = level;
    GameManager_AddPower(amount);
    if (M_Power() >= M_FULL_POWER) {
        M_FillPower(slot);
    }
    GameManager_AddScore(10);
    while (M_Power() >= m_PowerLevels[level]) {
        level++;
    }
    if (level != previous_level) {
        AsciiText_AddPopup(item->pos, -1, M_LEVEL_UP_COLOR);
        Sound_Queue(SOUND_POWER_UP);
    } else {
        AsciiText_AddPopup(item->pos, 10, M_WHITE);
    }
}

// Adds one to power_item_combo, up to the last bonus, and returns its
// bonus. The comparison is unsigned, so a negative combo also takes the
// last bonus.
static int32_t M_AddCombo(void)
{
    g_GameManager.player.power_item_combo =
        (int8_t)(g_GameManager.player.power_item_combo + 1);
    if ((uint32_t)g_GameManager.player.power_item_combo
        >= ARRAY_SIZE(m_PowerItemBonus)) {
        g_GameManager.player.power_item_combo =
            (int8_t)(ARRAY_SIZE(m_PowerItemBonus) - 1);
    }
    return m_PowerItemBonus[g_GameManager.player.power_item_combo];
}

static void M_CollectPower(
    const ITEM *const item, const int32_t slot, int32_t *const amount)
{
    if (M_Power() >= M_FULL_POWER) {
        *amount = M_AddCombo();
        GameManager_AddScore(*amount);
        AsciiText_AddPopup(
            item->pos, *amount, *amount >= 12800 ? M_YELLOW : M_WHITE);
    } else {
        g_GameManager.player.power_item_combo = 0;
        M_AddPower(item, slot, 1);
    }
    GameManager_AddRank(1);
}

// Shows amount at full power without scoring it: the original shows a
// value it never sets there.
static void M_CollectBigPower(
    const ITEM *const item, const int32_t slot, const int32_t amount)
{
    if (M_Power() >= M_FULL_POWER) {
        AsciiText_AddPopup(
            item->pos, amount, amount >= 1000 ? M_YELLOW : M_WHITE);
    } else {
        M_AddPower(item, slot, 8);
    }
}

// Returns the score of a point item at its height: full above the item
// line or when pulled in by the Border, else 100 less per whole pixel
// below the line.
static int32_t M_PointValue(const ITEM *const item)
{
    const float line = g_Player.shot_type.item_line;
    if (item->pos.y < line || item->full_value) {
        return M_POINT_VALUE;
    }
    return M_POINT_VALUE - X87_Ftol(item->pos.y - line) * 100;
}

static uint32_t M_PointColor(const ITEM *const item)
{
    return item->pos.y < g_Player.shot_type.item_line || item->full_value
        ? M_YELLOW
        : M_WHITE;
}

static int32_t M_NextExtendItems(const int32_t extends)
{
    if (g_GameManager.run.difficulty < 4) {
        if (extends < 3) {
            return extends * 75 + 50;
        }
        if (extends < 5) {
            return (extends - 3) * 150 + 300;
        }
        return (extends - 5) * 200 + 800;
    }
    if (extends == 0) {
        return 200;
    }
    if (extends == 1) {
        return 500;
    }
    return (extends - 2) * 500 + 800;
}

static void M_AddLifeOrBomb(void)
{
    if (X87_Ftol(g_GameManager.player.lives) < M_MAX_LIVES) {
        GameManager_AddLives(1);
    } else if (X87_Ftol(g_GameManager.player.bombs) < M_MAX_BOMBS) {
        GameManager_AddBombs(1);
    } else {
        return;
    }
    Sound_Queue(SOUND_EXTEND);
    GameManager_AddRank(200);
}

static void M_CollectPoint(const ITEM *const item, int32_t *const amount)
{
    *amount = M_PointValue(item);
    if (*amount >= M_POINT_VALUE) {
        if (M_Cherry() > M_POINT_VALUE) {
            *amount = M_Cherry();
        }
    } else if (M_Cherry() > M_POINT_VALUE) {
        *amount += (M_Cherry() - M_POINT_VALUE) / 5;
    }
    *amount -= *amount % 10;
    AsciiText_AddPopup(item->pos, *amount, M_PointColor(item));
    GameManager_AddScore(*amount);
    g_GameManager.player.point_items++;
    g_GameManager.player.point_items_total++;
    // Adds rank by a fixed line, unlike the score.
    GameManager_AddRank(item->pos.y < M_RANK_LINE ? 10 : 3);
    if (g_GameManager.player.extends < 0) {
        return;
    }
    // Wins every extend that the total reaches, not only the next.
    for (;;) {
        g_GameManager.player.next_extend_items =
            M_NextExtendItems(g_GameManager.player.extends);
        if (g_GameManager.player.point_items_total
            < g_GameManager.player.next_extend_items) {
            break;
        }
        M_AddLifeOrBomb();
        g_GameManager.player.extends++;
    }
}

static void M_CollectFullPower(const ITEM *const item, const int32_t slot)
{
    // Clears the bullets even during a spell card, unlike a power item.
    if (M_Power() < M_FULL_POWER) {
        Bullet_ClearAllToItems();
        Gui_ShowFullPowerPopup();
        Sound_Queue(SOUND_POWER_UP);
        AsciiText_AddPopup(item->pos, -1, M_LEVEL_UP_COLOR);
        M_ConvertPowerItems(slot);
    }
    g_GameManager.player.power = (float)M_FULL_POWER;
    GameManager_DrawChecksumKeys();
    GameManager_AddScore(1000);
    AsciiText_AddPopup(item->pos, 1000, M_WHITE);
}

static int32_t M_GrazeValue(void)
{
    const int32_t amount = g_GameManager.player.graze_total / 40 * 10 + 300;
    return amount <= 0 ? 10 : amount;
}

// Scores by the start of the bomb but adds cherry by its run, and adds the
// cherry of odd slots in a bomb without the plus.
static void M_CollectStar(
    const ITEM *const item, const int32_t slot, int32_t *const amount)
{
    *amount = g_Player.state.bomb_started ? 100 : M_GrazeValue();
    AsciiText_AddPopup2(item->pos, *amount, M_WHITE);
    GameManager_AddScore(*amount);
    if (!g_Player.bomb.running) {
        GameManager_AddCherry(20);
    } else if ((slot & 1) == 0) {
        GameManager_AddCherry(10);
    } else {
        GameManager_AddCherryNoPlus(10);
    }
}

static void M_CollectCherry(const ITEM *const item, int32_t *const amount)
{
    if (M_IsCherryFull()) {
        *amount = M_PointValue(item);
        *amount -= *amount % 10;
        AsciiText_AddPopup(item->pos, *amount, M_PointColor(item));
        GameManager_AddScore(*amount);
    }
    *amount = 1000 + g_GameManager.stats.spell_cards_captured * 100;
    if (!M_IsCherryFull()) {
        AsciiText_AddPopup(item->pos, *amount, M_CHERRY_COLOR);
    }
    GameManager_AddCherry(*amount);
}

static void M_CollectBossStar(const ITEM *const item, int32_t *const amount)
{
    *amount = M_GrazeValue();
    if (M_IsCherryFull()) {
        AsciiText_AddPopup(item->pos, *amount, M_WHITE);
    }
    GameManager_AddScore(*amount);
    *amount = 100;
    if (!M_IsCherryFull()) {
        AsciiText_AddPopup(item->pos, *amount, M_CHERRY_COLOR);
    }
    GameManager_AddCherry(*amount);
}

// Collects the item in slot. amount holds the last amount that a collect
// of this update set, as the original keeps it across items.
static void M_Collect(
    const ITEM *const item, const int32_t slot, int32_t *const amount)
{
    switch (item->type) {
    case ITEM_POWER:
        M_CollectPower(item, slot, amount);
        break;
    case ITEM_POINT:
        M_CollectPoint(item, amount);
        break;
    case ITEM_BIG_POWER:
        M_CollectBigPower(item, slot, *amount);
        break;
    case ITEM_BOMB:
        if (X87_Ftol(g_GameManager.player.bombs) < M_MAX_BOMBS) {
            GameManager_AddBombs(1);
        }
        GameManager_AddRank(5);
        break;
    case ITEM_FULL_POWER:
        M_CollectFullPower(item, slot);
        break;
    case ITEM_LIFE:
        M_AddLifeOrBomb();
        break;
    case ITEM_STAR:
        M_CollectStar(item, slot, amount);
        break;
    case ITEM_CHERRY:
        M_CollectCherry(item, amount);
        break;
    case ITEM_BORDER_STAR:
        GameManager_AddCherry(30);
        GameManager_AddCherryNoPlus(70);
        break;
    case ITEM_BOSS_STAR:
        M_CollectBossStar(item, amount);
        break;
    default:
        break;
    }
}

static bool M_IsPulled(const ITEM *const item)
{
    const bool above_line =
        (M_Power() >= M_FULL_POWER || g_GameManager.run.difficulty >= 4)
        && g_Player.pos.y < g_Player.shot_type.item_line;
    return item->state == ITEM_HOMING || above_line
        || g_Player.border_state == PLAYER_BORDER_RUNNING;
}

static void M_Pull(ITEM *const item)
{
    if (g_Player.state.value == PLAYER_RESPAWNING) {
        // Drops the item from a slow rise, at its sideways speed.
        item->velocity.y = M_RISE_SPEED;
        item->state = ITEM_FALLING;
        return;
    }
    const float angle = Player_AngleFrom(item->pos);
    X87_SinCosScale(
        angle, g_Player.shot_type.item_pull_speed, &item->velocity.x,
        &item->velocity.y);
    item->state = ITEM_HOMING;
    if (g_Player.border_state == PLAYER_BORDER_RUNNING) {
        item->full_value = true;
    }
}

// Moves item and returns false when it fell out of the bottom of the play
// area, which frees it.
static bool M_Move(ITEM *const item)
{
    item->pos = Vec3_Add(item->pos, Vec3_Scale(item->velocity, g_GameSpeed));
    if (g_GameManager.play_area.height + 16.0f <= item->pos.y) {
        item->active = false;
        GameManager_SubRank(3);
        return false;
    }
    if (item->velocity.y < M_MAX_FALL_SPEED) {
        item->velocity.y += M_GRAVITY * g_GameSpeed;
    } else {
        item->velocity.y = M_MAX_FALL_SPEED;
    }
    return true;
}

// Moves item for this frame. Returns false when it fell out.
static bool M_Update(ITEM *const item)
{
    if (item->state == ITEM_THROWN) {
        if (item->timer.current < M_THROW_FRAMES) {
            const float t = Timer_AsFloat(&item->timer) / (float)M_THROW_FRAMES;
            const VEC3 from = Vec3_Scale(item->velocity, 1.0f - t);
            const VEC3 to = Vec3_Scale(item->target, t);
            item->pos = Vec3_Add(to, from);
            return true;
        }
        // Lands as a falling item, unlike th06's, which stays thrown.
        if (item->timer.current == M_THROW_FRAMES) {
            item->velocity = (VEC3) {};
            item->state = ITEM_FALLING;
        }
        return M_Move(item);
    }
    if (M_IsPulled(item)) {
        M_Pull(item);
    } else {
        item->velocity.x = 0.0f;
        item->velocity.z = 0.0f;
        if (item->velocity.y < M_FALL_SPEED) {
            item->velocity.y = M_FALL_SPEED;
        }
    }
    return M_Move(item);
}

const ITEM *Item_GetSlots(void)
{
    return m_Items;
}

void Item_Reset(void)
{
    for (int32_t i = 0; i < ITEM_COUNT; i++) {
        m_Items[i] = (ITEM) {};
    }
    m_Next = 0;
}

void Item_Update(void)
{
    const float collect = g_Player.shot_type.item_collect_size;
    const VEC3 size = { collect, collect, 16.0f };
    int32_t amount = 0;
    bool collected = false;
    for (int32_t i = 0; i < ITEM_COUNT; i++) {
        ITEM *const item = &m_Items[i];
        item->drawn = false;
        if (!item->active || !M_Update(item)) {
            continue;
        }
        if (Player_TouchesCollectBox(item->pos, size)) {
            Replay_MarkEvent(REPLAY_EVENT_ITEM);
            M_Collect(item, i, &amount);
            item->active = false;
            collected = true;
            continue;
        }
        Timer_TickAt(&item->timer, g_GameSpeed);
        Anm_Step(&item->vm);
        item->drawn = true;
    }
    if (collected) {
        Sound_Queue(SOUND_ITEM);
    }
}

void Item_Spawn(const VEC3 pos, ITEM_TYPE type, const ITEM_STATE state)
{
    ITEM *item = &m_Items[m_Next];
    if (M_Power() >= M_FULL_POWER
        && (type == ITEM_POWER || type == ITEM_BIG_POWER)) {
        type = ITEM_CHERRY;
    }
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
        item->vm.color = M_WHITE;
        item->vm.z_write_off = true;
        item->full_value = false;
        item->on_screen = true;
        return;
    }
}

void Item_CollectAll(void)
{
    for (int32_t i = 0; i < ITEM_COUNT; i++) {
        ITEM *const item = &m_Items[i];
        if (item->active) {
            item->state = ITEM_HOMING;
            item->velocity = (VEC3) { 0.0f, M_RISE_SPEED, 0.0f };
        }
    }
}

void Item_DropPulled(void)
{
    for (int32_t i = 0; i < ITEM_COUNT; i++) {
        ITEM *const item = &m_Items[i];
        if (item->active && item->state == ITEM_HOMING) {
            item->state = ITEM_FALLING;
            item->velocity = (VEC3) { 0.0f, M_DROP_SPEED, 0.0f };
        }
    }
}

void Item_Draw(void)
{
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.interpolate = true;
    for (int32_t i = 0; i < ITEM_COUNT; i++) {
        ITEM *const item = &m_Items[i];
        if (!item->drawn) {
            continue;
        }
        item->vm.pos =
            (VEC3) { g_GameManager.play_area.x + item->pos.x,
                     g_GameManager.play_area.y + item->pos.y, 0.01f };
        if (item->pos.y < -8.0f) {
            item->vm.pos.y = 8.0f + g_GameManager.play_area.y;
            if (item->on_screen) {
                Anm_SetSprite(&item->vm, item->type + M_FIRST_MARKER);
                item->on_screen = false;
                item->vm.z_write_off = true;
            }
            int32_t alpha =
                255 - X87_Ftol((8.0f - item->pos.y) * 255.0f / 128.0f);
            if (alpha < 64) {
                alpha = 64;
            }
            item->vm.color =
                (item->vm.color & 0xffffffu) | (uint32_t)alpha << 24;
        } else if (!item->on_screen) {
            Anm_SetSprite(&item->vm, item->type + M_FIRST_SPRITE);
            item->on_screen = true;
            item->vm.color = M_WHITE;
            item->vm.z_write_off = true;
        }
        Anm_Draw(&item->vm, &draw);
    }
}
