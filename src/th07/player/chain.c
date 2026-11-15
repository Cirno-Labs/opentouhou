#include "th07/player/private.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/bomb.h"
#include "th07/buttons.h"
#include "th07/effect.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/item.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/sound.h"
#include "th07/stage.h"
#include "th07/stubs.h"

#include "core/base/utils.h"
#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/chain.h"
#include "stg/timer.h"

#include <math.h>

#define M_CALC_PRIORITY 8
#define M_DRAW_PRIORITY 6
#define M_DRAW2_PRIORITY 8
#define M_ANM_SLOT 10
#define M_FIRST_SPRITE 0x400
#define M_SCRIPT_STILL 0x400
#define M_SCRIPT_TURN_LEFT 0x401
#define M_SCRIPT_FROM_LEFT 0x402
#define M_SCRIPT_TURN_RIGHT 0x403
#define M_SCRIPT_FROM_RIGHT 0x404
#define M_SCRIPT_OPTION 0x480
#define M_FAN_ANGLE 0.22439948f
#define M_AIM_MIN -2.1991148f
#define M_AIM_MAX -0.9424778f
#define M_AIM_SNAP 0.031415928f
#define M_AIM_RETURN 0.062831856f
#define M_OPTION_SLIDE_FRAMES 8
#define M_DEATH_FRAMES 30
#define M_RESPAWN_FRAMES 30
#define M_START_FRAMES 120
#define M_INVULNERABLE_FRAMES 240
#define M_RESPAWN_CLEAR_FRAMES 60
#define M_BORDER_FRAMES 540
#define M_BORDER_FADE_FRAMES 30
#define M_BORDER_PLUS 50000
#define M_AFTER_BORDER_FRAMES 40
#define M_BOMB_FRAMES 999
#define M_BOMB_WINDOW_GAIN 6
#define M_GRAZE_MARGIN 20.0f
#define M_LASER_GRAZE_MARGIN 48.0f
#define M_SAKUYA 2
#define M_SHOT_TYPE_B 1
#define M_NO_CLEAR_ITEM 6
#define M_BREAK_CLEAR_ITEM 8
#define M_BREAK_SPARKS 0x20
#define M_BOSS_MARKERS 3 // of the four
#define M_SLOT_FOCUS 2
#define M_SLOT_SHOCKWAVE 3
#define M_SLOT_BORDER 4
#define M_ITEM_POWER 0
#define M_ITEM_BIG_POWER 2
#define M_ITEM_FULL_POWER 4
#define M_ITEM_THROWN 2
#define M_EFFECT_DEATH ((EFFECT_TYPE)(EFFECT_PARTICLE + 2))
#define M_EFFECT_GRAZE ((EFFECT_TYPE)(EFFECT_PARTICLE + 4))
#define M_COLOR_WHITE 0xffffffffu
#define M_COLOR_SHOCKWAVE 0xff4040ffu
#define M_COLOR_GRAZE_BORDER 0xffff8080u
#define M_COLOR_BLINK 0xff404040u
#define M_COLOR_BORDER_BLINK 0xffff0000u

PLAYER g_Player;

static PLAYER_ELEMS m_Elems;

static bool M_KeepsSprites(void)
{
    const SCENE scene = Scene_GetRequested();
    return scene == SCENE_NEXT_STAGE || scene == SCENE_RESTART_STAGE
        || scene == SCENE_NEXT_STAGE_AGAIN;
}

static bool M_IsSakuyaB(void)
{
    return g_GameManager.run.character == M_SAKUYA
        && g_GameManager.run.shot_type == M_SHOT_TYPE_B;
}

static bool M_IsDyingOrRespawning(void)
{
    return g_Player.state.value == PLAYER_DYING
        || g_Player.state.value == PLAYER_RESPAWNING;
}

static int32_t M_Cherry(void)
{
    return g_GameManager.cherry.value - g_GameManager.cherry.offset;
}

static void M_Box(
    const VEC3 pos, const VEC3 size, VEC3 *const min, VEC3 *const max)
{
    min->x = pos.x - size.x / 2.0f;
    min->y = pos.y - size.y / 2.0f;
    max->x = size.x / 2.0f + pos.x;
    max->y = size.y / 2.0f + pos.y;
}

static bool M_Overlap(
    const VEC3 a_min, const VEC3 a_max, const VEC3 b_min, const VEC3 b_max)
{
    return !(
        a_min.x > b_max.x || a_max.x < b_min.x || a_min.y > b_max.y
        || a_max.y < b_min.y);
}

static void M_EndEffect(EFFECT **const effect)
{
    if (*effect != nullptr) {
        (*effect)->active = false;
        *effect = nullptr;
    }
}

// Breaks the Border: the player is invulnerable for 40 frames, and a
// circle around it clears the bullets.
static void M_BreakBorder(void)
{
    PLAYER *const player = &g_Player;
    M_EndEffect(&player->effects.border);
    EFFECT *effect = Effect_SpawnReserved(
        EFFECT_BORDER, player->pos, M_SLOT_BORDER, M_COLOR_WHITE);
    ANM_VM *const vm = &effect->vm;
    Timer_Set(&vm->interp_time[ANM_INTERP_SCALE], 0);
    vm->interp_length[ANM_INTERP_SCALE] = M_BORDER_FADE_FRAMES;
    vm->interp_ease[ANM_INTERP_SCALE] = ANM_EASE_LINEAR;
    vm->scale_from = (VEC2) { 0.0625f, 0.0625f };
    vm->scale_to = (VEC2) { 1.3f, 1.3f };
    Timer_Set(&vm->interp_time[ANM_INTERP_ALPHA], 0);
    vm->interp_length[ANM_INTERP_ALPHA] = M_BORDER_FADE_FRAMES;
    vm->interp_ease[ANM_INTERP_ALPHA] = ANM_EASE_IN2;
    vm->color_from = (vm->color_from & 0xffffffu) | (vm->color & 0xff000000u);
    vm->color_to &= 0xffffffu;
    vm->int_vars[0] = M_BORDER_FADE_FRAMES;
    player->effects.border = effect;
    g_Enemies.spell_card.bonus = 0;
    g_Enemies.spell_card.capturable = false;
    player->border_state = PLAYER_BORDER_NONE;
    player->state.value = PLAYER_INVULNERABLE;
    Timer_Set(&player->state.timer, M_AFTER_BORDER_FRAMES);
    player->state.bomb_cooldown = M_AFTER_BORDER_FRAMES;
    g_GameManager.cherry.plus = g_GameManager.cherry.offset;
    Player_AddClearCircle(player->pos, 32.0f, 16.0f, 50, M_BREAK_CLEAR_ITEM);
    float angle = -MATH_PI;
    for (int32_t i = 0; i < M_BREAK_SPARKS; i++, angle += MATH_PI / 16.0f) {
        effect = Effect_Spawn(EFFECT_DIVERGE, player->pos, 1, M_COLOR_WHITE);
        effect->axis.x = X87_Cos(angle);
        effect->axis.y = X87_Sin(angle);
    }
    Sound_Queue(SOUND_SHOT_0);
    Sound_Queue(SOUND_BONUS);
    Replay_MarkEvent(REPLAY_EVENT_BORDER_BROKEN);
}

