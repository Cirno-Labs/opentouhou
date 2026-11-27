#include "th07/enemy.h"

#include "th07/ascii.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/gui.h"
#include "th07/item.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/sound.h"

#include "core/base/assert.h"
#include "core/base/color.h"
#include "core/math/x87.h"
#include "stg/angle.h"
#include "stg/chain.h"
#include "stg/rng.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define M_ANM_SLOT 15
#define M_FIRST_SPRITE 0x900
#define M_CALC_PRIORITY 10
#define M_DRAW_PRIORITY_LOW 5
#define M_DRAW_PRIORITY_HIGH 7
#define M_STAGE_COUNT 8
#define M_RANDOM_POS -990.0f
#define M_RANDOM_DEPTH 800.0f
#define M_NOWHERE -999.0f
#define M_RANK_PERIOD 2400
#define M_RANK_PERIOD_PER_LIFE 240
#define M_RANK_STEP 100
#define M_HURT_CAP 70
#define M_CHERRY_CAP 70
#define M_CONTACT_DAMAGE 10
#define M_GRAZE_PERIOD 6
#define M_BOSS_CLEAR_BONUS 8000
#define M_CLEAR_BONUS_START 2000
#define M_CLEAR_BONUS_STEP 30
#define M_ORBIT_GROWTH 0.3f
#define M_ORBIT_TURN 0.031415928f
#define M_YELLOW COLOR_ARGB(0xff, 0xff, 0xff, 0x00)
#define M_FLASH_COLOR 0xff80c0u
#define M_DROP_COUNT 0x20
#define M_MARKER_OFFSET 32.0f
#define M_MARKER_Y 472.0f
#define M_TRAIL_GONE -990.0f
#define M_AIM_MIN -2.0943952f
#define M_AIM_MAX -1.0471976f
#define M_AIM_NONE -900.0f
#define M_FULL_TURN 6.2831855f
#define M_STRIP_ANGLE_EPSILON 0.00001f
#define M_AIMING_CHARACTER 2

// The spawn opcodes of the timelines combine three bits.
#define M_SPAWN_DEFAULT 1
#define M_SPAWN_MIRRORED 2
#define M_SPAWN_RANDOM 4

ENEMY_MANAGER g_Enemies;

static const uint8_t m_ItemDrops[M_DROP_COUNT] = {
    0, 0, 1, 0, 1, 0, 0, 7, 1, 1, 0, 0, 7, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 7, 1, 1, 1, 0, 2,
};

static ENEMY m_Template;
// Is returned by the spawns when every slot is in use. The update never
// runs it, but the timelines still mark it mirrored.
static ENEMY m_Spare;
static CHAIN_ELEM m_CalcElem;
static CHAIN_ELEM m_DrawLowElem;
static CHAIN_ELEM m_DrawHighElem;

static void M_Reset(void)
{
    memset(&g_Enemies, 0, sizeof(g_Enemies));
    memset(&m_Spare, 0, sizeof(m_Spare));
    memset(&m_Template, 0, sizeof(m_Template));
    ENEMY *const enemy = &m_Template;
    for (int32_t i = 0; i < ENEMY_SPRITE_COUNT; i++) {
        enemy->vms[i].script_index = -1;
    }
    for (int32_t i = 0; i < ENEMY_TRAIL_COUNT; i++) {
        enemy->trail.boxes[i].pos.x = M_NOWHERE;
    }
    enemy->script.state.frame_func = ECL_FUNC_NONE;
    enemy->flags.active = true;
    Timer_Set(&enemy->callbacks.timer, 0);
    enemy->hitbox_size = (VEC3) { 12.0f, 12.0f, 12.0f };
    enemy->life = 1;
    enemy->score = 100;
    Timer_Set(&enemy->shots.timer, 0);
    enemy->turns.left = -1;
    enemy->turns.right = -1;
    enemy->turns.still = -1;
    enemy->flags.shootable = true;
    enemy->flags.contact_damage = true;
    enemy->flags.takes_damage = true;
    enemy->flags.hit_by_shots = true;
    enemy->flags.death = ENEMY_DEATH_REMOVE;
    enemy->script.death_sub = -1;
    enemy->script.pending_interrupt = -1;
    for (int32_t i = 0; i < ENEMY_LIFE_CALLBACK_COUNT; i++) {
        enemy->callbacks.life[i] = -1;
    }
    enemy->callbacks.time = -1;
    enemy->script.periodic.sub = -1;
    enemy->shots.rank_speed_low = -0.15f;
    enemy->shots.rank_speed_high = 0.15f;
    enemy->shots.shooter.sound = SOUND_SHOT_0;
    enemy->shots.shooter.bullet_sound = SOUND_KIRA_0;
}

static void M_ResetRankGrowth(ENEMY *const enemy)
{
    enemy->shots.rank_speed_low = -0.5f;
    enemy->shots.rank_speed_high = 0.5f;
    enemy->shots.rank_columns_low = 0;
    enemy->shots.rank_columns_high = 0;
    enemy->shots.rank_rows_low = 0;
    enemy->shots.rank_rows_high = 0;
}

// Resets what a callback or a death subroutine starts from: no periodic
// call, the shooter of a new enemy, the rank growth and the call stack.
static void M_ResetScript(ENEMY *const enemy)
{
    enemy->script.periodic.sub = -1;
    enemy->shots.shooter = m_Template.shots.shooter;
    enemy->shots.interval = 0;
    M_ResetRankGrowth(enemy);
    enemy->script.depth = 0;
}

static void M_ClearCallbacks(ENEMY *const enemy)
{
    for (int32_t i = 0; i < ENEMY_LIFE_CALLBACK_COUNT; i++) {
        enemy->callbacks.life[i] = -1;
    }
    enemy->callbacks.time = -1;
    enemy->script.periodic.sub = -1;
}

static void M_StartDeathSub(ENEMY *const enemy)
{
    Ecl_StartSub(&enemy->script.state, (int16_t)enemy->script.death_sub);
    enemy->script.death_sub = -1;
}

static void M_KillAll(void)
{
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        ENEMY *const enemy = &g_Enemies.enemies[i];
        if (!enemy->flags.active || enemy->flags.boss) {
            continue;
        }
        enemy->life = 0;
        if (!enemy->flags.shootable && enemy->script.death_sub >= 0) {
            M_StartDeathSub(enemy);
        }
    }
}

