#include "th06/bomb.h"

#include "th06/anm.h"
#include "th06/effect.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/item.h"
#include "th06/player.h"
#include "th06/screen_effect.h"
#include "th06/timer.h"

#include "core/math/x87.h"

#include "stg/rng.h"

#define M_PI 3.1415927f
#define M_TWO_PI 6.2831855f
#define M_NO_TARGET -100.0f
#define M_ORB_SPRITES 4
#define M_ORB_FIRST_SCRIPT 0x485
#define M_ORB_FIRST_FRAME 60
#define M_ORB_LAST_FRAME 180
#define M_ORB_INTERVAL 16
#define M_ORB_BURST_DAMAGE 100
#define M_ORB_BURST_FRAMES 30
#define M_ORB_MAX_SPEED 10.0f
#define M_PILLAR_COUNT 4
#define M_PILLAR_FIRST_SCRIPT 0x489
#define M_STAR_COUNT 8
#define M_STAR_FIRST_SCRIPT 0x405
#define M_BEAM_SPRITES 4
#define M_BEAM_FIRST_SCRIPT 0x408
#define M_BEAM_SPEED_SCALE 0.3f
#define M_SHAKE_WARMUP_FRAMES 60
#define M_COLOR_WHITE 0xffffffffu
#define M_COLOR_SHOCKWAVE 0xff4040ffu

// Holds the widths and heights of the pillars of Reimu B.
static const VEC2 m_PillarSizes[M_PILLAR_COUNT] = {
    { 62.0f, 448.0f },
    { 384.0f, 62.0f },
    { 62.0f, 448.0f },
    { 384.0f, 62.0f },
};

static TIMER *M_Timer(void)
{
    return &g_Player.bomb.timer;
}

// Ends the bomb once its duration is over, and returns whether it did.
static bool M_Ends(void)
{
    if (M_Timer()->current < g_Player.bomb.duration) {
        return false;
    }
    Gui_EndBombName();
    g_Player.bomb.running = false;
    return true;
}

static bool M_Starts(void)
{
    return Timer_HasTicked(M_Timer()) && M_Timer()->current == 0;
}

// Returns whether the bomb sets its boxes this frame: on the frames that
// are not a multiple of every.
static bool M_SetsBoxes(const int32_t every)
{
    return Timer_HasTicked(M_Timer()) && M_Timer()->current % every != 0;
}

static void M_Finish(void)
{
    g_Player.state.value = PLAYER_INVULNERABLE;
    Timer_Tick(M_Timer());
}

static void M_LaunchOrb(const int32_t i)
{
    PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[i];
    orb->state = 1;
    orb->speed = 4.0f;
    orb->pos = g_Player.pos;
    const float angle = Rng_F32(&g_Rng) * M_TWO_PI - M_PI;
    orb->velocity.x = X87_Cos(angle) * orb->speed;
    orb->velocity.y = X87_Sin(angle) * orb->speed;
    g_Player.bomb.hits[i].damage_dealt = 0;
    for (int32_t k = 0; k < M_ORB_SPRITES; k++) {
        Anm_StartScriptAtOrigin(
            &g_Player.bomb.vms[i * M_ORB_SPRITES + k], k + M_ORB_FIRST_SCRIPT);
    }
}

static void M_BurstOrb(const int32_t i)
{
    PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[i];
    PLAYER_BOMB_HIT *const hit = &g_Player.bomb.hits[i];
    Effect_Spawn(
        (EFFECT_TYPE)(EFFECT_PARTICLE + 2), orb->pos, 8, M_COLOR_WHITE);
    Effect_Spawn(EFFECT_SHOCKWAVE, orb->pos, 1, M_COLOR_SHOCKWAVE);
    orb->state = 2;
    for (int32_t k = 0; k < M_ORB_SPRITES; k++) {
        g_Player.bomb.vms[i * M_ORB_SPRITES + k].pending_interrupt = 1;
    }
    hit->size.x = 256.0f;
    hit->size.y = 256.0f;
    hit->damage = 200;
    g_Player.bomb.clear_areas[i].width = 256.0f;
    g_Player.bomb.clear_areas[i].height = 256.0f;
    ScreenEffect_Shake(16, 8, 0);
}