// Sets clear_item and returns true when the box touches a clear area. A
// circle tests only the centre of the box.
static bool M_TouchesClearArea(const VEC3 pos, const VEC3 size)
{
    VEC3 min;
    VEC3 max;
    M_Box(pos, size, &min, &max);
    for (int32_t i = 0; i < PLAYER_CLEAR_AREA_COUNT; i++) {
        const PLAYER_CLEAR_AREA *const area = &g_Player.clear_areas[i];
        if (area->width != 0.0f) {
            const VEC3 area_min = {
                area->x - area->width / 2.0f,
                area->y - area->height / 2.0f,
                0.0f,
            };
            const VEC3 area_max = {
                area->width / 2.0f + area->x,
                area->height / 2.0f + area->y,
                0.0f,
            };
            if (!M_Overlap(area_min, area_max, min, max)) {
                continue;
            }
        } else if (area->radius != 0.0f) {
            const float x = pos.x - area->x;
            const float y = pos.y - area->y;
            if (!(x * x + y * y < area->radius * area->radius)) {
                continue;
            }
        } else {
            continue;
        }
        g_Player.clear_item = area->item;
        return true;
    }
    return false;
}

static void M_Graze(const VEC3 pos)
{
    const bool border = g_Player.border_state == PLAYER_BORDER_RUNNING;
    const bool focused = g_Player.move.focused;
    if (!g_Player.bomb.running) {
        if (g_GameManager.player.graze < 9999) {
            g_GameManager.player.graze++;
        }
        if (g_GameManager.player.graze_total < 999999) {
            g_GameManager.player.graze_total++;
        }
    }
    const VEC3 mid = Vec3_Scale(Vec3_Add(g_Player.pos, pos), 1.0f / 2.0f);
    if (border && !focused) {
        Effect_Spawn(M_EFFECT_GRAZE, mid, 3, M_COLOR_GRAZE_BORDER);
    } else {
        Effect_Spawn(M_EFFECT_GRAZE, mid, 1, M_COLOR_WHITE);
    }
    GameManager_AddRank(6);
    Sound_Queue(SOUND_GRAZE);
    g_Enemies.spell_card.graze_bonus += 2500 + M_Cherry() / 1500 * 20;
    GameManager_AddScore(2000);
    if (border) {
        const int32_t gain = focused ? 30 : 80;
        GameManager_RaiseCherryMax(gain);
        GameManager_AddCherryQuiet(gain);
    }
}

static void M_Die(void)
{
    GameManager_DrawChecksumKeys();
    Effect_SpawnReserved(
        EFFECT_SHOCKWAVE, g_Player.pos, M_SLOT_SHOCKWAVE, M_COLOR_SHOCKWAVE);
    Effect_Spawn(M_EFFECT_DEATH, g_Player.pos, 16, M_COLOR_WHITE);
    g_Player.state.value = PLAYER_DYING;
    Timer_Set(&g_Player.state.timer, 0);
    Sound_Queue(SOUND_PLAYER_DEATH);
}

// Marks the hit, then breaks a running Border or kills a live player.
// Returns whether the player was neither in the Border nor alive.
static bool M_Hit(void)
{
    Replay_MarkEvent(REPLAY_EVENT_HIT);
    if (g_Player.state.value == PLAYER_IN_BORDER) {
        M_BreakBorder();
        return false;
    }
    if (g_Player.state.value != PLAYER_ALIVE) {
        return true;
    }
    GameManager_DrawLifeChecksumKeys();
    M_Die();
    return false;
}

static float M_SlideT(const TIMER *const timer)
{
    return Timer_AsFloat(timer) / (float)M_OPTION_SLIDE_FRAMES;
}

static void M_StartFocusAura(void)
{
    g_Player.effects.focus = Effect_SpawnReserved(
        EFFECT_PLAYER_AURA, g_Player.pos, M_SLOT_FOCUS, M_COLOR_WHITE);
}

static void M_EndFocusAura(void)
{
    if (g_Player.effects.focus != nullptr) {
        g_Player.effects.focus->vm.pending_interrupt = 1;
    }
}