static VEC3 M_RandomPos(VEC3 pos)
{
    if (pos.x <= M_RANDOM_POS) {
        pos.x = Rng_F32InRange(&g_Rng, g_GameManager.player_area.width);
    }
    if (pos.y <= M_RANDOM_POS) {
        pos.y = Rng_F32InRange(&g_Rng, g_GameManager.player_area.height);
    }
    if (pos.z <= M_RANDOM_POS) {
        pos.z = Rng_F32InRange(&g_Rng, M_RANDOM_DEPTH);
    }
    return pos;
}

static void M_SpawnEntry(const ECL_TIMELINE_ENTRY *const entry)
{
    const int32_t bits = entry->opcode;
    const bool random = (bits & M_SPAWN_RANDOM) != 0;
    const bool mirrored = (bits & M_SPAWN_MIRRORED) != 0;
    VEC3 pos = entry->args.spawn.pos;
    if (random) {
        pos = M_RandomPos(pos);
    }
    // The random mirrored spawns mirror after the first frame.
    const bool mirror_first = mirrored && !random;
    ENEMY *const enemy = (bits & M_SPAWN_DEFAULT) != 0
        ? Enemy_Spawn(entry->arg, pos, -1, -1, -1, mirror_first)
        : Enemy_Spawn(
              entry->arg, pos, entry->args.spawn.life, entry->args.spawn.item,
              entry->args.spawn.score, mirror_first);
    if (mirrored && random) {
        enemy->flags.mirrored = true;
    }
}

static ENEMY *M_GetBoss(const int32_t index)
{
    ASSERT(index >= 0 && index < ENEMY_BOSS_COUNT);
    return g_Enemies.bosses[index];
}

// Runs one entry of a timeline on its frame. Returns whether the timeline
// waits at the entry.
static bool M_RunEntry(const ECL_TIMELINE_ENTRY *const entry)
{
    switch ((ECL_TIMELINE_OP)entry->opcode) {
    case ECL_TIMELINE_SPAWN:
    case ECL_TIMELINE_SPAWN_DEFAULT:
    case ECL_TIMELINE_SPAWN_MIRRORED:
    case ECL_TIMELINE_SPAWN_MIRRORED_DEFAULT:
    case ECL_TIMELINE_SPAWN_RANDOM:
    case ECL_TIMELINE_SPAWN_RANDOM_DEFAULT:
    case ECL_TIMELINE_SPAWN_RANDOM_MIRRORED:
    case ECL_TIMELINE_SPAWN_RANDOM_MIRRORED_DEFAULT:
        if (!g_Gui.boss.present) {
            M_SpawnEntry(entry);
        }
        return false;
    case ECL_TIMELINE_START_MSG:
        Gui_StartMsg(entry->arg + g_GameManager.run.character * 10);
        return false;
    case ECL_TIMELINE_WAIT_MSG:
        return Gui_MsgHoldsTimeline();
    case ECL_TIMELINE_INTERRUPT_BOSS: {
        ENEMY *const boss = M_GetBoss(entry->args.interrupt.boss);
        ASSERT(boss != nullptr);
        boss->script.pending_interrupt = entry->args.interrupt.interrupt;
        return false;
    }
    case ECL_TIMELINE_SET_POWER:
        g_GameManager.player.power = (float)entry->arg;
        GameManager_DrawChecksumKeys();
        return false;
    case ECL_TIMELINE_WAIT_BOSS: {
        const ENEMY *const boss = M_GetBoss(entry->arg);
        return boss != nullptr && boss->flags.active;
    }
    }
    return false;
}

// Runs the entries of timeline index due on this frame and skips the ones
// whose frame has passed. An entry that waits holds the frame count back,
// so the entry runs again on the next frame.
static void M_RunTimeline(const int32_t index)
{
    const ECL_TIMELINE_ENTRY *const timeline = Ecl_GetTimeline(index);
    TIMER *const time = &g_Enemies.timelines[index].time;
    for (;;) {
        const ECL_TIMELINE_ENTRY *const entry =
            &timeline[g_Enemies.timelines[index].index];
        if (entry->time < 0) {
            break;
        }
        if (time->current == entry->time) {
            if (M_RunEntry(entry)) {
                Timer_SubAt(time, 1, g_GameSpeed);
                break;
            }
        } else if (time->current < entry->time) {
            break;
        }
        g_Enemies.timelines[index].index++;
    }
    Timer_TickAt(time, g_GameSpeed);
}

static void M_Move(ENEMY *const enemy)
{
    VEC3 *const pos = &enemy->move.pos;
    enemy->move.step = Vec3_Sub(*pos, enemy->move.prev_pos);
    enemy->move.prev_pos = *pos;
    if (!enemy->flags.mirrored) {
        pos->x += g_GameSpeed * enemy->move.velocity.x;
    } else {
        pos->x -= g_GameSpeed * enemy->move.velocity.x;
    }
    pos->y += g_GameSpeed * enemy->move.velocity.y;
    pos->z += g_GameSpeed * enemy->move.velocity.z;
}

static void M_Despawn(ENEMY *const enemy)
{
    if (enemy->flags.death == ENEMY_DEATH_REMOVE) {
        enemy->flags.active = false;
    } else {
        enemy->flags.shootable = false;
    }
    if (enemy->flags.boss && enemy->boss_index < 4) {
        g_Gui.boss.present = false;
    }
    if (enemy->orbits.count != 0) {
        Enemy_ReleaseOrbits(enemy);
    }
    if (enemy->flags.boss) {
        g_Enemies.bosses[enemy->boss_index] = nullptr;
    }
    Replay_MarkEvent(REPLAY_EVENT_ENEMY_GONE);
}

// Returns whether a life callback started.
static bool M_CheckLifeCallback(ENEMY *const enemy)
{
    for (int32_t i = 0; i < ENEMY_LIFE_CALLBACK_COUNT; i++) {
        const int32_t life = enemy->callbacks.life[i];
        if (life < 0 || enemy->life >= life) {
            continue;
        }
        enemy->life = life;
        Ecl_StartSub(
            &enemy->script.state, (int16_t)enemy->callbacks.life_sub[i]);
        enemy->callbacks.life[i] = -1;
        enemy->callbacks.time = -1;
        M_ResetScript(enemy);
        M_KillAll();
        return true;
    }
    return false;
}

