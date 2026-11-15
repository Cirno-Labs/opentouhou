#include "th07/bomb.h"

#include "th07/anm.h"
#include "th07/effect.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/screen_effect.h"
#include "th07/sound.h"
#include "th07/stubs.h"

#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/rng.h"
#include "stg/timer.h"

#define M_NO_TARGET -100.0f
#define M_NO_ITEM 6
#define M_PART_FREE 0
#define M_PART_FLYING 1
#define M_PART_BURST 2
#define M_ORB_COUNT 8
#define M_ORB_SPRITES 4
#define M_ORB_FIRST_SCRIPT 0x485
#define M_ORB_CLEARED_PARTS 0x20
#define M_ORB_BURST_FRAMES 30
#define M_ORB_BURST_DAMAGE 400
#define M_ORB_MAX_SPEED 10.0f
#define M_BARRIER_COUNT 4
#define M_BARRIER_FIRST_SCRIPT 0x489
#define M_BOUNDARY_SPRITES 3
#define M_BOUNDARY_FIRST_SCRIPT 0x48d
#define M_STAR_COUNT 8
#define M_STAR_FIRST_SCRIPT 0x405
#define M_MILKY_WAY_STARS 24
#define M_MILKY_WAY_TRAIL 8
#define M_LASER_COUNT 3
#define M_LASER_BOXES 6
#define M_LASER_FIRST_SCRIPT 0x40c
#define M_SPARK_SPRITES 4
#define M_SPARK_FIRST_SCRIPT 0x408
#define M_KNIFE_COUNT 0x60
#define M_KNIFE_FIRST_SCRIPT 0x405
#define M_DOLL_FIRST_SCRIPT 0x407
#define M_KNIFE_HIT_SCRIPT 0x460
#define M_KNIFE_SPENT 999 // damage_dealt once a knife has hit
#define M_KNIVES_PER_CALL 5
#define M_SQUARE_COUNT 4
#define M_SQUARE_FIRST_SCRIPT 0x409
#define M_PRIVATE_SQUARE_COUNT 2
#define M_PRIVATE_SQUARE_FIRST_SCRIPT 0x40d
#define M_SHAKE_FRAME 60
#define M_AURA_SCALE 0.0625f
#define M_AURA_COLOR 0xff4040u
#define M_EFFECT_SPARKS ((EFFECT_TYPE)(EFFECT_PARTICLE + 2))
#define M_COLOR_WHITE 0xffffffffu
#define M_COLOR_SHOCKWAVE 0xff4040ffu
#define M_COLOR_KNIFE_HIT 0xffff80ffu

typedef struct {
    int32_t sprite; // of the portrait
    const char *name;
} M_NAME;

// Holds the names of the bombs by shot_index * 2 + focused.
static const M_NAME m_Names[] = {
    { 0x4a1, "霊符「夢想封印　散」" },
    { 0x4a1, "霊符「夢想封印　集」" },
    { 0x4a1, "夢符「封魔陣」" },
    { 0x4a1, "夢符「二重結界」" },
    { 0x4a3, "魔符「スターダストレヴァリエ」" },
    { 0x4a2, "魔符「ミルキーウェイ」" },
    { 0x4a1, "恋符「ノンディレクショナルレーザー」" },
    { 0x4a2, "恋符「マスタースパーク」" },
    { 0x4a1, "幻符「インディスクリミネイト」" },
    { 0x4a1, "幻符「殺人ドール」" },
    { 0x4a3, "時符「パーフェクトスクウェア」" },
    { 0x4a3, "時符「プライベートスクウェア」" },
};

// Holds the widths and heights of the barriers of Reimu B.
static const VEC2 m_BarrierSizes[M_BARRIER_COUNT] = {
    { 62.0f, 448.0f },
    { 384.0f, 62.0f },
    { 62.0f, 448.0f },
    { 384.0f, 62.0f },
};

static TIMER *M_Timer(void)
{
    return &g_Player.bomb.timer;
}

static int32_t M_Index(void)
{
    return g_GameManager.run.shot_index * 2 + (g_Player.bomb.focused ? 1 : 0);
}

static bool M_TimerHasTickedTo(const TIMER *const timer, const int32_t frame)
{
    return Timer_HasTicked(timer) && timer->current == frame;
}

static bool M_HasTickedTo(const int32_t frame)
{
    return M_TimerHasTickedTo(M_Timer(), frame);
}