// Slides the options in beside the player while focused, and out while
// not.
static void M_SlideOptions(void)
{
    TIMER *const timer = &g_Player.options.timer;
    const bool focused = g_Player.move.focused;
    float side_x = 0.0f;
    float side_y = 0.0f;
    float t;
    switch (g_Player.options.state) {
    case PLAYER_OPTIONS_NONE:
        Timer_Set(timer, 0);
        break;
    case PLAYER_OPTIONS_OUT:
        side_x = 24.0f;
        Timer_Set(timer, 0);
        if (!focused) {
            break;
        }
        g_Player.options.state = PLAYER_OPTIONS_SLIDING_IN;
        M_StartFocusAura();
        [[fallthrough]];
    case PLAYER_OPTIONS_SLIDING_IN:
    slide_in:
        Timer_TickAt(timer, g_GameSpeed);
        t = M_SlideT(timer);
        side_y = (1.0f - t) * 32.0f + -32.0f;
        t = t * t;
        side_x = -16.0f * t + 24.0f;
        if (timer->current >= M_OPTION_SLIDE_FRAMES) {
            g_Player.options.state = PLAYER_OPTIONS_IN;
        }
        if (!focused) {
            g_Player.options.state = PLAYER_OPTIONS_SLIDING_OUT;
            Timer_Set(timer, M_OPTION_SLIDE_FRAMES - timer->current);
            M_EndFocusAura();
            goto slide_out;
        }
        break;
    case PLAYER_OPTIONS_IN:
        side_x = 8.0f;
        side_y = -32.0f;
        Timer_Set(timer, 0);
        if (focused) {
            break;
        }
        g_Player.options.state = PLAYER_OPTIONS_SLIDING_OUT;
        M_EndFocusAura();
        [[fallthrough]];
    case PLAYER_OPTIONS_SLIDING_OUT:
    slide_out:
        Timer_TickAt(timer, g_GameSpeed);
        t = M_SlideT(timer);
        side_y = 32.0f * t + -32.0f;
        t = t * t;
        t = 1.0f - t;
        side_x = -16.0f * t + 24.0f;
        if (timer->current >= M_OPTION_SLIDE_FRAMES) {
            g_Player.options.state = PLAYER_OPTIONS_OUT;
        }
        if (focused) {
            g_Player.options.state = PLAYER_OPTIONS_SLIDING_IN;
            Timer_Set(timer, M_OPTION_SLIDE_FRAMES - timer->current);
            M_StartFocusAura();
            goto slide_in;
        }
        break;
    }
    g_Player.options.pos[0].x -= side_x;
    g_Player.options.pos[1].x += side_x;
    g_Player.options.pos[0].y += side_y;
    g_Player.options.pos[1].y += side_y;
}

// Returns 24 pixels along the aim angle turned by offset.
static VEC3 M_Along(const float offset)
{
    const float angle = g_Player.shots.aim_angle + offset;
    return (VEC3) { X87_Cos(angle) * 24.0f, X87_Sin(angle) * 24.0f, 0.0f };
}

// Places the options of a fan at t, from beside the player (0) to the
// spread either side of the aim angle (1).
static void M_FanAt(const float t)
{
    const VEC3 side = M_Along(MATH_HALF_PI);
    VEC3 spread = M_Along(M_FAN_ANGLE);
    spread.x = (spread.x - side.x) * t + side.x;
    spread.y = (spread.y - side.y) * t + side.y;
    g_Player.options.pos[1].x += spread.x;
    g_Player.options.pos[1].y += spread.y;
    spread = M_Along(-M_FAN_ANGLE);
    spread.x = (spread.x + side.x) * t - side.x;
    spread.y = (spread.y + side.y) * t - side.y;
    g_Player.options.pos[0].x += spread.x;
    g_Player.options.pos[0].y += spread.y;
}

// Fans Sakuya B's options out along the aim angle while focused, and back
// beside the player while not.
static void M_FanOptions(void)
{
    TIMER *const timer = &g_Player.options.timer;
    const bool focused = g_Player.move.focused;
    VEC3 offset;
    switch (g_Player.options.state) {
    case PLAYER_OPTIONS_NONE:
        Timer_Set(timer, 0);
        break;
    case PLAYER_OPTIONS_OUT:
        offset = M_Along(MATH_HALF_PI);
        Timer_Set(timer, 0);
        if (!focused) {
            g_Player.options.pos[0].x -= offset.x;
            g_Player.options.pos[1].x += offset.x;
            g_Player.options.pos[0].y -= offset.y;
            g_Player.options.pos[1].y += offset.y;
            break;
        }
        g_Player.options.state = PLAYER_OPTIONS_SLIDING_IN;
        M_StartFocusAura();
        [[fallthrough]];
    case PLAYER_OPTIONS_SLIDING_IN:
    fan_out:
        if (!focused) {
            g_Player.options.state = PLAYER_OPTIONS_SLIDING_OUT;
            Timer_Set(timer, M_OPTION_SLIDE_FRAMES - timer->current);
            M_EndFocusAura();
            goto fan_in;
        }
        Timer_TickAt(timer, g_GameSpeed);
        M_FanAt(M_SlideT(timer));
        if (timer->current >= M_OPTION_SLIDE_FRAMES) {
            g_Player.options.state = PLAYER_OPTIONS_IN;
        }
        break;
    case PLAYER_OPTIONS_IN:
        Timer_Set(timer, 0);
        if (focused) {
            offset = M_Along(M_FAN_ANGLE);
            g_Player.options.pos[1].x += offset.x;
            g_Player.options.pos[1].y += offset.y;
            offset = M_Along(-M_FAN_ANGLE);
            g_Player.options.pos[0].x += offset.x;
            g_Player.options.pos[0].y += offset.y;
            break;
        }
        g_Player.options.state = PLAYER_OPTIONS_SLIDING_OUT;
        M_EndFocusAura();
        [[fallthrough]];
    case PLAYER_OPTIONS_SLIDING_OUT:
    fan_in:
        if (focused) {
            g_Player.options.state = PLAYER_OPTIONS_SLIDING_IN;
            Timer_Set(timer, M_OPTION_SLIDE_FRAMES - timer->current);
            M_StartFocusAura();
            goto fan_out;
        }
        Timer_TickAt(timer, g_GameSpeed);
        M_FanAt(1.0f - M_SlideT(timer));
        if (timer->current >= M_OPTION_SLIDE_FRAMES) {
            g_Player.options.state = PLAYER_OPTIONS_OUT;
        }
        break;
    }
}

// Turns the aim angle against the horizontal move, within 54 degrees of
// straight up, and back up while still.
static void M_Aim(void)
{
    float *const angle = &g_Player.shots.aim_angle;
    if (g_Player.move.delta_x != 0.0f) {
        const float step =
            -(g_Player.move.delta_x / 4.0f) * MATH_PI / 5.0f / 10.0f;
        *angle -= step;
        if (*angle < M_AIM_MIN) {
            *angle = M_AIM_MIN;
        } else if (*angle > M_AIM_MAX) {
            *angle = M_AIM_MAX;
        }
    } else if (fabsf(*angle - -MATH_HALF_PI) > M_AIM_SNAP) {
        *angle += *angle < -MATH_HALF_PI ? M_AIM_RETURN * g_GameSpeed
                                         : -M_AIM_RETURN * g_GameSpeed;
    } else {
        *angle = -MATH_HALF_PI;
    }
}