// Returns whether the timer callback started.
static bool M_CheckTimeCallback(ENEMY *const enemy)
{
    if (enemy->flags.boss && enemy->boss_index == 0) {
        g_Gui.boss.seconds =
            (enemy->callbacks.time - enemy->callbacks.timer.current) / 60;
    }
    if (enemy->callbacks.timer.current < enemy->callbacks.time) {
        return false;
    }
    int32_t best = 0;
    int32_t best_index = 0;
    for (int32_t i = 0; i < ENEMY_LIFE_CALLBACK_COUNT; i++) {
        if (enemy->callbacks.life[i] >= 0 && best < enemy->callbacks.life[i]) {
            best = enemy->callbacks.life[i];
            best_index = i;
        }
    }
    if (best > 0) {
        enemy->life = best;
        enemy->callbacks.life[best_index] = -1;
    }
    Ecl_StartSub(&enemy->script.state, (int16_t)enemy->callbacks.time_sub);
    enemy->callbacks.time = -1;
    enemy->callbacks.time_sub = enemy->script.death_sub;
    Timer_Set(&enemy->callbacks.timer, 0);
    if (!enemy->flags.keep_spell_card) {
        g_Enemies.spell_card.bonus = 0;
        g_Enemies.spell_card.capturable = false;
        // Counts up rather than setting ENEMY_SPELL_TIMED_OUT, so a second
        // timeout in one spell card goes past it.
        if (g_Enemies.spell_card.state != ENEMY_SPELL_NONE) {
            g_Enemies.spell_card.state =
                (ENEMY_SPELL)(g_Enemies.spell_card.state + 1);
        }
        Bullet_ClearAllAndKeptLasers();
        // Takes a quarter of the cherry, less its last digit, from the
        // value with its offset.
        const int32_t cherry =
            g_GameManager.cherry.value - g_GameManager.cherry.offset;
        int32_t lost = X87_Ftol((float)cherry * 0.25f);
        lost -= lost % 10;
        g_GameManager.cherry.value -= lost;
    }
    M_KillAll();
    M_ResetScript(enemy);
    return true;
}

static void M_CheckPlayerContact(
    ENEMY *const enemy, const VEC3 pos, const VEC3 size)
{
    if (enemy->flags.grazeable && Timer_HasTicked(&enemy->callbacks.timer)
        && enemy->callbacks.timer.current % M_GRAZE_PERIOD == 0) {
        Player_CheckGraze(pos, Vec3_Scale(size, 1.0f / 0.7f));
    }
    if (Player_CheckHit(pos, Vec3_Scale(size, 1.0f / 1.5f))
            == PLAYER_CONTACT_BOX
        && enemy->flags.shootable && !enemy->flags.boss
        && !enemy->flags.grazeable) {
        enemy->life -= M_CONTACT_DAMAGE;
    }
}

static void M_CheckContacts(ENEMY *const enemy)
{
    M_CheckPlayerContact(enemy, enemy->move.pos, enemy->hitbox_size);
    if (enemy->trail.flags == 0) {
        return;
    }
    VEC3 size = enemy->hitbox_size;
    for (int32_t i = 1; i < enemy->trail.contact_count; i += M_GRAZE_PERIOD) {
        if ((enemy->trail.flags & ENEMY_TRAIL_NARROW) != 0) {
            size = Vec3_Sub(
                enemy->hitbox_size,
                Vec3_Scale(
                    Vec3_Scale(enemy->hitbox_size, (float)i),
                    1.0f / (float)enemy->trail.contact_count));
        }
        M_CheckPlayerContact(enemy, enemy->trail.boxes[i].pos, size);
    }
}

// Returns the cherry points that damage hurt scores: by the stage, more on
// a boss while unfocused.
static int32_t M_CherryPoints(const ENEMY *const enemy, const int32_t hurt)
{
    const int32_t stage = g_GameManager.run.stage;
    const int32_t level = stage >= 5 ? 10 : stage * 2;
    const bool focused = g_Player.move.focused;
    int32_t points = enemy->flags.boss && !focused
        ? hurt / (10 - level / 3) * 10
        : hurt / (30 - level) * 10;
    if (points > M_CHERRY_CAP) {
        points = M_CHERRY_CAP;
    }
    if (points == 0
        && (!focused || (enemy->callbacks.timer.current & 1) != 0)) {
        points = 10;
    }
    return points;
}

// Cuts the damage of shot index 0 on stages 4 to 6 for enemies that are
// not bosses.
static int32_t M_CutShotDamage(const ENEMY *const enemy, int32_t hurt)
{
    const int32_t stage = g_GameManager.run.stage;
    if (stage >= 5 && stage <= 6 && !enemy->flags.boss) {
        hurt /= 2;
    }
    if (stage == 4 && !enemy->flags.boss) {
        hurt -= hurt / 4 + hurt / 16;
    }
    return hurt;
}

// Divides the damage of a frame during a spell card: a shot deals a
// seventh, a bomb its damage / 2.5 once the player bombed during it and
// nothing before. Damage that the division takes to 0 deals 1.
static int32_t M_SpellCardDamage(const int32_t hurt, const bool by_bomb)
{
    if (!by_bomb) {
        if (hurt > 7) {
            return hurt / 7;
        }
        return hurt != 0 ? 1 : 0;
    }
    if (!g_Enemies.spell_card.bombed) {
        return 0;
    }
    if (hurt > 2) {
        return X87_Ftol((float)hurt / 2.5f);
    }
    return hurt != 0 ? 1 : 0;
}

static void M_ApplyDamage(ENEMY *const enemy, int32_t hurt, const bool by_bomb)
{
    if ((enemy->flags.boss || !g_Player.move.focused)
        && !g_Player.bomb.running) {
        int32_t points = M_CherryPoints(enemy, hurt);
        if (g_GameManager.run.shot_index == 0) {
            // Odd frames take 20 and 30 points down to 10 and 20.
            if ((points == 20 || points == 30)
                && (enemy->callbacks.timer.current & 1) != 0) {
                points -= 10;
            }
            hurt = M_CutShotDamage(enemy, hurt);
        }
        if (points != 0) {
            GameManager_AddCherry(points);
        }
    }
    if (hurt >= M_HURT_CAP) {
        hurt = M_HURT_CAP;
    }
    GameManager_AddScore(hurt / 5 * 10);
    if (!enemy->flags.takes_damage) {
        return;
    }
    if (g_Enemies.spell_card.state != ENEMY_SPELL_NONE) {
        hurt = M_SpellCardDamage(hurt, by_bomb);
    }
    if (enemy->invulnerable_timer.current > 0) {
        hurt = enemy->flags.boss ? hurt / 9 : 0;
    }
    enemy->life -= hurt;
    enemy->last_damage = hurt;
}