// Steers a flying orb to the homing target, or to the player when there
// is none, and bursts it after 100 damage or in the last 30 frames.
static void M_SteerOrb(const int32_t i)
{
    PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[i];
    PLAYER_BOMB_HIT *const hit = &g_Player.bomb.hits[i];
    const VEC3 aim = g_Player.homing_target.x > M_NO_TARGET
        ? g_Player.homing_target
        : g_Player.pos;
    float x = aim.x - orb->pos.x;
    float y = aim.y - orb->pos.y;
    float len = X87_Sqrt(x * x + y * y) / (orb->speed / 8.0f);
    if (len < 1.0f) {
        len = 1.0f;
    }
    x = x / len + orb->velocity.x;
    y = y / len + orb->velocity.y;
    len = X87_Sqrt(x * x + y * y);
    orb->speed = len > M_ORB_MAX_SPEED ? M_ORB_MAX_SPEED : len;
    if (orb->speed < 1.0f) {
        orb->speed = 1.0f;
    }
    orb->velocity.x = x * orb->speed / len;
    orb->velocity.y = y * orb->speed / len;
    hit->size.x = 48.0f;
    hit->size.y = 48.0f;
    hit->pos = orb->pos;
    hit->damage = 8;
    g_Player.bomb.clear_areas[i] = (PLAYER_CLEAR_AREA) {
        orb->pos.x,
        orb->pos.y,
        48.0f,
        48.0f,
    };
    if (hit->damage_dealt >= M_ORB_BURST_DAMAGE
        || M_Timer()->current >= g_Player.bomb.duration - M_ORB_BURST_FRAMES) {
        M_BurstOrb(i);
    }
}

// Launches an orb every 16 frames from frame 80 to frame 176, into parts
// 1 to 7. Frame 64 would launch into part 0, which stays unused.
static void M_ReimuA(void)
{
    PLAYER *const player = &g_Player;
    TIMER *const timer = M_Timer();
    if (M_Ends()) {
        return;
    }
    if (M_Starts()) {
        Gui_ShowBombName(0x4a1, "霊符「夢想封印」");
        player->bomb.duration = 300;
        Timer_Set(&player->state.timer, 360);
        for (int32_t i = 0; i < PLAYER_BOMB_PART_COUNT; i++) {
            player->bomb.parts[i].state = 0;
        }
        Item_CollectAll();
        Effect_Spawn(EFFECT_SHOCKWAVE, player->pos, 1, M_COLOR_SHOCKWAVE);
        player->bomb.clear_areas[8] = (PLAYER_CLEAR_AREA) {
            player->pos.x,
            player->pos.y,
            256.0f,
            256.0f,
        };
    }
    if (timer->current >= M_ORB_FIRST_FRAME && timer->current < M_ORB_LAST_FRAME
        && timer->current % M_ORB_INTERVAL == 0) {
        const int32_t i = (timer->current - M_ORB_FIRST_FRAME) / M_ORB_INTERVAL;
        if (i != 0) {
            M_LaunchOrb(i);
        }
    }
    player->state.value = PLAYER_INVULNERABLE;
    for (int32_t i = 0; i < PLAYER_BOMB_PART_COUNT; i++) {
        PLAYER_BOMB_PART *const orb = &player->bomb.parts[i];
        if (orb->state == 0) {
            continue;
        }
        if (orb->state == 1) {
            if (Timer_HasTicked(timer)) {
                M_SteerOrb(i);
            }
        } else if (Timer_HasTicked(timer)) {
            orb->state++;
            if (orb->state >= M_ORB_BURST_FRAMES) {
                orb->state = 0;
            }
        }
        orb->pos.x += orb->velocity.x;
        orb->pos.y += orb->velocity.y;
        for (int32_t k = 0; k < M_ORB_SPRITES; k++) {
            Anm_Step(&player->bomb.vms[i * M_ORB_SPRITES + k]);
        }
    }
    Timer_Tick(timer);
}

// Keeps two vertical and two horizontal pillars through where the player
// started the bomb. The boxes follow the sprites on the odd frames only.
// The clear areas keep their sizes on the even frames too, so they stay
// at the places of the frame before.
static void M_ReimuB(void)
{
    PLAYER *const player = &g_Player;
    if (M_Ends()) {
        return;
    }
    if (M_Starts()) {
        Item_CollectAll();
        Gui_ShowBombName(0x4a3, "夢符「封魔陣」");
        player->bomb.duration = 140;
        Timer_Set(&player->state.timer, 200);
        for (int32_t i = 0; i < M_PILLAR_COUNT; i++) {
            Anm_StartScriptAtOrigin(
                &player->bomb.vms[i], i + M_PILLAR_FIRST_SCRIPT);
        }
        player->bomb.parts[0].pos = (VEC3) { player->pos.x, 224.0f, 0.42f };
        player->bomb.parts[1].pos = (VEC3) { 192.0f, player->pos.y, 0.415f };
        player->bomb.parts[2].pos = (VEC3) { player->pos.x, 224.0f, 0.41f };
        player->bomb.parts[3].pos = (VEC3) { 192.0f, player->pos.y, 0.405f };
        ScreenEffect_Shake(60, 2, 6);
    } else {
        if (M_Timer()->current == M_SHAKE_WARMUP_FRAMES) {
            ScreenEffect_Shake(80, 20, 0);
        }
        for (int32_t i = 0; i < M_PILLAR_COUNT; i++) {
            player->bomb.clear_areas[i].width = m_PillarSizes[i].x;
            player->bomb.clear_areas[i].height = m_PillarSizes[i].y;
        }
        for (int32_t i = 0; i < M_PILLAR_COUNT; i++) {
            const PLAYER_BOMB_PART *const pillar = &player->bomb.parts[i];
            const ANM_VM *const vm = &player->bomb.vms[i];
            PLAYER_CLEAR_AREA *const area = &player->bomb.clear_areas[i];
            PLAYER_BOMB_HIT *const hit = &player->bomb.hits[i];
            Anm_Step(&player->bomb.vms[i]);
            if (!M_SetsBoxes(2)) {
                continue;
            }
            area->x = pillar->pos.x + vm->pos_offset.x;
            area->y = pillar->pos.y + vm->pos_offset.y;
            hit->size.x = area->width;
            hit->size.y = area->height;
            hit->pos = Vec3_Add(pillar->pos, vm->pos_offset);
            hit->damage = 8;
        }
    }
    M_Finish();
}