// Starts the turning script on a change of the horizontal move.
static void M_Turn(const float dx)
{
    const float last_x = g_Player.move.last_x;
    if (dx < 0.0f && last_x >= 0.0f) {
        Anm_StartScript(&g_Player.vm, M_SCRIPT_TURN_LEFT);
    } else if (dx == 0.0f && last_x < 0.0f) {
        Anm_StartScript(&g_Player.vm, M_SCRIPT_FROM_LEFT);
    }
    if (dx > 0.0f && last_x <= 0.0f) {
        Anm_StartScript(&g_Player.vm, M_SCRIPT_TURN_RIGHT);
    } else if (dx == 0.0f && last_x > 0.0f) {
        Anm_StartScript(&g_Player.vm, M_SCRIPT_FROM_RIGHT);
    }
}

// Moves the player by the direction held, at the game speed, then the
// options, and fires and aims while the shot button is held. Up wins over
// down, and right over left.
static void M_HandleInput(void)
{
    PLAYER *const player = &g_Player;
    const TH07_INPUT held = Buttons_FromWord(g_Buttons.play_word);
    const bool focused = held.focus;
    player->move.focused = focused;
    const SHOT_TYPE *const shot_type = &player->shot_type;
    const float straight =
        focused ? shot_type->speed_focused : shot_type->speed;
    const float diagonal =
        focused ? shot_type->speed_diagonal_focused : shot_type->speed_diagonal;
    const float step = (held.up || held.down) && (held.left || held.right)
        ? diagonal
        : straight;
    float dx = 0.0f;
    float dy = 0.0f;
    if (held.right) {
        dx = step;
    } else if (held.left) {
        dx = -step;
    }
    if (held.up) {
        dy = -step;
    } else if (held.down) {
        dy = step;
    }
    M_Turn(dx);
    player->move.last_x = dx;
    player->move.delta_x = dx * player->move.speed_scale_x * g_GameSpeed;
    const float delta_y = dy * player->move.speed_scale_y * g_GameSpeed;
    player->pos.x += player->move.delta_x;
    player->pos.y += delta_y;

    const PLAY_RECT *const area = &g_GameManager.player_area;
    if (player->pos.x < area->x) {
        player->pos.x = area->x;
    } else if (area->x + area->width < player->pos.x) {
        player->pos.x = area->x + area->width;
    }
    if (player->pos.y < area->y) {
        player->pos.y = area->y;
    } else if (area->y + area->height < player->pos.y) {
        player->pos.y = area->y + area->height;
    }
    player->box.hitbox_min = Vec3_Sub(player->pos, player->box.hitbox_size);
    player->box.hitbox_max = Vec3_Add(player->pos, player->box.hitbox_size);
    player->box.graze_min = Vec3_Sub(player->pos, player->box.graze_size);
    player->box.graze_max = Vec3_Add(player->pos, player->box.graze_size);
    player->box.collect_min = Vec3_Sub(player->pos, player->box.collect_size);
    player->box.collect_max = Vec3_Add(player->pos, player->box.collect_size);

    player->options.pos[0] = player->pos;
    player->options.pos[1] = player->pos;
    if (M_IsSakuyaB()) {
        M_FanOptions();
    } else {
        M_SlideOptions();
    }

    if (held.shoot && !Gui_IsMsgRunning()) {
        Player_StartFiring();
        if (!focused) {
            M_Aim();
        }
    }
}

static void M_UpdateAreas(void)
{
    for (int32_t i = 0; i < PLAYER_DAMAGE_BOX_COUNT; i++) {
        g_Player.damage_boxes[i].size.x = 0.0f;
    }
    for (int32_t i = 0; i < PLAYER_CLEAR_AREA_COUNT; i++) {
        PLAYER_CLEAR_AREA *const area = &g_Player.clear_areas[i];
        if (area->frames_left <= 0) {
            area->radius = 0.0f;
            area->width = 0.0f;
        } else {
            area->frames_left--;
            area->radius += area->radius_growth;
        }
    }
}

static void M_StartBomb(void)
{
    PLAYER *const player = &g_Player;
    Replay_MarkEvent(REPLAY_EVENT_BOMB);
    GameManager_AddBombsUsed(1);
    GameManager_AddBombs(-1);
    player->bomb.focused = player->move.focused;
    player->bomb.running = true;
    player->state.bomb_started = true;
    Timer_Set(&player->bomb.timer, 0);
    player->bomb.duration = M_BOMB_FRAMES;
    Bomb_Update();
    g_Enemies.spell_card.bonus = 0;
    g_Enemies.spell_card.capturable = false;
    GameManager_SubRank(200);
    g_Enemies.spell_card.bombed = g_Enemies.spell_card.active;
    player->state.bomb_window += M_BOMB_WINDOW_GAIN;
    if (player->state.bomb_window > player->shot_type.bomb_window) {
        player->state.bomb_window = player->shot_type.bomb_window;
    }
}

// Ends the Border on the bomb button, starts a pending one, and runs or
// starts a bomb.
static void M_UpdateBomb(void)
{
    PLAYER *const player = &g_Player;
    const bool bomb_held = Buttons_FromWord(g_Buttons.play_word).bomb;
    if (player->border_state != PLAYER_BORDER_NONE && !player->bomb.running
        && bomb_held) {
        M_BreakBorder();
        player->state.bomb_started = false;
        Item_CollectAll();
        return;
    }
    if (player->border_state == PLAYER_BORDER_PENDING) {
        Player_StartBorder();
    }
    if (player->state.bomb_cooldown != 0) {
        player->state.bomb_cooldown--;
    }
    if (player->bomb.running) {
        if (Timer_HasTicked(&player->bomb.timer)) {
            GameManager_SubCherry(player->bomb.drain);
        }
        Bomb_Update();
        return;
    }
    if (!Gui_IsMsgRunning() && player->state.bomb_window != 0
        && X87_Ftol(g_GameManager.player.bombs) > 0
        && player->state.bomb_cooldown == 0 && bomb_held) {
        M_StartBomb();
    } else {
        player->state.bomb_started = false;
    }
}