static float M_AngleFromPlayer(const VEC3 pos)
{
    return X87_Atan2(pos.y - g_Player.pos.y, pos.x - g_Player.pos.x);
}

static bool M_InAimWindow(const VEC3 pos)
{
    const float angle = M_AngleFromPlayer(pos);
    return angle >= M_AIM_MIN && angle <= M_AIM_MAX;
}

// Sets the homing and aim targets of the player: a boss nearer the player
// in x than the target, else the lowest enemy. Character 2 aims only at
// enemies 60 to 120 degrees above the player.
static void M_UpdateTargets(const ENEMY *const enemy)
{
    const VEC3 pos = enemy->move.pos;
    if (enemy->flags.boss) {
        const VEC3 toward = Vec3_Sub(pos, g_Player.pos);
        const VEC3 homing = Vec3_Sub(g_Player.homing_target, g_Player.pos);
        if (!g_Player.target_found || fabsf(homing.x) > fabsf(toward.x)) {
            g_Player.homing_target = pos;
        }
        if (g_GameManager.run.character == M_AIMING_CHARACTER) {
            const VEC3 aim = Vec3_Sub(g_Player.aim_target, g_Player.pos);
            if (M_InAimWindow(pos)
                && (!g_Player.target_found || fabsf(aim.x) > fabsf(toward.x))) {
                g_Player.aim_target = pos;
                g_Player.target_found = true;
            }
        } else {
            g_Player.target_found = true;
        }
    }
    if (g_Player.target_found) {
        return;
    }
    if (g_Player.homing_target.y < pos.y) {
        g_Player.homing_target = pos;
    }
    if (g_GameManager.run.character == M_AIMING_CHARACTER
        && g_Player.aim_target.y < M_AIM_NONE && M_InAimWindow(pos)) {
        g_Player.aim_target = pos;
    }
}

// Applies the contact with the player and the damage of the shots and the
// bomb, and sets the targets. Returns whether the enemy took damage, and
// sets *by_bomb to whether the bomb hit it.
static bool M_TakeDamage(ENEMY *const enemy, bool *const by_bomb)
{
    if (enemy->flags.shootable && enemy->flags.contact_damage) {
        M_CheckContacts(enemy);
    }
    enemy->last_damage = 0;
    if (!enemy->flags.shootable || !enemy->flags.hit_by_shots) {
        return false;
    }
    bool hit = false;
    int32_t hurt =
        Player_CalcDamage(enemy->move.pos, enemy->hitbox_size, by_bomb);
    if (enemy->extra_hitbox_size.x > 0.0f) {
        // The second test sets *by_bomb again.
        const int32_t extra = Player_CalcDamage(
            enemy->move.pos, enemy->extra_hitbox_size, by_bomb);
        if (!*by_bomb) {
            hurt = X87_Ftol((float)hurt + (float)extra / 2.5f);
        }
    }
    if (hurt > 0) {
        M_ApplyDamage(enemy, hurt, *by_bomb);
        hit = true;
    }
    M_UpdateTargets(enemy);
    return hit;
}

static void M_DropItem(const ENEMY *const enemy, const bool by_bomb)
{
    const ITEM_STATE state = by_bomb ? ITEM_HOMING : ITEM_FALLING;
    const EFFECT_TYPE particle =
        (EFFECT_TYPE)(EFFECT_PARTICLE + enemy->death.particle_color);
    if (enemy->death.item >= 0) {
        Effect_Spawn(particle, enemy->move.pos, 3, COLOR_WHITE);
        Item_Spawn(enemy->move.pos, (ITEM_TYPE)enemy->death.item, state);
    } else if (enemy->death.item == -1) {
        if (g_Enemies.drops.counter % 3 == 0) {
            Effect_Spawn(particle, enemy->move.pos, 6, COLOR_WHITE);
            Item_Spawn(
                enemy->move.pos, (ITEM_TYPE)m_ItemDrops[g_Enemies.drops.index],
                state);
            g_Enemies.drops.index++;
            if (g_Enemies.drops.index >= M_DROP_COUNT) {
                g_Enemies.drops.index = 0;
            }
        }
        g_Enemies.drops.counter++;
    }
}

static void M_SpawnDeathEffect(const ENEMY *const enemy)
{
    Effect_Spawn(
        (EFFECT_TYPE)enemy->death.effect, enemy->move.pos, 1, COLOR_WHITE);
}

static void M_Die(ENEMY *const enemy, const int32_t slot, const bool by_bomb)
{
    M_ClearCallbacks(enemy);
    switch (enemy->flags.death) {
    case ENEMY_DEATH_SURVIVE:
        enemy->life = 1;
        enemy->flags.takes_damage = false;
        enemy->flags.death = ENEMY_DEATH_REMOVE;
        g_Gui.boss.present = false;
        Replay_MarkEvent(REPLAY_EVENT_ENEMY_GONE);
        // Spawns the effect three times here, and once more below.
        if (enemy->death.effect >= 0) {
            for (int32_t i = 0; i < 3; i++) {
                M_SpawnDeathEffect(enemy);
            }
        }
        break;
    case ENEMY_DEATH_REMOVE:
    case ENEMY_DEATH_KEEP:
        GameManager_AddScore(enemy->score);
        if (enemy->flags.death == ENEMY_DEATH_KEEP) {
            enemy->flags.shootable = false;
        } else {
            enemy->flags.active = false;
        }
        if (enemy->flags.boss) {
            g_Gui.boss.present = false;
            Enemy_ReleaseOrbits(enemy);
        }
        [[fallthrough]];
    case ENEMY_DEATH_DROP:
        M_DropItem(enemy, by_bomb);
        if (enemy->flags.boss
            && g_Enemies.spell_card.state == ENEMY_SPELL_NONE) {
            int32_t bonus = Bullet_ClearAllWithBonus(M_BOSS_CLEAR_BONUS, true);
            bonus = Enemy_ClearForBonus(M_BOSS_CLEAR_BONUS, bonus);
            if (bonus != 0) {
                GameManager_AddScore(bonus);
                Gui_ShowBonusPopup(bonus);
            }
        }
        enemy->life = 0;
        Replay_MarkEvent(REPLAY_EVENT_ENEMY_GONE);
        break;
    default:
        break;
    }
    Sound_Queue(slot % 2 + SOUND_ENEMY_DEATH_0);
    if (enemy->death.effect >= 0) {
        M_SpawnDeathEffect(enemy);
        Effect_Spawn(
            (EFFECT_TYPE)(EFFECT_PARTICLE + enemy->death.particle_color),
            enemy->move.pos, 4, COLOR_WHITE);
    }
    if (enemy->script.death_sub >= 0) {
        M_ClearCallbacks(enemy);
        M_ResetScript(enemy);
        M_StartDeathSub(enemy);
    }
}