// Sends eight stars out from the player. Their boxes act two frames in
// three.
static void M_MarisaA(void)
{
    PLAYER *const player = &g_Player;
    if (M_Ends()) {
        return;
    }
    if (M_Starts()) {
        Item_CollectAll();
        Gui_ShowBombName(0x4a3, "魔符「スターダストレヴァリエ」");
        player->bomb.duration = 250;
        Timer_Set(&player->state.timer, 300);
        for (int32_t i = 0; i < M_STAR_COUNT; i++) {
            PLAYER_BOMB_PART *const star = &player->bomb.parts[i];
            Anm_StartScriptAtOrigin(
                &player->bomb.vms[i], i % 3 + M_STAR_FIRST_SCRIPT);
            star->pos = player->pos;
            const float angle = (float)i * M_TWO_PI / 8.0f;
            star->velocity.x = X87_Cos(angle) * 2.0f;
            star->velocity.y = X87_Sin(angle) * 2.0f;
            star->velocity.z = 0.0f;
        }
        ScreenEffect_Shake(120, 4, 1);
    } else {
        for (int32_t i = 0; i < M_STAR_COUNT; i++) {
            PLAYER_BOMB_PART *const star = &player->bomb.parts[i];
            star->pos = Vec3_Add(star->pos, star->velocity);
            if (M_SetsBoxes(3)) {
                PLAYER_BOMB_HIT *const hit = &player->bomb.hits[i];
                player->bomb.clear_areas[i] = (PLAYER_CLEAR_AREA) {
                    star->pos.x,
                    star->pos.y,
                    128.0f,
                    128.0f,
                };
                hit->size.x = 128.0f;
                hit->size.y = 128.0f;
                hit->pos = star->pos;
                hit->damage = 8;
            }
            Anm_Step(&player->bomb.vms[i]);
        }
    }
    M_Finish();
}

// Fires a beam from the player to the top of the play area, and slows
// the player while it runs. Its box acts three frames in four.
static void M_MarisaB(void)
{
    PLAYER *const player = &g_Player;
    if (M_Ends()) {
        player->move.speed_scale_y = 1.0f;
        player->move.speed_scale_x = 1.0f;
        return;
    }
    if (M_Starts()) {
        Item_CollectAll();
        Gui_ShowBombName(0x4a1, "恋符「マスタースパーク」");
        player->bomb.duration = 300;
        Timer_Set(&player->state.timer, 360);
        for (int32_t i = 0; i < M_BEAM_SPRITES; i++) {
            Anm_StartScriptAtOrigin(
                &player->bomb.vms[i], i + M_BEAM_FIRST_SCRIPT);
            player->bomb.parts[i].pos = player->pos;
        }
        player->move.speed_scale_y = M_BEAM_SPEED_SCALE;
        player->move.speed_scale_x = M_BEAM_SPEED_SCALE;
    } else {
        if (M_Timer()->current == M_SHAKE_WARMUP_FRAMES) {
            ScreenEffect_Shake(60, 1, 7);
        } else if (M_Timer()->current == 120) {
            ScreenEffect_Shake(200, 24, 0);
        }
        if (M_SetsBoxes(4)) {
            PLAYER_CLEAR_AREA *const area = &player->bomb.clear_areas[0];
            PLAYER_BOMB_HIT *const hit = &player->bomb.hits[0];
            *area = (PLAYER_CLEAR_AREA) {
                192.0f,
                player->pos.y / 2.0f,
                384.0f,
                player->pos.y,
            };
            hit->size.x = 384.0f;
            hit->size.y = player->pos.y;
            hit->pos.x = area->x;
            hit->pos.y = area->y;
            hit->damage = 12;
        }
        for (int32_t i = 0; i < M_BEAM_SPRITES; i++) {
            Anm_Step(&player->bomb.vms[i]);
        }
    }
    M_Finish();
}

void Bomb_Update(void)
{
    switch (g_GameManager.run.character * 2 + g_GameManager.run.shot_type) {
    case 0:
        M_ReimuA();
        break;
    case 1:
        M_ReimuB();
        break;
    case 2:
        M_MarisaA();
        break;
    case 3:
        M_MarisaB();
        break;
    default:
        break;
    }
}