static void M_DropItems(void)
{
    PLAYER *const player = &g_Player;
    const VEC3 pos = player->pos;
    if (X87_Ftol(g_GameManager.player.lives) > 0) {
        if (X87_Ftol(g_GameManager.player.power) <= 16) {
            g_GameManager.player.power = 0.0f;
            GameManager_DrawChecksumKeys();
        } else {
            GameManager_AddPower(-16);
        }
        Item_Spawn(pos, M_ITEM_BIG_POWER, M_ITEM_THROWN);
        for (int32_t i = 0; i < 5; i++) {
            Item_Spawn(pos, M_ITEM_POWER, M_ITEM_THROWN);
        }
        int32_t loss =
            X87_Ftol((float)M_Cherry() * player->shot_type.death_cherry_share);
        const int32_t cap =
            g_GameManager.run.character != M_SAKUYA ? 100000 : 60000;
        if (loss > cap) {
            loss = cap;
        }
        loss -= loss % 10;
        g_GameManager.cherry.value -= loss;
        Item_DropPulled();
    } else {
        g_GameManager.player.power = 0.0f;
        GameManager_DrawChecksumKeys();
        for (int32_t i = 0; i < 5; i++) {
            Item_Spawn(pos, M_ITEM_FULL_POWER, M_ITEM_THROWN);
        }
    }
}

static void M_UpdateRespawning(void)
{
    PLAYER *const player = &g_Player;
    const int32_t frames = player->state.timer.current;
    player->state.respawn_clear_frames = M_RESPAWN_CLEAR_FRAMES;
    const float progress =
        1.0f - Timer_AsFloat(&player->state.timer) / (float)M_RESPAWN_FRAMES;
    player->vm.scale_y = 2.0f * progress + 1.0f;
    player->vm.scale_x = 1.0f - 1.0f * progress;
    player->vm.blend_add = true;
    player->move.speed_scale_y = 1.0f;
    player->move.speed_scale_x = 1.0f;
    player->vm.color =
        (uint32_t)(frames * 255 / M_RESPAWN_FRAMES) << 24 | 0xffffffu;
    player->state.bomb_window = 0;
    if (frames >= M_RESPAWN_FRAMES) {
        player->state.value = PLAYER_INVULNERABLE;
        player->vm.scale_x = 1.0f;
        player->vm.scale_y = 1.0f;
        player->vm.color = M_COLOR_WHITE;
        player->vm.blend_add = false;
        Timer_Set(&player->state.timer, M_INVULNERABLE_FRAMES);
        player->state.bomb_window = player->shot_type.bomb_window;
    }
}

// Waits out the bomb window, which breaks a running Border, and takes the
// life when it closes, then shrinks the player away. Returns whether the
// respawn starts in the same frame: false when no life is left, which
// opens the continue menu.
static bool M_UpdateDying(void)
{
    PLAYER *const player = &g_Player;
    if (player->state.bomb_window != 0) {
        if (player->border_state == PLAYER_BORDER_RUNNING) {
            M_BreakBorder();
            return false;
        }
        player->state.bomb_window--;
        if (player->state.bomb_window == 0) {
            Replay_MarkEvent(REPLAY_EVENT_DEATH);
            g_GameManager.player.power_item_combo = 0;
            g_Enemies.spell_card.bonus = 0;
            g_Enemies.spell_card.capturable = false;
            GameManager_AddDeaths(1);
            M_DropItems();
            GameManager_SubRank(1600);
        }
        return false;
    }
    const float frames = Timer_AsFloat(&player->state.timer);
    const float progress = frames / (float)M_DEATH_FRAMES;
    player->vm.scale_y = 3.0f * progress + 1.0f;
    player->vm.scale_x = 1.0f - 1.0f * progress;
    player->vm.color =
        (uint32_t)X87_Ftol(255.0f - frames * 255.0f / (float)M_DEATH_FRAMES)
            << 24
        | 0xffffffu;
    player->vm.blend_add = true;
    player->move.last_x = 0.0f;
    if (player->state.timer.current < M_DEATH_FRAMES) {
        return false;
    }
    player->state.value = PLAYER_RESPAWNING;
    player->pos.x = g_GameManager.play_area.width / 2.0f;
    player->pos.y = g_GameManager.play_area.height - 64.0f;
    player->pos.z = 0.2f;
    Timer_Set(&player->state.timer, 0);
    player->vm.scale_x = 3.0f;
    player->vm.scale_y = 3.0f;
    Anm_StartScript(&player->vm, M_SCRIPT_STILL);
    if (X87_Ftol(g_GameManager.player.lives) <= 0) {
        g_GameManager.mode.in_continue_menu = true;
        return false;
    }
    GameManager_AddLives(-1);
    g_GameManager.player.bombs = (float)X87_Ftol(player->shot_type.bombs);
    return true;
}

// Blinks the player red and tints the stage through the Border, fading
// the tint in over its first 30 frames and out over its last 30.
static void M_ShowBorder(void)
{
    const int32_t frames = g_Player.state.timer.current;
    g_Player.vm.color = frames % 4 < 2 ? M_COLOR_BORDER_BLINK : M_COLOR_WHITE;
    uint8_t shade;
    if (frames >= M_BORDER_FRAMES - M_BORDER_FADE_FRAMES) {
        shade =
            (uint8_t)(0x80
                      - (M_BORDER_FRAMES - frames) * 80 / M_BORDER_FADE_FRAMES);
    } else if (frames < M_BORDER_FADE_FRAMES) {
        shade = (uint8_t)(0x80 - frames * 80 / M_BORDER_FADE_FRAMES);
    } else {
        shade = 0x30;
    }
    Stage_SetTint(
        0x80000000u | (uint32_t)shade << 16 | (uint32_t)shade << 8 | shade);
}