static void M_UpdateHitFlash(ENEMY *const enemy, const bool hit)
{
    ANM_VM *const vm = &enemy->vm;
    if (enemy->hit_flash_frames != 0) {
        enemy->hit_flash_frames--;
        vm->use_color2 = false;
    } else if (hit) {
        Sound_Queue(SOUND_DAMAGE);
        vm->color2 = (vm->color & 0xff000000u) | M_FLASH_COLOR;
        vm->use_color2 = true;
        enemy->hit_flash_frames = 1;
    } else {
        vm->use_color2 = false;
    }
}

static void M_UpdateBoss(const ENEMY *const enemy)
{
    if (!Gui_IsMsgRunning() && enemy->boss_index == 0) {
        g_Gui.boss.health = (float)enemy->life / (float)enemy->max_life;
    }
    // The original also writes the markers of bosses 4 to 7, past its
    // four.
    if (enemy->boss_index >= ASCII_BOSS_MARKER_COUNT) {
        return;
    }
    ANM_VM *const marker = &g_AsciiText.boss_markers[enemy->boss_index];
    marker->pos = (VEC3) {
        enemy->flags.hidden ? M_NOWHERE : enemy->move.pos.x + M_MARKER_OFFSET,
        M_MARKER_Y,
        0.0f,
    };
    g_AsciiText.boss_marker_blue[enemy->boss_index] = enemy->vm.use_color2;
}

static void M_MoveOrbits(ENEMY *const enemy)
{
    for (int32_t i = 0; i < enemy->orbits.count; i++) {
        EFFECT *const effect = enemy->orbits.effects[i];
        if (effect == nullptr) {
            continue;
        }
        effect->vm.visible = !enemy->flags.hidden;
        effect->origin = enemy->move.pos;
        if (effect->radius < enemy->orbits.radius) {
            effect->radius += M_ORBIT_GROWTH;
        }
        effect->angle = Angle_AddNormalize(effect->angle, M_ORBIT_TURN);
    }
}

static bool M_IsInPlayArea(const ENEMY *const enemy, const VEC3 pos)
{
    const ANM_SPRITE *const sprite = enemy->vm.sprite_data;
    ASSERT(sprite != nullptr);
    return GameManager_IsInPlayArea(
        pos.x, pos.y, sprite->width, sprite->height);
}

// Returns whether the enemy left the play area after it came on screen:
// with a trail, only once the end of the trail left it too.
static bool M_HasLeft(const ENEMY *const enemy)
{
    if (!enemy->flags.on_screen || enemy->flags.stays_off_screen
        || M_IsInPlayArea(enemy, enemy->move.pos)) {
        return false;
    }
    if (enemy->trail.flags == 0) {
        return true;
    }
    ASSERT(enemy->trail.count > 0);
    return !M_IsInPlayArea(
        enemy, enemy->trail.boxes[enemy->trail.count - 1].pos);
}

static void M_MoveTrail(ENEMY *const enemy)
{
    ENEMY_TRAIL_BOX *const boxes = enemy->trail.boxes;
    for (int32_t i = enemy->trail.count - 1; i > 0; i--) {
        boxes[i] = boxes[i - 1];
    }
    boxes[0] = (ENEMY_TRAIL_BOX) { enemy->move.pos, enemy->move.velocity,
                                   enemy->move.angle };
}

// Runs the script and moves the enemy until no callback starts. Returns
// false once the enemy is gone.
static bool M_RunScript(ENEMY *const enemy)
{
    do {
        if (Ecl_RunEnemy(enemy) == ECL_RUN_DELETE) {
            enemy->flags.active = false;
            M_Despawn(enemy);
            return false;
        }
        if (!enemy->flags.warping_to_boss) {
            Enemy_ClampPos(enemy);
            M_Move(enemy);
            Enemy_ClampPos(enemy);
            EFFECT *const circle = enemy->orbits.effects[ENEMY_SPELL_CIRCLE];
            if (circle != nullptr && !enemy->flags.spell_circle_detached) {
                circle->pos = Vec3_Add(
                    Vec3_Scale(
                        Vec3_Sub(enemy->move.pos, circle->pos), 1.0f / 16.0f),
                    circle->pos);
            }
        }
        if (enemy->trail.flags != 0) {
            M_MoveTrail(enemy);
        }
        if (enemy->vm.sprite_data == nullptr) {
            enemy->flags.hidden = true;
        }
        if (!enemy->flags.hidden && !enemy->flags.on_screen
            && M_IsInPlayArea(enemy, enemy->move.pos)) {
            enemy->flags.on_screen = true;
        }
        if (M_HasLeft(enemy)) {
            enemy->flags.active = false;
            M_Despawn(enemy);
            return false;
        }
    } while (M_CheckLifeCallback(enemy)
             || (enemy->callbacks.time >= 0 && M_CheckTimeCallback(enemy)));
    return true;
}

static void M_StepSprites(ENEMY *const enemy)
{
    enemy->vm.color = enemy->color;
    Anm_Step(&enemy->vm);
    enemy->color = enemy->vm.color;
    for (int32_t i = 0; i < ENEMY_SPRITE_COUNT; i++) {
        ANM_VM *const vm = &enemy->vms[i];
        if (vm->script_index >= 0 && Anm_Step(vm)) {
            vm->script_index = -1;
        }
    }
}