static bool M_Starts(void)
{
    return M_HasTickedTo(0);
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

static void M_ShowName(void)
{
    const M_NAME *const name = &m_Names[M_Index()];
    Gui_ShowBombName(name->sprite, name->name);
}

static void M_Slow(const float scale)
{
    g_Player.move.speed_scale_y = scale;
    g_Player.move.speed_scale_x = scale;
}

static void M_Finish(void)
{
    g_Player.state.value = PLAYER_INVULNERABLE;
    Timer_TickAt(M_Timer(), g_GameSpeed);
}

// Sets the cherry drain of each frame to share of the cherry, less on the
// harder difficulties, over the duration, in tens and at least minimum over
// the duration.
static void M_SetDrain(int32_t minimum, const float share)
{
    PLAYER *const player = &g_Player;
    int32_t drain = X87_Ftol(
        (float)(g_GameManager.cherry.value - g_GameManager.cherry.offset)
        * share);
    switch (g_GameManager.run.difficulty) {
    case 2:
        drain /= 2;
        break;
    case 3:
        drain /= 4;
        break;
    case 4:
    case 5:
        drain /= 3;
        break;
    default:
        break;
    }
    drain /= player->bomb.duration;
    drain -= drain % 10;
    minimum /= player->bomb.duration;
    minimum -= minimum % 10;
    player->bomb.drain = drain < minimum ? minimum : drain;
}

// Replaces the bomb aura with a spell circle that shrinks over the state
// timer, turning the other way. The old aura stops but keeps its slot.
static void M_StartAura(void)
{
    PLAYER *const player = &g_Player;
    if (player->effects.bomb_aura != nullptr) {
        player->effects.bomb_aura->active = false;
    }
    EFFECT *const effect = Effect_SpawnReserved(
        EFFECT_SPELL_CIRCLE, player->pos, 0, M_COLOR_WHITE);
    ANM_VM *const vm = &effect->vm;
    Timer_Set(&vm->interp_time[ANM_INTERP_SCALE], 0);
    vm->interp_length[ANM_INTERP_SCALE] = player->state.timer.current;
    vm->interp_ease[ANM_INTERP_SCALE] = ANM_EASE_LINEAR;
    vm->scale_from = (VEC2) { vm->scale_x, vm->scale_y };
    vm->scale_to = (VEC2) { M_AURA_SCALE, M_AURA_SCALE };
    vm->int_vars[0] = player->state.timer.current;
    vm->rotation_speed.z *= -1.0f;
    vm->color = (vm->color & 0xff000000u) | M_AURA_COLOR;
    player->effects.bomb_aura = effect;
}

static void M_AddClearCircle(const VEC3 pos, const float radius)
{
    Player_AddClearCircle(pos, radius, 0.0f, 0, M_NO_ITEM);
}

static void M_StepOrb(PLAYER_BOMB_PART *const orb)
{
    orb->pos.x += g_GameSpeed * orb->velocity.x;
    orb->pos.y += g_GameSpeed * orb->velocity.y;
    for (int32_t k = 0; k < M_ORB_SPRITES; k++) {
        Anm_Step(&orb->vms[k]);
    }
}

// Bursts orb i into a 256 box and a growing clear circle.
static void M_BurstOrb(
    const int32_t i, const float radius, const float growth,
    const int32_t frames)
{
    PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[i];
    PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[i];
    Effect_Spawn(M_EFFECT_SPARKS, orb->pos, 8, M_COLOR_WHITE);
    Effect_Spawn(EFFECT_SHOCKWAVE, orb->pos, 1, M_COLOR_SHOCKWAVE);
    orb->state = M_PART_BURST;
    for (int32_t k = 0; k < M_ORB_SPRITES; k++) {
        orb->vms[k].pending_interrupt = 1;
    }
    box->pos = orb->pos;
    box->size.x = 256.0f;
    box->size.y = 256.0f;
    box->damage = M_ORB_BURST_DAMAGE;
    Player_AddClearCircle(orb->pos, radius, growth, frames, M_NO_ITEM);
    Sound_Queue(SOUND_SHOT_0_ALT);
    ScreenEffect_Shake(16, 8, 0);
}

static void M_SetOrbBox(const int32_t i)
{
    PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[i];
    box->size.x = 48.0f;
    box->size.y = 48.0f;
    box->pos = g_Player.bomb.parts[i].pos;
    box->damage = 8;
    M_AddClearCircle(g_Player.bomb.parts[i].pos, 128.0f);
}

static void M_StartOrbScripts(PLAYER_BOMB_PART *const orb)
{
    for (int32_t k = 0; k < M_ORB_SPRITES; k++) {
        Anm_StartScriptAtOrigin(&orb->vms[k], k + M_ORB_FIRST_SCRIPT);
    }
}

// Fans an orb out every 6 frames from frame 12, turning away from the side
// that the bomb started on. Each orb slows down, flies back and bursts once
// its speed falls below -10.
static void M_ReimuA(void)
{
    PLAYER *const player = &g_Player;
    TIMER *const timer = M_Timer();
    if (M_Ends()) {
        return;
    }
    if (M_Starts()) {
        M_ShowName();
        player->bomb.duration = 140;
        Timer_Set(&player->state.timer, 200);
        M_StartAura();
        for (int32_t i = 0; i < M_ORB_CLEARED_PARTS; i++) {
            player->bomb.parts[i].state = M_PART_FREE;
        }
        Item_CollectAll();
        Effect_Spawn(EFFECT_SHOCKWAVE, player->pos, 1, M_COLOR_SHOCKWAVE);
        Player_AddClearCircle(player->pos, 32.0f, 8.0f, 16, M_NO_ITEM);
        player->bomb.start_pos = player->pos;
        M_SetDrain(4000, 0.2f);
    }
    // Launches 12 orbs, though only the first 8 move: parts 8 to 11 start
    // their scripts, reset their boxes and queue a sound.
    if (Timer_HasTicked(timer) && timer->current >= 8 && timer->current < 80
        && timer->current % 6 == 0) {
        const int32_t i = (timer->current - 8) / 6;
        PLAYER_BOMB_PART *const orb = &player->bomb.parts[i];
        orb->state = M_PART_FLYING;
        orb->speed = 15.0f;
        orb->pos = player->pos;
        const float angle = player->bomb.start_pos.x < 192.0f
            ? (float)i * MATH_TWO_PI / 8.0f - MATH_HALF_PI
            : (float)-i * MATH_TWO_PI / 8.0f - MATH_HALF_PI;
        orb->angle = Angle_AddNormalize(angle, 0.0f);
        orb->burst_timer = 0;
        player->damage_boxes[i].damage_dealt = 0;
        M_StartOrbScripts(orb);
        Sound_Queue(SOUND_GUN);
    }
    player->state.value = PLAYER_INVULNERABLE;
    for (int32_t i = 0; i < M_ORB_COUNT; i++) {
        PLAYER_BOMB_PART *const orb = &player->bomb.parts[i];
        PLAYER_DAMAGE_BOX *const box = &player->damage_boxes[i];
        if (orb->state == M_PART_FREE) {
            continue;
        }
        if (orb->state == M_PART_FLYING) {
            orb->speed -= 0.4f * g_GameSpeed;
            X87_SinCosScale(
                orb->angle, orb->speed, &orb->velocity.x, &orb->velocity.y);
            if (orb->speed < -10.0f) {
                M_BurstOrb(i, 64.0f, 4.2666669f, 30);
                orb->velocity = (VEC3) {};
            }
            // Replaces the burst box of this frame.
            if (Timer_HasTicked(timer)) {
                M_SetOrbBox(i);
            }
        } else if (Timer_HasTicked(timer)) {
            box->pos = orb->pos;
            box->size.x = 256.0f;
            box->size.y = 256.0f;
            box->damage = 2;
            orb->burst_timer++;
            if (orb->burst_timer >= M_ORB_BURST_FRAMES) {
                orb->state = M_PART_FREE;
            }
        }
        M_StepOrb(orb);
    }
    Timer_TickAt(timer, g_GameSpeed);
}

static void M_LaunchHomingOrb(const int32_t i)
{
    PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[i];
    orb->state = M_PART_FLYING;
    orb->burst_timer = 0;
    orb->homing_speed = 8.0f;
    orb->pos = g_Player.pos;
    const float angle = Rng_F32(&g_Rng) * MATH_TWO_PI - MATH_PI;
    X87_SinCosScale(
        angle, orb->homing_speed, &orb->velocity.x, &orb->velocity.y);
    g_Player.damage_boxes[i].damage_dealt = 0;
    M_StartOrbScripts(orb);
    Sound_Queue(SOUND_GUN);
}

// Steers a flying orb to the homing target, or to the player when there
// is none, and bursts it after 100 damage or in the last 30 frames. A burst
// orb keeps its velocity.
static void M_SteerOrb(const int32_t i)
{
    PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[i];
    const VEC3 aim = g_Player.homing_target.x > M_NO_TARGET
        ? g_Player.homing_target
        : g_Player.pos;
    float x = aim.x - orb->pos.x;
    float y = aim.y - orb->pos.y;
    float len = X87_Sqrt(x * x + y * y) / (orb->homing_speed / 8.0f);
    if (len < 1.0f) {
        len = 1.0f;
    }
    x = x / len + orb->velocity.x;
    y = y / len + orb->velocity.y;
    len = X87_Sqrt(x * x + y * y);
    orb->homing_speed = len > M_ORB_MAX_SPEED ? M_ORB_MAX_SPEED : len;
    if (orb->homing_speed < 1.0f) {
        orb->homing_speed = 1.0f;
    }
    orb->velocity.x = x * orb->homing_speed / len;
    orb->velocity.y = y * orb->homing_speed / len;
    M_SetOrbBox(i);
    if (g_Player.damage_boxes[i].damage_dealt >= 100
        || M_Timer()->current >= g_Player.bomb.duration - M_ORB_BURST_FRAMES) {
        M_BurstOrb(i, 32.0f, 6.6666665f, 15);
    }
}

// Launches a homing orb every 16 frames from frame 80 to frame 176, into
// parts 1 to 7, and slows the player. Frame 64 would launch into part 0,
// which stays unused.
static void M_ReimuAFocused(void)
{
    PLAYER *const player = &g_Player;
    TIMER *const timer = M_Timer();
    if (M_Ends()) {
        M_Slow(1.0f);
        return;
    }
    if (M_Starts()) {
        M_ShowName();
        player->bomb.duration = 300;
        Timer_Set(&player->state.timer, 360);
        M_StartAura();
        for (int32_t i = 0; i < M_ORB_COUNT; i++) {
            player->bomb.parts[i].state = M_PART_FREE;
        }
        Item_CollectAll();
        Effect_Spawn(EFFECT_SHOCKWAVE, player->pos, 1, M_COLOR_SHOCKWAVE);
        Player_AddClearCircle(player->pos, 32.0f, 8.0f, 16, M_NO_ITEM);
        M_SetDrain(5000, 0.22f);
        M_Slow(0.6f);
    }
    // Has no tick test: below full speed, an orb launches again, with a new
    // angle, on every call of its frame.
    if (timer->current >= 60 && timer->current < 180
        && timer->current % 16 == 0) {
        const int32_t i = (timer->current - 60) / 16;
        if (i != 0) {
            M_LaunchHomingOrb(i);
        }
    }
    player->state.value = PLAYER_INVULNERABLE;
    for (int32_t i = 0; i < M_ORB_COUNT; i++) {
        PLAYER_BOMB_PART *const orb = &player->bomb.parts[i];
        if (orb->state == M_PART_FREE) {
            continue;
        }
        if (orb->state == M_PART_FLYING) {
            if (Timer_HasTicked(timer)) {
                M_SteerOrb(i);
            }
        } else if (Timer_HasTicked(timer)) {
            orb->burst_timer++;
            if (orb->burst_timer >= M_ORB_BURST_FRAMES) {
                orb->state = M_PART_FREE;
            }
        }
        M_StepOrb(orb);
    }
    Timer_TickAt(timer, g_GameSpeed);
}

// Keeps two vertical and two horizontal barriers through where the player
// started the bomb. On the odd frames the clear areas and boxes follow the
// sprites; on the even frames the clear areas stay on the player.
static void M_ReimuB(void)
{
    PLAYER *const player = &g_Player;
    if (M_Ends()) {
        return;
    }
    if (M_Starts()) {
        Item_CollectAll();
        M_ShowName();
        player->bomb.duration = 140;
        Timer_Set(&player->state.timer, 200);
        M_StartAura();
        for (int32_t i = 0; i < M_BARRIER_COUNT; i++) {
            Anm_StartScriptAtOrigin(
                &player->bomb.parts[i].vms[0], i + M_BARRIER_FIRST_SCRIPT);
        }
        Sound_Queue(SOUND_POWER_1);
        PLAYER_BOMB_PART *const parts = player->bomb.parts;
        parts[0].pos = (VEC3) { player->pos.x, 224.0f, 0.42f };
        parts[1].pos = (VEC3) { 192.0f, player->pos.y, 0.415f };
        parts[2].pos = (VEC3) { player->pos.x, 224.0f, 0.41f };
        parts[3].pos = (VEC3) { 192.0f, player->pos.y, 0.405f };
        ScreenEffect_Shake(60, 2, 6);
        M_SetDrain(3000, 0.17f);
    } else {
        if (M_Timer()->current == M_SHAKE_FRAME) {
            ScreenEffect_Shake(80, 20, 0);
        }
        PLAYER_CLEAR_AREA *areas[M_BARRIER_COUNT];
        for (int32_t i = 0; i < M_BARRIER_COUNT; i++) {
            areas[i] = Player_AddClearArea(
                player->pos, m_BarrierSizes[i].x, m_BarrierSizes[i].y,
                M_NO_ITEM);
        }
        for (int32_t i = 0; i < M_BARRIER_COUNT; i++) {
            const PLAYER_BOMB_PART *const barrier = &player->bomb.parts[i];
            const ANM_VM *const vm = &barrier->vms[0];
            PLAYER_DAMAGE_BOX *const box = &player->damage_boxes[i];
            Anm_Step(&player->bomb.parts[i].vms[0]);
            if (!Timer_HasTicked(M_Timer()) || M_Timer()->current % 2 == 0) {
                continue;
            }
            areas[i]->x = barrier->pos.x + vm->pos_offset.x;
            areas[i]->y = barrier->pos.y + vm->pos_offset.y;
            box->size.x = areas[i]->width;
            box->size.y = areas[i]->height;
            box->pos = Vec3_Add(barrier->pos, vm->pos_offset);
            box->damage = 16;
        }
    }
    M_Finish();
}

// Keeps a double boundary around where the player started the bomb, and
// slows the player. Its box acts on every call.
static void M_ReimuBFocused(void)
{
    PLAYER *const player = &g_Player;
    PLAYER_BOMB_PART *const part = &player->bomb.parts[0];
    if (M_Ends()) {
        M_Slow(1.0f);
        return;
    }
    if (M_Starts()) {
        Item_CollectAll();
        M_ShowName();
        player->bomb.duration = 190;
        Timer_Set(&player->state.timer, 250);
        M_StartAura();
        for (int32_t i = 0; i < M_BOUNDARY_SPRITES; i++) {
            Anm_StartScriptAtOrigin(&part->vms[i], i + M_BOUNDARY_FIRST_SCRIPT);
        }
        Sound_Queue(SOUND_POWER_1);
        ScreenEffect_Shake(60, 2, 6);
        player->bomb.start_pos = player->pos;
        M_SetDrain(3000, 0.17f);
        M_Slow(0.4f);
        Player_AddClearCircle(player->pos, 192.0f, 0.384f, 210, M_NO_ITEM);
    } else {
        if (M_Timer()->current == M_SHAKE_FRAME) {
            ScreenEffect_Shake(80, 20, 0);
        }
        for (int32_t i = 0; i < M_BOUNDARY_SPRITES; i++) {
            Anm_Step(&part->vms[i]);
        }
        PLAYER_DAMAGE_BOX *const box = &player->damage_boxes[0];
        box->size.x = 256.0f;
        box->size.y = 256.0f;
        box->pos = Vec3_Add(player->bomb.start_pos, part->vms[0].pos_offset);
        box->damage = 18;
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
        M_ShowName();
        player->bomb.duration = 200;
        Timer_Set(&player->state.timer, 250);
        M_StartAura();
        for (int32_t i = 0; i < M_STAR_COUNT; i++) {
            PLAYER_BOMB_PART *const star = &player->bomb.parts[i];
            Anm_StartScriptAtOrigin(&star->vms[0], i % 3 + M_STAR_FIRST_SCRIPT);
            star->pos = player->pos;
            const float angle = (float)i * MATH_TWO_PI / 8.0f;
            X87_SinCosScale(angle, 2.0f, &star->velocity.x, &star->velocity.y);
            star->velocity.z = 0.0f;
        }
        Sound_Queue(SOUND_POWER_1);
        ScreenEffect_Shake(120, 4, 1);
        M_SetDrain(8000, 0.3f);
    } else {
        for (int32_t i = 0; i < M_STAR_COUNT; i++) {
            PLAYER_BOMB_PART *const star = &player->bomb.parts[i];
            star->pos =
                Vec3_Add(star->pos, Vec3_Scale(star->velocity, g_GameSpeed));
            if (Timer_HasTicked(M_Timer()) && M_Timer()->current % 3 != 0) {
                PLAYER_DAMAGE_BOX *const box = &player->damage_boxes[i];
                M_AddClearCircle(star->pos, 96.0f);
                box->size.x = 128.0f;
                box->size.y = 128.0f;
                box->pos = star->pos;
                box->damage = 8;
            }
            Anm_Step(&star->vms[0]);
        }
    }
    M_Finish();
}

static float M_MilkyWayAngle(void)
{
    return Rng_F32(&g_Rng) * 0.3926991f - 0.19634955f - MATH_HALF_PI;
}

static void M_LaunchMilkyWayStar(const int32_t i)
{
    PLAYER *const player = &g_Player;
    PLAYER_BOMB_PART *const star = &player->bomb.parts[i];
    Anm_StartScriptAtOrigin(&star->vms[0], i % 3 + M_STAR_FIRST_SCRIPT);
    star->pos = player->pos;
    for (int32_t k = 0; k < M_MILKY_WAY_TRAIL; k++) {
        star->trail[k] = player->pos;
    }
    star->state = M_PART_FLYING;
    float angle = M_MilkyWayAngle();
    X87_SinCosScale(angle, -5.0f, &star->velocity.x, &star->velocity.y);
    star->velocity.z = 0.0f;
    angle = M_MilkyWayAngle();
    X87_SinCosScale(angle, 0.24f, &star->acceleration.x, &star->acceleration.y);
    star->acceleration.z = 0.0f;
    Sound_Queue(SOUND_SHOT_0);
    ScreenEffect_Shake(120, 4, 1);
    player->damage_boxes[i].damage_dealt = 0;
}

// Launches a star every 6 frames, 24 in all, that falls back and then
// rises off the top, and slows the player. A star's box stops after 80
// damage; its clear circle stays.
static void M_MarisaAFocused(void)
{
    PLAYER *const player = &g_Player;
    TIMER *const timer = M_Timer();
    if (M_Ends()) {
        M_Slow(1.0f);
        return;
    }
    if (M_Starts()) {
        Item_CollectAll();
        M_ShowName();
        player->bomb.duration = 260;
        Timer_Set(&player->state.timer, 310);
        M_StartAura();
        for (int32_t i = 0; i < M_MILKY_WAY_STARS; i++) {
            player->bomb.parts[i].state = M_PART_FREE;
        }
        M_SetDrain(9000, 0.33f);
        M_Slow(0.4f);
    }
    if (Timer_HasTicked(timer) && timer->current % 6 == 0) {
        const int32_t i = timer->current / 6;
        if (i < M_MILKY_WAY_STARS) {
            M_LaunchMilkyWayStar(i);
        }
    }
    for (int32_t i = 0; i < M_MILKY_WAY_STARS; i++) {
        PLAYER_BOMB_PART *const star = &player->bomb.parts[i];
        PLAYER_DAMAGE_BOX *const box = &player->damage_boxes[i];
        if (star->state == M_PART_FREE) {
            continue;
        }
        for (int32_t k = M_MILKY_WAY_TRAIL - 1; k > 0; k--) {
            star->trail[k] = star->trail[k - 1];
        }
        star->trail[0] = star->pos;
        star->pos =
            Vec3_Add(star->pos, Vec3_Scale(star->velocity, g_GameSpeed));
        star->velocity = Vec3_Add(
            star->velocity, Vec3_Scale(star->acceleration, g_GameSpeed));
        if (star->pos.y < -256.0f) {
            star->state = M_PART_FREE;
        }
        M_AddClearCircle(star->pos, 96.0f);
        if (box->damage_dealt < 80) {
            box->size.x = 128.0f;
            box->size.y = 128.0f;
            box->pos = star->pos;
            box->damage = 12;
        }
        Anm_Step(&star->vms[0]);
    }
    M_Finish();
}

// Turns three lasers around the player, one way when the bomb started left
// of the centre and the other way otherwise, faster as the bomb runs, and
// slows the player. Each laser carries six boxes along its sprite.
static void M_MarisaB(void)
{
    PLAYER *const player = &g_Player;
    if (M_Ends()) {
        M_Slow(1.0f);
        return;
    }
    if (M_Starts()) {
        Item_CollectAll();
        player->bomb.start_pos = player->pos;
        M_ShowName();
        player->bomb.duration = 300;
        Timer_Set(&player->state.timer, 300);
        M_StartAura();
        for (int32_t i = 0; i < M_LASER_COUNT; i++) {
            PLAYER_BOMB_PART *const laser = &player->bomb.parts[i];
            Anm_StartScriptAtOrigin(&laser->vms[0], i + M_LASER_FIRST_SCRIPT);
            laser->pos = player->pos;
            laser->laser_angle = (float)i * MATH_TWO_PI / 3.0f - MATH_HALF_PI;
        }
        Sound_Queue(SOUND_SPARK);
        M_Slow(0.4f);
        M_SetDrain(8000, 0.35f);
    } else {
        const TIMER *const timer = M_Timer();
        const float duration = (float)player->bomb.duration;
        PLAYER_DAMAGE_BOX *box = player->damage_boxes;
        for (int32_t i = 0; i < M_LASER_COUNT; i++) {
            PLAYER_BOMB_PART *const laser = &player->bomb.parts[i];
            ANM_VM *const vm = &laser->vms[0];
            const float turn = player->bomb.start_pos.x < 192.0f
                ? Timer_AsFloat(timer) * MATH_PI / 30.0f / duration
                : Timer_AsFloat(timer) * -MATH_PI / 30.0f / duration;
            laser->laser_angle = Angle_AddNormalize(laser->laser_angle, turn);
            const float angle = laser->laser_angle;
            vm->pos = player->pos;
            vm->pos.x +=
                X87_Cos(angle) * vm->sprite_data->height * vm->scale_y / 2.0f;
            vm->pos.y +=
                X87_Sin(angle) * vm->sprite_data->height * vm->scale_y / 2.0f;
            float len = 32.0f;
            for (int32_t k = 0; k < M_LASER_BOXES; k++, box++) {
                box->pos = player->pos;
                box->pos.x += X87_Cos(angle) * len;
                box->pos.y += X87_Sin(angle) * len;
                box->size.x = 128.0f;
                box->size.y = 128.0f;
                box->damage = 10;
                M_AddClearCircle(box->pos, 64.0f);
                len += vm->sprite_data->height * vm->scale_y / 5.0f;
            }
            Anm_Step(vm);
        }
        if (timer->current == 20) {
            ScreenEffect_Shake(60, 1, 7);
        } else if (timer->current == 80) {
            ScreenEffect_Shake(100, 24, 0);
        }
    }
    M_Finish();
}

// Fires a beam from the player to the top of the play area, and slows the
// player. Its box acts three frames in four.
static void M_MarisaBFocused(void)
{
    PLAYER *const player = &g_Player;
    PLAYER_BOMB_PART *const part = &player->bomb.parts[0];
    if (M_Ends()) {
        M_Slow(1.0f);
        return;
    }
    if (M_Starts()) {
        Item_CollectAll();
        M_ShowName();
        player->bomb.duration = 340;
        Timer_Set(&player->state.timer, 390);
        M_StartAura();
        for (int32_t i = 0; i < M_SPARK_SPRITES; i++) {
            Anm_StartScriptAtOrigin(&part->vms[i], i + M_SPARK_FIRST_SCRIPT);
            player->bomb.parts[i].pos = player->pos;
        }
        Sound_Queue(SOUND_SPARK);
        M_Slow(0.2f);
        M_SetDrain(10000, 0.41f);
    } else {
        if (M_Timer()->current == M_SHAKE_FRAME) {
            ScreenEffect_Shake(60, 1, 7);
        } else if (M_Timer()->current == 120) {
            ScreenEffect_Shake(200, 24, 0);
        }
        if (Timer_HasTicked(M_Timer()) && M_Timer()->current % 4 != 0) {
            PLAYER_DAMAGE_BOX *const box = &player->damage_boxes[0];
            box->size.x = 384.0f;
            box->size.y = player->pos.y;
            box->pos.x = 192.0f;
            box->pos.y = player->pos.y / 2.0f;
            box->damage = 23;
            Player_AddClearArea(box->pos, 384.0f, player->pos.y, M_NO_ITEM);
        }
        for (int32_t i = 0; i < M_SPARK_SPRITES; i++) {
            Anm_Step(&part->vms[i]);
        }
    }
    M_Finish();
}

static void M_SpawnKnifeEffect(void)
{
    g_Player.bomb.parts[0].effect =
        Effect_Spawn(EFFECT_KNIFE_BOMB, g_Player.pos, 1, M_COLOR_WHITE);
    Sound_Queue(SOUND_POWER_0);
}

// Throws knife i from 24 out of where the bomb started, at a random angle,
// speed, speed step and turn.
static void M_ThrowKnife(const int32_t i)
{
    PLAYER *const player = &g_Player;
    PLAYER_BOMB_PART *const knife = &player->bomb.parts[i];
    knife->state = M_PART_FLYING;
    Anm_StartScriptAtOrigin(&knife->vms[0], (i & 1) + M_KNIFE_FIRST_SCRIPT);
    knife->angle = Rng_F32(&g_Rng) * MATH_TWO_PI - MATH_PI;
    knife->speed = Rng_F32(&g_Rng) * 6.0f + 5.5f;
    knife->speed_step = Rng_F32(&g_Rng) * 0.1f + 0.1f;
    // acceleration.x holds the turn of each call.
    knife->acceleration.x = Rng_F32(&g_Rng) * 0.06283186f - 0.031415927f;
    X87_SinCosScale(
        knife->angle, 24.0f, &knife->velocity.x, &knife->velocity.y);
    knife->pos = Vec3_Add(player->bomb.start_pos, knife->velocity);
    knife->velocity.z = 0.0f;
    player->damage_boxes[i].damage_dealt = 0;
}

// Turns and speeds knife i up, and moves its box and clear circle with it
// until it has dealt 30 damage. Frees it outside the play area.
static void M_MoveKnife(const int32_t i)
{
    PLAYER_BOMB_PART *const knife = &g_Player.bomb.parts[i];
    PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[i];
    knife->angle = Angle_AddNormalize(knife->angle, knife->acceleration.x);
    knife->speed += knife->speed_step;
    X87_SinCosScale(
        knife->angle, knife->speed, &knife->velocity.x, &knife->velocity.y);
    if (box->damage_dealt < 30) {
        knife->pos =
            Vec3_Add(knife->pos, Vec3_Scale(knife->velocity, g_GameSpeed));
        M_AddClearCircle(knife->pos, 32.0f);
        box->size.x = 24.0f;
        box->size.y = 24.0f;
        box->pos = knife->pos;
        box->damage = 10;
    } else if (box->damage_dealt < M_KNIFE_SPENT) {
        Anm_StartScriptAtOrigin(&knife->vms[0], M_KNIFE_HIT_SCRIPT);
        box->damage_dealt = M_KNIFE_SPENT;
    }
    if (!GameManager_IsInPlayArea(knife->pos.x, knife->pos.y, 64.0f, 64.0f)) {
        knife->state = M_PART_FREE;
    }
    Anm_Step(&knife->vms[0]);
}

// Throws knives in all directions from where the bomb started, from frame
// 60 to frame 120.
static void M_SakuyaA(void)
{
    PLAYER *const player = &g_Player;
    if (M_Ends()) {
        return;
    }
    if (M_Starts()) {
        Item_CollectAll();
        M_ShowName();
        player->bomb.duration = 160;
        Timer_Set(&player->state.timer, 210);
        M_StartAura();
        player->bomb.start_pos = player->pos;
        for (int32_t i = 0; i < M_KNIFE_COUNT; i++) {
            player->bomb.parts[i].state = M_PART_FREE;
        }
        M_SetDrain(6000, 0.28f);
        M_SpawnKnifeEffect();
    }
    // Throws, moves and turns the knives on each call, not each frame.
    if (M_Timer()->current >= 60) {
        int32_t remaining = M_KNIVES_PER_CALL;
        for (int32_t i = 0; i < M_KNIFE_COUNT; i++) {
            if (player->bomb.parts[i].state != M_PART_FREE) {
                M_MoveKnife(i);
            } else if (M_Timer()->current <= 120 && remaining != 0) {
                M_ThrowKnife(i);
                remaining--;
            }
        }
    }
    M_Finish();
}

// Throws knife i from 24 out of the player, one of a ring of 96.
static void M_ThrowDollKnife(const int32_t i)
{
    PLAYER *const player = &g_Player;
    PLAYER_BOMB_PART *const knife = &player->bomb.parts[i];
    knife->state = M_PART_FLYING;
    Anm_StartScriptAtOrigin(&knife->vms[0], (i & 1) + M_DOLL_FIRST_SCRIPT);
    knife->angle = (float)i * MATH_TWO_PI / 96.0f - MATH_PI;
    knife->speed = Rng_F32(&g_Rng) * 1.0f + 0.5f;
    knife->speed_step = Rng_F32(&g_Rng) * 0.1f + 0.03f;
    // Draws below 1, so every knife spins the negative way.
    knife->acceleration.x =
        Rng_U16InRange(&g_Rng, 1) != 0 ? 0.15707964f : -0.15707964f;
    X87_SinCosScale(
        knife->angle, 24.0f, &knife->velocity.x, &knife->velocity.y);
    knife->pos = Vec3_Add(player->pos, knife->velocity);
    Timer_Set(&knife->timer, 0);
    knife->velocity.z = 0.0f;
    player->damage_boxes[i].damage_dealt = 0;
}

// Flies knife i out, spins it in place from its frame 30 to 70, then sends
// it at the homing target at speed 14. Its box acts until its first hit,
// which bursts it.
static void M_MoveDollKnife(const int32_t i)
{
    PLAYER_BOMB_PART *const knife = &g_Player.bomb.parts[i];
    PLAYER_DAMAGE_BOX *const box = &g_Player.damage_boxes[i];
    if (knife->timer.current < 30 || knife->timer.current >= 70) {
        if (M_TimerHasTickedTo(&knife->timer, 70)) {
            const VEC3 target = g_Player.homing_target;
            if (target.x > M_NO_TARGET) {
                knife->angle = Angle_AddNormalize(
                    X87_Atan2(target.y - knife->pos.y, target.x - knife->pos.x),
                    0.0f);
            }
            knife->speed = 14.0f;
        }
        knife->speed += knife->speed_step;
        X87_SinCosScale(
            knife->angle, knife->speed, &knife->velocity.x, &knife->velocity.y);
    } else {
        knife->angle = Angle_AddNormalize(knife->angle, knife->acceleration.x);
        knife->velocity.x = 0.0f;
        knife->velocity.y = 0.0f;
    }
    if (box->damage_dealt == 0) {
        knife->pos =
            Vec3_Add(knife->pos, Vec3_Scale(knife->velocity, g_GameSpeed));
        M_AddClearCircle(knife->pos, 32.0f);
        box->size.x = 24.0f;
        box->size.y = 24.0f;
        box->pos = knife->pos;
        box->damage = 22;
    } else if (box->damage_dealt < M_KNIFE_SPENT) {
        Anm_StartScriptAtOrigin(&knife->vms[0], M_KNIFE_HIT_SCRIPT);
        box->damage_dealt = M_KNIFE_SPENT;
        Effect_Spawn(EFFECT_EXPLOSION, knife->pos, 1, M_COLOR_KNIFE_HIT);
    }
    Anm_Step(&knife->vms[0]);
    Timer_TickAt(&knife->timer, g_GameSpeed);
}

// Throws a ring of 96 knives from the player, two every 2 frames from frame
// 20, and slows the player.
static void M_SakuyaAFocused(void)
{
    PLAYER *const player = &g_Player;
    const TIMER *const timer = M_Timer();
    if (M_Ends()) {
        M_Slow(1.0f);
        return;
    }
    if (M_Starts()) {
        Item_CollectAll();
        M_ShowName();
        player->bomb.duration = 250;
        Timer_Set(&player->state.timer, 290);
        M_StartAura();
        for (int32_t i = 0; i < M_KNIFE_COUNT; i++) {
            player->bomb.parts[i].state = M_PART_FREE;
        }
        M_SetDrain(6500, 0.29f);
        M_Slow(0.3f);
        M_SpawnKnifeEffect();
    }
    if (timer->current >= 0 && timer->current <= 60) {
        player->bomb.parts[0].effect->pos = player->pos;
    }
    if (timer->current >= 20 && timer->current < 116) {
        for (int32_t i = 0; i < M_KNIFE_COUNT; i++) {
            if (M_HasTickedTo(i % 48 * 2 + 20)) {
                M_ThrowDollKnife(i);
            }
        }
        // Restarts on each call, not each frame.
        Sound_Queue(SOUND_POWER_1);
        ScreenEffect_Shake(120, 4, 1);
    }
    for (int32_t i = 0; i < M_KNIFE_COUNT; i++) {
        if (player->bomb.parts[i].state != M_PART_FREE) {
            M_MoveDollKnife(i);
        }
    }
    M_Finish();
}

// Stops every enemy bullet at frames 0, 60 and 120, puts four sprites in
// the corners around the centre, hits the whole play area every 4 frames,
// and speeds the player up. Clears the play area as it ends.
static void M_SakuyaB(void)
{
    PLAYER *const player = &g_Player;
    const TIMER *const timer = M_Timer();
    if (M_Ends()) {
        M_Slow(1.0f);
        M_AddClearCircle(player->pos, 800.0f);
        return;
    }
    if (M_Starts()) {
        Item_CollectAll();
        M_ShowName();
        player->bomb.duration = 160;
        Timer_Set(&player->state.timer, 260);
        M_StartAura();
        for (int32_t i = 0; i < M_SQUARE_COUNT; i++) {
            player->bomb.parts[i].state = M_PART_FREE;
        }
        Sound_Queue(SOUND_SPARK);
        M_SetDrain(5500, 0.26f);
        M_Slow(2.0f);
        Bullet_StopAll();
    }
    if (M_HasTickedTo(60)) {
        Bullet_StopAll();
    }
    if (M_HasTickedTo(120)) {
        Bullet_StopAll();
    }
    if (M_HasTickedTo(30)) {
        for (int32_t i = 0; i < M_SQUARE_COUNT; i++) {
            PLAYER_BOMB_PART *const part = &player->bomb.parts[i];
            ANM_VM *const vm = &part->vms[0];
            part->state = M_PART_FLYING;
            Anm_StartScriptAtOrigin(vm, i + M_SQUARE_FIRST_SCRIPT);
            vm->pos.x = 192.0f + ((i & 1) != 0 ? 128.0f : -128.0f);
            vm->pos.y = 224.0f + (i / 2 != 0 ? 128.0f : -128.0f);
            vm->pos.z = 0.49f;
            part->velocity.z = 0.0f;
        }
    }
    // Acts on the frames that are multiples of 4, unlike the other bombs.
    if (timer->current >= 30 && Timer_HasTicked(timer)
        && timer->current % 4 == 0) {
        PLAYER_DAMAGE_BOX *const box = &player->damage_boxes[0];
        box->pos.x = 192.0f;
        box->pos.y = 224.0f;
        box->size.x = 352.0f;
        box->size.y = 416.0f;
        box->damage = 3;
    }
    if (M_HasTickedTo(40)) {
        ScreenEffect_Shake(60, 1, 7);
    }
    if (M_HasTickedTo(100)) {
        ScreenEffect_Shake(70, 24, 0);
    }
    for (int32_t i = 0; i < M_SQUARE_COUNT; i++) {
        if (player->bomb.parts[i].state != M_PART_FREE) {
            Anm_Step(&player->bomb.parts[i].vms[0]);
        }
    }
    M_Finish();
}

// Sends two sprites out from the player that are pulled back to it, each
// trailing its last 32 positions. Stops every enemy bullet at frames 40 and
// 100, speeds the player up, and clears the play area as it ends.
static void M_SakuyaBFocused(void)
{
    PLAYER *const player = &g_Player;
    PLAYER_BOMB_PART *const first = &player->bomb.parts[0];
    if (M_Ends()) {
        M_Slow(1.0f);
        M_AddClearCircle(player->pos, 800.0f);
        // Overwrites the first clear area, whichever bomb set it.
        PLAYER_CLEAR_AREA *const area = &player->clear_areas[0];
        area->x = 192.0f;
        area->y = 224.0f;
        area->width = 448.0f;
        area->height = 512.0f;
        return;
    }
    if (M_Starts()) {
        Item_CollectAll();
        M_ShowName();
        player->bomb.duration = 300;
        Timer_Set(&player->state.timer, 420);
        M_StartAura();
        player->state.bomb_started = false;
        for (int32_t i = 0; i < M_PRIVATE_SQUARE_COUNT; i++) {
            PLAYER_BOMB_PART *const part = &player->bomb.parts[i];
            part->state = M_PART_FLYING;
            Anm_StartScriptAtOrigin(
                &part->vms[0], i + M_PRIVATE_SQUARE_FIRST_SCRIPT);
            part->pos = player->pos;
            for (int32_t k = PLAYER_BOMB_PART_TRAIL_COUNT - 1; k >= 0; k--) {
                part->trail[k] = part->pos;
            }
            part->velocity = (VEC3) {};
            part->acceleration = (VEC3) { 0.0f, -0.008f, 0.0f };
        }
        M_Slow(1.5f);
        Sound_Queue(SOUND_SPARK);
        M_SetDrain(6000, 0.29f);
    }
    // Acts on each call, not each frame.
    M_AddClearCircle(first->pos, 96.0f);
    PLAYER_DAMAGE_BOX *const box = &player->damage_boxes[0];
    box->pos.x = first->pos.x;
    box->pos.y = first->pos.y;
    box->size.x = 160.0f;
    box->size.y = 160.0f;
    box->damage = 1;
    if (M_HasTickedTo(40)) {
        ScreenEffect_Shake(60, 1, 7);
        Bullet_StopAll();
    }
    if (M_HasTickedTo(100)) {
        Bullet_StopAll();
        ScreenEffect_Shake(70, 24, 0);
    }
    for (int32_t i = 0; i < M_PRIVATE_SQUARE_COUNT; i++) {
        PLAYER_BOMB_PART *const part = &player->bomb.parts[i];
        if (part->state == M_PART_FREE) {
            continue;
        }
        part->acceleration = Vec3_Scale(
            Vec3_Scale(Vec3_Sub(player->pos, part->pos), 1.0f / 1700.0f),
            g_GameSpeed);
        part->velocity = Vec3_Add(
            part->velocity, Vec3_Scale(part->acceleration, g_GameSpeed));
        for (int32_t k = PLAYER_BOMB_PART_TRAIL_COUNT - 1; k > 0; k--) {
            part->trail[k] = part->trail[k - 1];
        }
        part->trail[0] = part->pos;
        part->pos =
            Vec3_Add(part->pos, Vec3_Scale(part->velocity, g_GameSpeed));
        Anm_Step(&part->vms[0]);
    }
    M_Finish();
}

void Bomb_Update(void)
{
    switch (M_Index()) {
    case 0:
        M_ReimuA();
        break;
    case 1:
        M_ReimuAFocused();
        break;
    case 2:
        M_ReimuB();
        break;
    case 3:
        M_ReimuBFocused();
        break;
    case 4:
        M_MarisaA();
        break;
    case 5:
        M_MarisaAFocused();
        break;
    case 6:
        M_MarisaB();
        break;
    case 7:
        M_MarisaBFocused();
        break;
    case 8:
        M_SakuyaA();
        break;
    case 9:
        M_SakuyaAFocused();
        break;
    case 10:
        M_SakuyaB();
        break;
    case 11:
        M_SakuyaBFocused();
        break;
    default:
        break;
    }
}