static void M_UpdateInvulnerable(void)
{
    PLAYER *const player = &g_Player;
    if (player->effects.bomb_aura != nullptr) {
        player->effects.bomb_aura->pos = player->pos;
    }
    Timer_SubAt(&player->state.timer, 1, g_GameSpeed);
    if (player->state.timer.current <= 0) {
        M_EndEffect(&player->effects.bomb_aura);
        player->state.value = PLAYER_ALIVE;
        Timer_Set(&player->state.timer, 0);
        player->vm.color = M_COLOR_WHITE;
    } else if (player->state.timer.current % 8 < 2) {
        player->vm.color = M_COLOR_BLINK;
    } else {
        player->vm.color = M_COLOR_WHITE;
    }
}

// Runs the plus down with the Border's frames left, and ends the Border
// when they run out.
static void M_UpdateInBorder(void)
{
    PLAYER *const player = &g_Player;
    if (player->effects.border != nullptr) {
        player->effects.border->pos = player->pos;
    }
    int32_t plus = player->state.timer.current * M_BORDER_PLUS
        / player->state.border_length.current;
    if (plus < 0) {
        plus = 0;
    }
    g_GameManager.cherry.plus = plus + g_GameManager.cherry.offset;
    Timer_SubAt(&player->state.timer, 1, g_GameSpeed);
    if (player->state.timer.current <= 0) {
        player->vm.color = M_COLOR_WHITE;
        Player_EndBorder();
    } else {
        M_ShowBorder();
    }
}

static void M_UpdateState(void)
{
    PLAYER *const player = &g_Player;
    if (player->state.respawn_clear_frames != 0) {
        player->state.respawn_clear_frames--;
        Bullet_ClearAll();
    }
    if (player->state.value == PLAYER_INVULNERABLE) {
        M_UpdateInvulnerable();
    } else if (player->state.value == PLAYER_IN_BORDER) {
        M_UpdateInBorder();
    } else {
        Timer_TickAt(&player->state.timer, g_GameSpeed);
    }
}

// Clears the targets for the enemy update, and hides the cherry summary
// while the player is under it, in the bottom left corner.
static void M_ResetTargets(void)
{
    PLAYER *const player = &g_Player;
    player->homing_target = (VEC3) { -999.0f, -999.0f, 0.0f };
    player->aim_target = (VEC3) { -999.0f, -999.0f, 0.0f };
    player->target_found = false;
    const int16_t interrupt = g_AsciiText.summary_interrupt;
    if (player->pos.y >= 400.0f) {
        if (interrupt != 2 && player->pos.x < 160.0f) {
            AsciiText_SetSummaryInterrupt(2);
        } else if (interrupt == 2 && player->pos.x > 160.0f) {
            AsciiText_SetSummaryInterrupt(3);
        }
    } else if (interrupt == 2) {
        AsciiText_SetSummaryInterrupt(3);
    }
}

static CHAIN_STATUS M_Update(void *const arg)
{
    PLAYER *const player = &g_Player;
    M_UpdateAreas();
    M_UpdateBomb();
    if (player->state.value == PLAYER_DYING) {
        if (M_UpdateDying()) {
            M_UpdateRespawning();
        }
    } else if (player->state.value == PLAYER_RESPAWNING) {
        M_UpdateRespawning();
    }
    M_UpdateState();
    if (!M_IsDyingOrRespawning()) {
        M_HandleInput();
    }
    Anm_Step(&player->vm);
    if (player->options.state != PLAYER_OPTIONS_NONE) {
        Anm_Step(&player->option_vms[0]);
        Anm_Step(&player->option_vms[1]);
    }
    Player_UpdateShots();
    Player_UpdateFiring();
    M_ResetTargets();
    return CHAIN_CONTINUE;
}