// Updates enemy in slot. *hit carries over from the enemy before.
static void M_UpdateEnemy(ENEMY *const enemy, const int32_t slot, bool *hit)
{
    // A halted enemy flashes by the hit of the enemy updated before it.
    if (enemy->flags.halted_by_bomb
        && (g_Player.bomb.running || g_Player.state.value != PLAYER_ALIVE)) {
        Timer_SubAt(&enemy->callbacks.timer, 1, g_GameSpeed);
    } else {
        if (!M_RunScript(enemy)) {
            return;
        }
        M_StepSprites(enemy);
        bool by_bomb = false;
        *hit = false;
        if (!enemy->flags.hidden && !enemy->flags.hidden_by_bomb) {
            *hit = M_TakeDamage(enemy, &by_bomb);
        }
        if (enemy->life <= 0 && enemy->flags.shootable) {
            M_Die(enemy, slot, by_bomb);
        }
    }
    M_UpdateHitFlash(enemy, *hit);
    if (enemy->flags.boss) {
        M_UpdateBoss(enemy);
    }
    M_MoveOrbits(enemy);
    Timer_TickAt(&enemy->callbacks.timer, g_GameSpeed);
    if (enemy->invulnerable_timer.current > 0) {
        Timer_SubAt(&enemy->invulnerable_timer, 1, g_GameSpeed);
    }
    if (!enemy->flags.hidden && enemy->flags.active) {
        ASSERT(enemy->draw_list < ENEMY_DRAW_LIST_COUNT);
        enemy->next_drawn = g_Enemies.draw_lists[enemy->draw_list];
        g_Enemies.draw_lists[enemy->draw_list] = enemy;
    }
}

static CHAIN_STATUS M_Update(void *const arg)
{
    if (!Gui_IsMsgRunning()) {
        const int32_t period = M_RANK_PERIOD
            - (int32_t)g_GameManager.player.lives * M_RANK_PERIOD_PER_LIFE;
        if (Timer_HasTicked(&g_Enemies.frames)
            && g_Enemies.frames.current % period == 0) {
            GameManager_AddRank(M_RANK_STEP);
        }
        g_GameManager.run.progress_frames++;
    }
    for (int32_t i = 0; i < ENEMY_DRAW_LIST_COUNT; i++) {
        g_Enemies.draw_lists[i] = nullptr;
    }
    const int32_t timelines = Ecl_GetTimelineCount();
    ASSERT(timelines <= ECL_TIMELINE_COUNT);
    for (int32_t i = 0; i < timelines; i++) {
        M_RunTimeline(i);
    }
    // The original leaves the flag unset before the first enemy.
    bool hit = false;
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        if (g_Enemies.enemies[i].flags.active) {
            M_UpdateEnemy(&g_Enemies.enemies[i], i, &hit);
        }
    }
    Timer_TickAt(&g_Enemies.frames, g_GameSpeed);
    return CHAIN_CONTINUE;
}

// Returns the lower of angles a and b plus the fraction t of the shorter of
// their direct and wrapped distances.
static float M_LerpAngle(float a, const float b, const float t)
{
    float diff;
    float wrap;
    if (a < b) {
        diff = b - a;
        wrap = a + M_FULL_TURN - b;
    } else {
        diff = a - b;
        wrap = b + M_FULL_TURN - a;
        a = b;
    }
    if (diff < wrap) {
        return diff * t + a;
    }
    return wrap * t + a;
}

static void M_SetRotation(ANM_VM *const vm, const float angle)
{
    vm->rotation.z = angle;
    vm->rotation_changed = true;
}

static void M_DrawAt(
    ANM_VM *const vm, const VEC3 pos, const float z, const ANM_DRAW *const draw)
{
    vm->pos = Vec3_Add(pos, vm->pos_offset);
    vm->pos.z = z;
    vm->pos.x = g_GameManager.play_area.x + vm->pos.x;
    vm->pos.y = g_GameManager.play_area.y + vm->pos.y;
    Anm_Draw(vm, draw);
}

static void M_DrawExtraSprite(
    const ENEMY *const enemy, ANM_VM *const vm, const float angle,
    const ANM_DRAW *const draw)
{
    if (vm->script_index < 0) {
        return;
    }
    if (vm->auto_rotate != 0) {
        M_SetRotation(vm, angle);
    }
    M_DrawAt(vm, enemy->move.pos, 0.3f, draw);
}

static uint8_t M_FadeAlpha(
    const ENEMY *const enemy, const uint32_t color, const int32_t box)
{
    const int32_t alpha = (int32_t)(color >> 24);
    return (uint8_t)(alpha - alpha * box / enemy->trail.count);
}

// Draws copies of the main sprite on every step-th box of the trail.
static void M_DrawTrailCopies(
    ENEMY *const enemy, const VEC2 scale, const uint32_t color, ANM_DRAW draw)
{
    ANM_VM *const vm = &enemy->vm;
    const uint8_t flags = enemy->trail.flags;
    for (int32_t i = enemy->trail.step; i < enemy->trail.count;
         i += enemy->trail.step) {
        const ENEMY_TRAIL_BOX *const box = &enemy->trail.boxes[i];
        if (box->pos.x < M_TRAIL_GONE) {
            continue;
        }
        if (enemy->flags.rotate_sprite) {
            M_SetRotation(vm, box->angle);
        }
        if ((flags & ENEMY_TRAIL_NARROW) != 0) {
            vm->scale_x = scale.x - (float)i * scale.x / enemy->trail.count;
        }
        if ((flags & ENEMY_TRAIL_FADE) != 0) {
            vm->color = (vm->color & 0xffffffu)
                | (uint32_t)M_FadeAlpha(enemy, color, i) << 24;
        }
        draw.part = (uint32_t)i;
        M_DrawAt(vm, box->pos, 0.3f, &draw);
    }
}

// Writes the pair of strip vertices of trail box index across the trail
// at angle, half the sprite's height (by scale_y) to each side.
static void M_WriteStripPair(
    const ENEMY *const enemy, ANM_STRIP_VERTEX *const out, const int32_t index,
    const float angle, const float scale_y, const uint32_t color, const float u)
{
    const ANM_VM *const vm = &enemy->vm;
    const ANM_SPRITE *const sprite = vm->sprite_data;
    const float sine = X87_Sin(angle);
    const float cosine = X87_Cos(angle);
    float width = 0.0f;
    float height = scale_y * sprite->height / 2.0f;
    if ((enemy->trail.flags & ENEMY_TRAIL_NARROW) != 0) {
        const float narrow = 1.0f - (float)index / enemy->trail.count;
        width *= narrow;
        height *= narrow;
    }
    uint32_t pair_color = vm->color;
    if ((enemy->trail.flags & ENEMY_TRAIL_FADE) != 0) {
        pair_color = (pair_color & 0xffffffu)
            | (uint32_t)M_FadeAlpha(enemy, color, index) << 24;
    }
    const VEC3 pos = enemy->trail.boxes[index].pos;
    // The play area's corner is fixed at 32, 16, whatever the shake does.
    out[0] = (ANM_STRIP_VERTEX) {
        { cosine * width - sine * height + 32.0f + pos.x,
          sine * width + cosine * height + 16.0f + pos.y, pos.z },
        pair_color,
        { u, sprite->v0 + vm->uv_scroll_y },
    };
    out[1] = (ANM_STRIP_VERTEX) {
        { cosine * width + sine * height + 32.0f + pos.x,
          sine * width - cosine * height + 16.0f + pos.y, pos.z },
        pair_color,
        { u, sprite->v1 + vm->uv_scroll_y },
    };
}

// Draws the trail as one strip through every step-th box up to the first
// box off the trail.
static void M_DrawTrailStrip(
    ENEMY *const enemy, const VEC2 scale, const uint32_t color, ANM_DRAW draw)
{
    const int32_t count = enemy->trail.count;
    const int32_t step = enemy->trail.step;
    const ENEMY_TRAIL_BOX *const boxes = enemy->trail.boxes;
    int32_t vertices = 0;
    for (int32_t i = 0; i < count; i += step) {
        if (boxes[i].pos.x < M_TRAIL_GONE) {
            break;
        }
        vertices += 2;
    }
    if (vertices <= 2) {
        return;
    }
    const ANM_SPRITE *const sprite = enemy->vm.sprite_data;
    const float u_step = (sprite->u1 - sprite->u0) / ((vertices + 1) / 2 - 1);
    float u = sprite->u1 + enemy->vm.uv_scroll_x;
    ANM_STRIP_VERTEX strip[ENEMY_TRAIL_COUNT * 2];
    ANM_STRIP_VERTEX *out = strip;
    float last_angle = 0.0f;
    for (int32_t i = 0; i < count; i += step, u -= u_step) {
        if (boxes[i].pos.x < M_TRAIL_GONE) {
            break;
        }
        const float angle = i == 0
            ? boxes[0].angle
            : M_LerpAngle(boxes[i - 1].angle, boxes[i].angle, 0.5f);
        if ((enemy->trail.flags & ENEMY_TRAIL_NARROW) != 0 && i > 0
            && i + step < count) {
            // Compares with the box at step, not the one after this box.
            const float next =
                M_LerpAngle(boxes[i + step - 1].angle, boxes[step].angle, 0.5f);
            if (fabsf(last_angle - angle) < M_STRIP_ANGLE_EPSILON
                && fabsf(angle - next) < M_STRIP_ANGLE_EPSILON) {
                vertices -= 2;
                continue;
            }
        }
        last_angle = angle;
        M_WriteStripPair(enemy, out, i, angle, scale.y, color, u);
        out += 2;
    }
    if (vertices > 2) {
        draw.part = 1;
        Anm_DrawStrip(&enemy->vm, strip, vertices, &draw);
    }
}

static void M_DrawTrail(ENEMY *const enemy, const ANM_DRAW *const draw)
{
    ANM_VM *const vm = &enemy->vm;
    const VEC2 scale = { vm->scale_x, vm->scale_y };
    const uint32_t color = vm->color;
    ASSERT(enemy->trail.step > 0);
    if ((enemy->trail.flags & ENEMY_TRAIL_STRIP) == 0) {
        M_DrawTrailCopies(enemy, scale, color, *draw);
    } else {
        M_DrawTrailStrip(enemy, scale, color, *draw);
    }
    vm->scale_x = scale.x;
    vm->scale_y = scale.y;
    vm->color = color;
}

static void M_DrawEnemy(ENEMY *const enemy, const ANM_DRAW *const draw)
{
    M_DrawExtraSprite(enemy, &enemy->vms[0], enemy->move.angle, draw);
    ANM_VM *const vm = &enemy->vm;
    if (enemy->flags.rotate_sprite) {
        M_SetRotation(vm, enemy->move.angle);
    }
    vm->pos = Vec3_Add(enemy->move.pos, vm->pos_offset);
    vm->pos.z = 0.29f;
    if ((enemy->trail.flags & ENEMY_TRAIL_HIDE_HEAD) == 0
        && !enemy->flags.hidden_by_bomb) {
        vm->pos.x = g_GameManager.play_area.x + vm->pos.x;
        vm->pos.y = g_GameManager.play_area.y + vm->pos.y;
        Anm_Draw(vm, draw);
    }
    M_DrawExtraSprite(enemy, &enemy->vms[1], -enemy->move.angle, draw);
    if (enemy->trail.flags != 0) {
        M_DrawTrail(enemy, draw);
    }
}

static void M_DrawLists(const int32_t first)
{
    ANM_DRAW draw = GameManager_PlayAreaDraw();
    for (int32_t i = first; i < first + 2; i++) {
        for (ENEMY *enemy = g_Enemies.draw_lists[i]; enemy != nullptr;
             enemy = enemy->next_drawn) {
            M_DrawEnemy(enemy, &draw);
        }
    }
}

static CHAIN_STATUS M_DrawLow(void *const arg)
{
    M_DrawLists(ENEMY_DRAW_LOW);
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_DrawHigh(void *const arg)
{
    M_DrawLists(ENEMY_DRAW_HIGH);
    return CHAIN_CONTINUE;
}

static void M_HideBossMarkers(void)
{
    for (int32_t i = 0; i < ASCII_BOSS_MARKER_COUNT; i++) {
        g_AsciiText.boss_markers[i].pos =
            (VEC3) { M_NOWHERE, M_NOWHERE, M_NOWHERE };
    }
}

static RESULT M_Added(void *const arg)
{
    const int32_t stage = g_GameManager.run.stage;
    ASSERT(stage >= 1 && stage <= M_STAGE_COUNT);
    char path[32];
    snprintf(path, sizeof(path), "data/stg%denm.anm", (int)stage);
    MUST(Anm_Load(M_ANM_SLOT, path, M_FIRST_SPRITE));
    g_Enemies.drops.counter = Rng_U16InRange(&g_Rng, 3);
    g_Enemies.drops.index = Rng_U16InRange(&g_Rng, 8);
    g_Enemies.spell_card.state = ENEMY_SPELL_NONE;
    M_HideBossMarkers();
    return OK;
}

static void M_Deleted(void *const arg)
{
    Anm_Release(M_ANM_SLOT);
    M_HideBossMarkers();
}

RESULT Enemy_Register(void)
{
    M_Reset();
    Chain_InitElem(&m_CalcElem, M_Update, nullptr);
    m_CalcElem.added = M_Added;
    m_CalcElem.deleted = M_Deleted;
    MUST(Chain_AddCalc(&m_CalcElem, M_CALC_PRIORITY));
    Chain_InitElem(&m_DrawLowElem, M_DrawLow, nullptr);
    MUST(Chain_AddDraw(&m_DrawLowElem, M_DRAW_PRIORITY_LOW));
    Chain_InitElem(&m_DrawHighElem, M_DrawHigh, nullptr);
    return Chain_AddDraw(&m_DrawHighElem, M_DRAW_PRIORITY_HIGH);
}

void Enemy_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawLowElem);
    Chain_Remove(&m_DrawHighElem);
}