static ANM_DRAW M_DrawContext(void)
{
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.view =
        Anm_ScreenView(g_GameManager.camera.viewport, ANM_VIEW_PLAY_AREA);
    draw.interpolate = true;
    return draw;
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    PLAYER *const player = &g_Player;
    const ANM_DRAW draw = M_DrawContext();
    Player_DrawShots(PLAYER_BULLET_FLYING, &draw);
    if (player->bomb.running) {
        Bomb_Draw(&draw);
    }
    if (!g_GameManager.mode.in_continue_menu) {
        player->vm.pos = (VEC3) {
            g_GameManager.play_area.x + player->pos.x,
            g_GameManager.play_area.y + player->pos.y,
            0.0f,
        };
        Anm_DrawUnrotated(&player->vm, &draw);
        if (player->options.state != PLAYER_OPTIONS_NONE
            && (player->state.value == PLAYER_ALIVE
                || player->state.value == PLAYER_IN_BORDER
                || player->state.value == PLAYER_INVULNERABLE)) {
            for (int32_t i = 0; i < PLAYER_OPTION_COUNT; i++) {
                player->option_vms[i].pos = (VEC3) {
                    g_GameManager.play_area.x + player->options.pos[i].x,
                    g_GameManager.play_area.y + player->options.pos[i].y,
                    0.0f,
                };
            }
            for (int32_t i = 0; i < PLAYER_OPTION_COUNT; i++) {
                Anm_Draw(&player->option_vms[i], &draw);
            }
        }
    }
    // The draw blinks and tints too, as the original does.
    if (player->state.value == PLAYER_IN_BORDER
        && player->state.timer.current > 0) {
        M_ShowBorder();
    }
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Draw2(void *const arg)
{
    const ANM_DRAW draw = M_DrawContext();
    Player_DrawShots(PLAYER_BULLET_HIT, &draw);
    return CHAIN_CONTINUE;
}

static RESULT M_Added(void *const arg)
{
    static const char *const paths[] = {
        "data/ply00a.sht", "data/ply00b.sht", "data/ply01a.sht",
        "data/ply01b.sht", "data/ply02a.sht", "data/ply02b.sht",
    };
    static const char *const focused_paths[] = {
        "data/ply00as.sht", "data/ply00bs.sht", "data/ply01as.sht",
        "data/ply01bs.sht", "data/ply02as.sht", "data/ply02bs.sht",
    };
    static const char *const sprites[] = {
        "data/player00.anm",
        "data/player01.anm",
        "data/player02.anm",
    };
    PLAYER *const player = &g_Player;
    const uint8_t shot = g_GameManager.run.shot_index;
    if (shot >= ARRAY_SIZE(paths)) {
        return FAIL("no shot type %d", shot);
    }
    MUST(ShotType_Load(paths[shot], &player->shot_type));
    MUST(ShotType_Load(focused_paths[shot], &player->shot_type_focused));
    const uint8_t character = g_GameManager.run.character;
    if (!M_KeepsSprites() && character < ARRAY_SIZE(sprites)) {
        MUST(Anm_Load(M_ANM_SLOT, sprites[character], M_FIRST_SPRITE));
    }
    Anm_StartScript(&player->vm, M_SCRIPT_STILL);
    player->pos.x = g_GameManager.play_area.width / 2.0f;
    player->pos.y = g_GameManager.play_area.height - 64.0f;
    player->pos.z = 0.49f;
    player->options.pos[0].z = 0.49f;
    player->options.pos[1].z = 0.49f;
    for (int32_t i = 0; i < PLAYER_DAMAGE_BOX_COUNT; i++) {
        player->damage_boxes[i].size.x = 0.0f;
    }
    const float hitbox = player->shot_type.hitbox_size / 2.0f;
    player->box.hitbox_size = (VEC3) { hitbox, hitbox, 5.0f };
    const float graze = player->shot_type.graze_size / 2.0f;
    player->box.graze_size = (VEC3) { graze, graze, 5.0f };
    player->box.collect_size = (VEC3) { 12.0f, 12.0f, 5.0f };
    player->state.value = PLAYER_RESPAWNING;
    Timer_Set(&player->state.timer, M_START_FRAMES);
    player->options.state = PLAYER_OPTIONS_OUT;
    Anm_StartScript(&player->option_vms[0], M_SCRIPT_OPTION);
    Anm_StartScript(&player->option_vms[1], M_SCRIPT_OPTION + 1);
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT; i++) {
        player->shots.bullets[i].state = PLAYER_BULLET_FREE;
    }
    Timer_Set(&player->shots.fire_timer, -1);
    player->bomb.running = false;
    player->shots.aim_angle = -MATH_HALF_PI;
    player->move.speed_scale_y = 1.0f;
    player->move.speed_scale_x = 1.0f;
    player->state.bomb_window = player->shot_type.bomb_window;
    if (!M_KeepsSprites()) {
        AsciiText_SetSummaryInterrupt(1);
    }
    for (int32_t i = 0; i < M_BOSS_MARKERS; i++) {
        g_AsciiText.boss_markers[i].pending_interrupt = 2;
    }
    const int32_t full = g_GameManager.cherry.offset + M_BORDER_PLUS;
    if (g_GameManager.cherry.plus >= full) {
        g_GameManager.cherry.plus = full;
        Player_StartBorder();
    }
    return OK;
}

static void M_Deleted(void *const arg)
{
    if (!M_KeepsSprites()) {
        Anm_Release(M_ANM_SLOT);
        AsciiText_SetSummaryInterrupt(99);
        for (int32_t i = 0; i < M_BOSS_MARKERS; i++) {
            g_AsciiText.boss_markers[i].pending_interrupt = 99;
        }
    }
    ShotType_Release(&g_Player.shot_type);
    ShotType_Release(&g_Player.shot_type_focused);
}

RESULT Player_Register(void)
{
    g_Player = (PLAYER) {};
    Timer_Set(&g_Player.state.timer, 0);
    Chain_InitElem(&m_Elems.calc, M_Update, nullptr);
    m_Elems.calc.added = M_Added;
    m_Elems.calc.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &m_Elems.calc, M_CALC_PRIORITY));
    Chain_InitElem(&m_Elems.draw, M_Draw, nullptr);
    MUST(Chain_AddDraw(&g_Chain, &m_Elems.draw, M_DRAW_PRIORITY));
    Chain_InitElem(&m_Elems.draw2, M_Draw2, nullptr);
    return Chain_AddDraw(&g_Chain, &m_Elems.draw2, M_DRAW2_PRIORITY);
}

void Player_Remove(void)
{
    Chain_Remove(&m_Elems.calc);
    Chain_Remove(&m_Elems.draw);
    Chain_Remove(&m_Elems.draw2);
}

float Player_AngleFrom(const VEC3 pos)
{
    const float x = g_Player.pos.x - pos.x;
    const float y = g_Player.pos.y - pos.y;
    if (y == 0.0f && x == 0.0f) {
        return MATH_HALF_PI;
    }
    return X87_Atan2(y, x);
}

PLAYER_CONTACT Player_CheckHit(const VEC3 pos, const VEC3 size)
{
    g_Player.clear_item = M_NO_CLEAR_ITEM;
    if (M_TouchesClearArea(pos, size)) {
        return PLAYER_CONTACT_CLEARED;
    }
    VEC3 min;
    VEC3 max;
    M_Box(pos, size, &min, &max);
    if (!M_Overlap(
            g_Player.box.hitbox_min, g_Player.box.hitbox_max, min, max)) {
        return PLAYER_CONTACT_NONE;
    }
    M_Hit();
    return PLAYER_CONTACT_BOX;
}

PLAYER_CONTACT Player_CheckGraze(const VEC3 pos, const VEC3 size)
{
    g_Player.clear_item = M_NO_CLEAR_ITEM;
    if (M_TouchesClearArea(pos, size)) {
        return PLAYER_CONTACT_CLEARED;
    }
    const VEC3 min = {
        pos.x - size.x / 2.0f - M_GRAZE_MARGIN,
        pos.y - size.y / 2.0f - M_GRAZE_MARGIN,
        0.0f,
    };
    const VEC3 max = {
        size.x / 2.0f + pos.x + M_GRAZE_MARGIN,
        size.y / 2.0f + pos.y + M_GRAZE_MARGIN,
        0.0f,
    };
    if (M_IsDyingOrRespawning()
        || !M_Overlap(
            g_Player.box.graze_min, g_Player.box.graze_max, min, max)) {
        return PLAYER_CONTACT_NONE;
    }
    M_Graze(pos);
    return PLAYER_CONTACT_BOX;
}