static ENEMY *M_FreeSlot(void)
{
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        if (!g_Enemies.enemies[i].flags.active) {
            return &g_Enemies.enemies[i];
        }
    }
    return nullptr;
}

// Starts sub on a free slot from the template, with vars when not null,
// and runs its first frame. Returns the enemy, or nullptr when the script
// ended in that frame.
static ENEMY *M_Spawn(
    ENEMY *const enemy, const int32_t sub, const VEC3 pos, const int32_t life,
    const int32_t item, const ECL_VARS *const vars)
{
    if (life >= 0) {
        enemy->life = life;
    }
    enemy->move.pos = pos;
    Ecl_StartSub(&enemy->script.state, (int16_t)sub);
    if (vars != nullptr) {
        enemy->script.state.vars = *vars;
    }
    if (Ecl_RunEnemy(enemy) == ECL_RUN_DELETE) {
        enemy->flags.active = false;
        return nullptr;
    }
    enemy->color = enemy->vm.color;
    enemy->death.item = (int8_t)item;
    return enemy;
}

static void M_SetScore(ENEMY *const enemy, const int32_t score)
{
    if (score >= 0) {
        enemy->score = score;
    }
    enemy->max_life = enemy->life;
}

static ENEMY *M_TakeSlot(void)
{
    ENEMY *const enemy = M_FreeSlot();
    if (enemy == nullptr) {
        return nullptr;
    }
    *enemy = m_Template;
    Enemy_RenewPresentation(enemy);
    return enemy;
}

ENEMY *Enemy_Spawn(
    const int32_t sub, const VEC3 pos, const int32_t life, const int32_t item,
    const int32_t score, const bool mirrored)
{
    ENEMY *const enemy = M_TakeSlot();
    if (enemy == nullptr) {
        return &m_Spare;
    }
    enemy->flags.mirrored = mirrored;
    if (M_Spawn(enemy, sub, pos, life, item, nullptr) != nullptr) {
        M_SetScore(enemy, score);
    }
    return enemy;
}

ENEMY *Enemy_SpawnWithVars(
    const int32_t sub, const VEC3 pos, const int32_t life, const int32_t item,
    const int32_t score, const ECL_VARS *const vars)
{
    ENEMY *const enemy = M_TakeSlot();
    if (enemy == nullptr) {
        return &m_Spare;
    }
    if (M_Spawn(enemy, sub, pos, life, item, vars) != nullptr) {
        if (life >= 0) {
            enemy->life = life;
        }
        M_SetScore(enemy, score);
    }
    return enemy;
}

void Enemy_RenewPresentation(ENEMY *const enemy)
{
    Anm_RenewPresentation(&enemy->vm);
    for (int32_t i = 0; i < ENEMY_SPRITE_COUNT; i++) {
        Anm_RenewPresentation(&enemy->vms[i]);
    }
}

void Enemy_ClampPos(ENEMY *const enemy)
{
    if (!enemy->flags.clamp_pos) {
        return;
    }
    VEC3 *const pos = &enemy->move.pos;
    if (pos->x < enemy->move.bounds_min.x) {
        pos->x = enemy->move.bounds_min.x;
    } else if (pos->x > enemy->move.bounds_max.x) {
        pos->x = enemy->move.bounds_max.x;
    }
    if (pos->y < enemy->move.bounds_min.y) {
        pos->y = enemy->move.bounds_min.y;
    } else if (pos->y > enemy->move.bounds_max.y) {
        pos->y = enemy->move.bounds_max.y;
    }
}

void Enemy_ReleaseOrbits(ENEMY *const enemy)
{
    for (int32_t i = 0; i < enemy->orbits.count; i++) {
        if (enemy->orbits.effects[i] == nullptr) {
            continue;
        }
        enemy->orbits.effects[i]->fade_out = true;
        enemy->orbits.effects[i] = nullptr;
    }
    enemy->orbits.count = 0;
}

// Drops a homing ITEM_STAR at pos and pops *bonus, then grows it.
static int32_t M_PopBonus(
    const VEC3 pos, int32_t *const bonus, const int32_t max_bonus)
{
    const int32_t popped = *bonus;
    Item_Spawn(pos, ITEM_STAR, ITEM_HOMING);
    AsciiText_AddPopup(
        pos, popped, popped >= max_bonus ? M_YELLOW : COLOR_WHITE);
    *bonus += M_CLEAR_BONUS_STEP;
    if (*bonus > max_bonus) {
        *bonus = max_bonus;
    }
    return popped;
}

int32_t Enemy_ClearForBonus(const int32_t max_bonus, const int32_t start)
{
    int32_t total = start;
    // Starts at 2000 even when max_bonus is lower.
    int32_t bonus = M_CLEAR_BONUS_START;
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        ENEMY *const enemy = &g_Enemies.enemies[i];
        if (!enemy->flags.active || enemy->flags.boss) {
            continue;
        }
        enemy->life = 0;
        if (enemy->flags.grazeable) {
            total += M_PopBonus(enemy->move.pos, &bonus, max_bonus);
            if (enemy->trail.flags != 0) {
                for (int32_t j = 0; j < enemy->trail.count;
                     j += M_GRAZE_PERIOD) {
                    total += M_PopBonus(
                        enemy->trail.boxes[j].pos, &bonus, max_bonus);
                }
            }
        }
        if (!enemy->flags.shootable && enemy->script.death_sub >= 0) {
            M_StartDeathSub(enemy);
        }
    }
    return total;
}

bool Enemy_IsBossActive(void)
{
    for (int32_t i = 0; i < ENEMY_BOSS_COUNT; i++) {
        if (g_Enemies.bosses[i] != nullptr) {
            return true;
        }
    }
    return false;
}