void Player_CheckLaserHit(
    const VEC3 pos, const VEC3 size, const VEC3 origin, const float angle,
    const bool can_graze)
{
    const VEC3 offset = Vec3_Sub(g_Player.pos, origin);
    VEC3 turned = {};
    Angle_Rotate2D(&turned, &offset, angle);
    turned.z = 0.0f;
    const VEC3 center = Vec3_Add(turned, origin);
    const VEC3 hit_min = Vec3_Sub(center, g_Player.box.hitbox_size);
    const VEC3 hit_max = Vec3_Add(center, g_Player.box.hitbox_size);
    VEC3 min;
    VEC3 max;
    M_Box(pos, size, &min, &max);
    if (M_Overlap(hit_min, hit_max, min, max)) {
        M_Hit();
        return;
    }
    if (!can_graze) {
        return;
    }
    min.x -= M_LASER_GRAZE_MARGIN;
    min.y -= M_LASER_GRAZE_MARGIN;
    max.x += M_LASER_GRAZE_MARGIN;
    max.y += M_LASER_GRAZE_MARGIN;
    if (!M_Overlap(hit_min, hit_max, min, max) || M_IsDyingOrRespawning()) {
        return;
    }
    M_Graze(g_Player.pos);
}

bool Player_TouchesCollectBox(const VEC3 pos, const VEC3 size)
{
    const PLAYER_STATE state = g_Player.state.value;
    if (state != PLAYER_ALIVE && state != PLAYER_INVULNERABLE
        && state != PLAYER_IN_BORDER) {
        return false;
    }
    VEC3 min;
    VEC3 max;
    M_Box(pos, size, &min, &max);
    return M_Overlap(
        g_Player.box.collect_min, g_Player.box.collect_max, min, max);
}

void Player_StartBorder(void)
{
    PLAYER *const player = &g_Player;
    if (player->bomb.running || Gui_IsMsgRunning()) {
        player->border_state = PLAYER_BORDER_PENDING;
        return;
    }
    switch (player->state.value) {
    case PLAYER_RESPAWNING:
    case PLAYER_INVULNERABLE:
        player->border_state = PLAYER_BORDER_PENDING;
        return;
    case PLAYER_DYING:
        if (player->state.bomb_window != 0) {
            M_BreakBorder();
        } else {
            player->border_state = PLAYER_BORDER_PENDING;
        }
        return;
    default:
        break;
    }
    Timer_Set(&player->state.timer, M_BORDER_FRAMES);
    player->state.border_length = player->state.timer;
    player->border_state = PLAYER_BORDER_RUNNING;
    player->state.value = PLAYER_IN_BORDER;
    if (player->effects.border != nullptr) {
        player->effects.border->active = false;
    }
    M_EndEffect(&player->effects.bomb_aura);
    EFFECT *const effect = Effect_SpawnReserved(
        EFFECT_BORDER, player->pos, M_SLOT_BORDER, M_COLOR_WHITE);
    ANM_VM *const vm = &effect->vm;
    Timer_Set(&vm->interp_time[ANM_INTERP_SCALE], 0);
    vm->interp_length[ANM_INTERP_SCALE] = player->state.timer.current;
    vm->interp_ease[ANM_INTERP_SCALE] = ANM_EASE_LINEAR;
    vm->scale_from = (VEC2) { 1.0f, 1.0f };
    vm->scale_to = (VEC2) { 0.25f, 0.25f };
    vm->int_vars[0] = player->state.timer.current;
    vm->rotation_speed.z *= -1.0f;
    player->effects.border = effect;
    Gui_ShowBorderStartPopup();
    Sound_Queue(SOUND_BORDER);
    Sound_Queue(SOUND_BONUS_2);
    Replay_MarkEvent(REPLAY_EVENT_BORDER);
}

void Player_EndBorder(void)
{
    PLAYER *const player = &g_Player;
    GameManager_RaiseCherryMax(10000);
    GameManager_AddCherryQuiet(10000);
    const int32_t bonus = M_Cherry() * 10;
    GameManager_AddScore(bonus);
    Gui_ShowBorderBonusPopup(bonus);
    g_GameManager.cherry.plus = g_GameManager.cherry.offset;
    Sound_Queue(SOUND_BONUS);
    if (player->state.value == PLAYER_RESPAWNING) {
        player->vm.scale_x = 1.0f;
        player->vm.scale_y = 1.0f;
        player->vm.color = M_COLOR_WHITE;
        player->vm.blend_add = false;
        player->state.bomb_window = player->shot_type.bomb_window;
    }
    player->state.value = PLAYER_INVULNERABLE;
    Timer_Set(&player->state.timer, M_AFTER_BORDER_FRAMES);
    player->state.bomb_cooldown = M_AFTER_BORDER_FRAMES;
    player->border_state = PLAYER_BORDER_NONE;
    M_EndEffect(&player->effects.border);
}

// Returns the first free clear area, else the last.
static PLAYER_CLEAR_AREA *M_FreeClearArea(void)
{
    PLAYER_CLEAR_AREA *area = g_Player.clear_areas;
    for (int32_t i = 0; i < PLAYER_CLEAR_AREA_COUNT - 1; i++, area++) {
        if (area->width == 0.0f && area->radius == 0.0f) {
            break;
        }
    }
    return area;
}

PLAYER_CLEAR_AREA *Player_AddClearArea(
    const VEC3 pos, const float width, const float height, const int32_t item)
{
    PLAYER_CLEAR_AREA *const area = M_FreeClearArea();
    area->x = pos.x;
    area->y = pos.y;
    area->width = width;
    area->height = height;
    area->frames_left = 0;
    area->item = item;
    return area;
}

PLAYER_CLEAR_AREA *Player_AddClearCircle(
    const VEC3 pos, const float radius, const float growth,
    const int32_t frames, const int32_t item)
{
    PLAYER_CLEAR_AREA *const area = M_FreeClearArea();
    area->x = pos.x;
    area->y = pos.y;
    area->radius = radius;
    area->radius_growth = growth;
    area->frames_left = frames;
    area->item = item;
    return area;
}
