#include "th06/enemy.h"

#include "th06/angle.h"
#include "th06/chain.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/item.h"
#include "th06/player.h"

#include "core/base/assert.h"

#include "stg/rng.h"

#include <string.h>

#define M_ANM_SLOT 8
#define M_ANM_SLOT2 9
#define M_FIRST_SPRITE 0x100
#define M_CALC_PRIORITY 9
#define M_DRAW_PRIORITY 6
#define M_STAGE_COUNT 8
#define M_RANDOM_POS -990.0f
#define M_RANDOM_DEPTH 800.0f
#define M_RANK_PERIOD 2400
#define M_RANK_PERIOD_PER_LIFE 240
#define M_RANK_STEP 100
#define M_HURT_CAP 70
#define M_CONTACT_DAMAGE 10
#define M_HIT_FLASH_FRAMES 4
#define M_BOSS_CLEAR_BONUS 0x3200
#define M_ORBIT_GROWTH 0.3f
#define M_ORBIT_TURN 0.031415928f
#define M_WHITE 0xffffffffu
#define M_DROP_COUNT 0x20

// The spawn opcodes of the timeline combine three bits.
#define M_SPAWN_DEFAULT 1
#define M_SPAWN_MIRRORED 2
#define M_SPAWN_RANDOM 4

typedef struct {
    const char *anm;
    const char *anm2;
} M_ANM_PATHS;

ENEMY_MANAGER g_Enemies;

static const uint8_t m_ItemDrops[M_DROP_COUNT] = {
    0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 2,
};

static const M_ANM_PATHS m_AnmPaths[M_STAGE_COUNT] = {
    { "dummy", "dummy" },
    { "data/stg1enm.anm", "data/stg1enm2.anm" },
    { "data/stg2enm.anm", "data/stg2enm2.anm" },
    { "data/stg3enm.anm", nullptr },
    { "data/stg4enm.anm", nullptr },
    { "data/stg5enm.anm", "data/stg5enm2.anm" },
    { "data/stg6enm.anm", "data/stg6enm2.anm" },
    { "data/stg7enm.anm", "data/stg7enm2.anm" },
};

static ENEMY m_Template;
// Is returned by Enemy_Spawn when every slot is in use. The update never
// runs it, but the timeline still marks it mirrored.
static ENEMY m_Spare;
static CHAIN_ELEM m_CalcElem;
static CHAIN_ELEM m_DrawElem;

static void M_Reset(void)
{
    memset(&g_Enemies, 0, sizeof(g_Enemies));
    memset(&m_Spare, 0, sizeof(m_Spare));
    memset(&m_Template, 0, sizeof(m_Template));
    ENEMY *const enemy = &m_Template;
    for (int32_t i = 0; i < ENEMY_SPRITE_COUNT; i++) {
        enemy->vms[i].script_index = -1;
    }
    enemy->script.state.frame_func = ECL_FUNC_NONE;
    enemy->flags.active = true;
    Timer_Set(&enemy->callbacks.timer, 0);
    enemy->flags.shootable = true;
    enemy->flags.contact_damage = true;
    enemy->hitbox_size = (VEC3) { 12.0f, 12.0f, 12.0f };
    enemy->life = 1;
    enemy->score = 100;
    Timer_Set(&enemy->shots.timer, 0);
    enemy->turns.left = -1;
    enemy->turns.right = -1;
    enemy->turns.still = -1;
    enemy->flags.takes_damage = true;
    enemy->flags.death = ENEMY_DEATH_REMOVE;
    enemy->script.death_sub = -1;
    enemy->script.pending_interrupt = -1;
    enemy->callbacks.life = -1;
    enemy->callbacks.time = -1;
    enemy->shots.rank_speed_low = -0.5f;
    enemy->shots.rank_speed_high = 0.5f;
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
    VEC3 pos = entry->args.spawn.pos;
    if ((bits & M_SPAWN_RANDOM) != 0) {
        pos = M_RandomPos(pos);
    }
    ENEMY *const enemy = (bits & M_SPAWN_DEFAULT) != 0
        ? Enemy_Spawn(entry->arg, pos, -1, -1, -1)
        : Enemy_Spawn(
              entry->arg, pos, entry->args.spawn.life, entry->args.spawn.item,
              entry->args.spawn.score);
    if ((bits & M_SPAWN_MIRRORED) != 0) {
        enemy->flags.mirrored = true;
    }
}

static ENEMY *M_GetBoss(const int32_t index)
{
    ASSERT(index >= 0 && index < ENEMY_BOSS_COUNT);
    return g_Enemies.bosses[index];
}

// Runs one entry of the timeline on its frame. Returns whether the
// timeline waits at the entry.
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
    case ECL_TIMELINE_START_MSG: {
        const int32_t character = g_GameManager.run.character * 10;
        if (g_GameManager.run.difficulty == 0 && g_GameManager.run.stage == 5
            && entry->arg == 1) {
            Gui_StartMsg(character + 3);
        } else {
            Gui_StartMsg(entry->arg + character);
        }
        return false;
    }
    case ECL_TIMELINE_WAIT_MSG:
        return Gui_MsgHoldsTimeline();
    case ECL_TIMELINE_INTERRUPT_BOSS: {
        ENEMY *const boss = M_GetBoss(entry->args.interrupt.boss);
        ASSERT(boss != nullptr);
        boss->script.pending_interrupt = entry->args.interrupt.interrupt;
        return false;
    }
    case ECL_TIMELINE_SET_POWER:
        g_GameManager.player.power = (uint16_t)entry->arg;
        return false;
    case ECL_TIMELINE_WAIT_BOSS: {
        const ENEMY *const boss = M_GetBoss(entry->arg);
        return boss != nullptr && boss->flags.active;
    }
    }
    return false;
}

// Runs the entries of the timeline due on this frame and skips the ones
// whose frame has passed. An entry that waits holds the frame count
// back, so the entry runs again on the next frame; the frame does not
// count as progress then.
static void M_RunTimeline(void)
{
    const ECL_TIMELINE_ENTRY *const timeline = Ecl_GetTimeline();
    TIMER *const time = &g_Enemies.timeline.time;
    if (!Gui_IsMsgRunning()) {
        const int32_t period =
            M_RANK_PERIOD - g_GameManager.player.lives * M_RANK_PERIOD_PER_LIFE;
        if (Timer_HasTicked(time) && time->current % period == 0) {
            GameManager_AddRank(M_RANK_STEP);
        }
    }
    for (;;) {
        const ECL_TIMELINE_ENTRY *const entry =
            &timeline[g_Enemies.timeline.index];
        if (entry->time < 0) {
            break;
        }
        if (time->current == entry->time) {
            if (M_RunEntry(entry)) {
                time->current--;
                return;
            }
        } else if (time->current < entry->time) {
            break;
        }
        g_Enemies.timeline.index++;
    }
    if (!Gui_IsMsgRunning()) {
        g_GameManager.run.progress_frames++;
    }
}

static void M_Move(ENEMY *const enemy)
{
    if (!enemy->flags.mirrored) {
        enemy->move.pos.x += enemy->move.velocity.x;
    } else {
        enemy->move.pos.x -= enemy->move.velocity.x;
    }
    enemy->move.pos.y += enemy->move.velocity.y;
    enemy->move.pos.z += enemy->move.velocity.z;
}

static void M_Despawn(ENEMY *const enemy)
{
    if (enemy->flags.death == ENEMY_DEATH_REMOVE) {
        enemy->flags.active = false;
    } else {
        enemy->flags.shootable = false;
    }
    if (enemy->flags.boss) {
        g_Gui.boss.present = false;
    }
    if (enemy->orbits.count != 0) {
        Enemy_ReleaseOrbits(enemy);
    }
}

static void M_CheckLifeCallback(ENEMY *const enemy)
{
    if (enemy->life >= enemy->callbacks.life) {
        return;
    }
    enemy->life = enemy->callbacks.life;
    Ecl_StartSub(&enemy->script.state, (int16_t)enemy->callbacks.life_sub);
    enemy->callbacks.life = -1;
    enemy->callbacks.time_sub = enemy->script.death_sub;
    M_ResetRankGrowth(enemy);
    enemy->script.depth = 0;
    Enemy_KillAll();
}

static void M_CheckTimeCallback(ENEMY *const enemy)
{
    if (enemy->flags.boss) {
        g_Gui.boss.seconds =
            (enemy->callbacks.time - enemy->callbacks.timer.current) / 60;
    }
    if (enemy->callbacks.timer.current < enemy->callbacks.time) {
        return;
    }
    if (enemy->callbacks.life > 0) {
        enemy->life = enemy->callbacks.life;
        enemy->callbacks.life = -1;
    }
    Ecl_StartSub(&enemy->script.state, (int16_t)enemy->callbacks.time_sub);
    enemy->callbacks.time = -1;
    enemy->callbacks.time_sub = enemy->script.death_sub;
    Timer_Set(&enemy->callbacks.timer, 0);
    if (!enemy->flags.keep_spell_card) {
        g_Enemies.spell_card.capturable = false;
        // Counts up rather than setting ENEMY_SPELL_TIMED_OUT, so a second
        // timeout in one spell card goes past it.
        if (g_Enemies.spell_card.state != ENEMY_SPELL_NONE) {
            g_Enemies.spell_card.state =
                (ENEMY_SPELL)(g_Enemies.spell_card.state + 1);
        }
        Bullet_ClearAll();
    }
    Enemy_KillAll();
    M_ResetRankGrowth(enemy);
    enemy->script.depth = 0;
}

// Divides the damage of a frame during a spell card: a shot deals a
// seventh, a bomb a third once the player bombed during it and nothing
// before. Damage that the division takes to 0 deals 1.
static int32_t M_SpellCardDamage(const int32_t hurt, const bool by_bomb)
{
    int32_t divisor = 7;
    if (by_bomb) {
        if (!g_Enemies.spell_card.bombed) {
            return 0;
        }
        divisor = 3;
    }
    if (hurt > divisor) {
        return hurt / divisor;
    }
    return hurt != 0 ? 1 : 0;
}

// Applies the contact with the player and the damage of the shots and the
// bomb. Returns whether the bomb hit the enemy.
static bool M_TakeDamage(ENEMY *const enemy)
{
    bool by_bomb = false;
    if (enemy->flags.contact_damage && enemy->flags.shootable) {
        const VEC3 size = Vec3_Scale(enemy->hitbox_size, 1.0f / 1.5f);
        if (Player_CheckHit(enemy->move.pos, size) == PLAYER_CONTACT_BOX
            && enemy->flags.shootable && !enemy->flags.boss) {
            enemy->life -= M_CONTACT_DAMAGE;
        }
    }
    if (!enemy->flags.shootable) {
        return by_bomb;
    }
    int32_t hurt =
        Player_CalcDamage(enemy->move.pos, enemy->hitbox_size, &by_bomb);
    if (hurt >= M_HURT_CAP) {
        hurt = M_HURT_CAP;
    }
    g_GameManager.score.value += (uint32_t)(hurt / 5 * 10);
    if (g_Enemies.spell_card.state != ENEMY_SPELL_NONE) {
        hurt = M_SpellCardDamage(hurt, by_bomb);
    }
    if (enemy->flags.takes_damage) {
        enemy->life -= hurt;
    }
    if (g_Player.homing_target.y < enemy->move.pos.y) {
        g_Player.homing_target = enemy->move.pos;
    }
    return by_bomb;
}

static void M_DropItem(const ENEMY *const enemy, const bool by_bomb)
{
    const ITEM_STATE state = by_bomb ? ITEM_HOMING : ITEM_FALLING;
    const EFFECT_TYPE particle =
        (EFFECT_TYPE)(EFFECT_PARTICLE + enemy->death.particle_color);
    if (enemy->death.item >= 0) {
        Effect_Spawn(particle, enemy->move.pos, 3, M_WHITE);
        Item_Spawn(enemy->move.pos, (ITEM_TYPE)enemy->death.item, state);
    } else if (enemy->death.item == -1) {
        if (g_Enemies.drops.counter % 3 == 0) {
            Effect_Spawn(particle, enemy->move.pos, 6, M_WHITE);
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

static void M_Die(ENEMY *const enemy, const bool by_bomb)
{
    enemy->callbacks.life = -1;
    enemy->callbacks.time = -1;
    switch (enemy->flags.death) {
    case ENEMY_DEATH_SURVIVE:
        enemy->life = 1;
        enemy->flags.takes_damage = false;
        enemy->flags.death = ENEMY_DEATH_REMOVE;
        g_Gui.boss.present = false;
        for (int32_t i = 0; i < 3; i++) {
            Effect_Spawn(
                (EFFECT_TYPE)enemy->death.effect, enemy->move.pos, 1, M_WHITE);
        }
        break;
    case ENEMY_DEATH_REMOVE:
    case ENEMY_DEATH_KEEP:
        g_GameManager.score.value += (uint32_t)enemy->score;
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
            Bullet_ClearAllWithBonus(M_BOSS_CLEAR_BONUS, false);
        }
        enemy->life = 0;
        break;
    default:
        break;
    }
    Effect_Spawn((EFFECT_TYPE)enemy->death.effect, enemy->move.pos, 1, M_WHITE);
    Effect_Spawn(
        (EFFECT_TYPE)(EFFECT_PARTICLE + enemy->death.particle_color),
        enemy->move.pos, 4, M_WHITE);
    if (enemy->script.death_sub >= 0) {
        M_ResetRankGrowth(enemy);
        enemy->script.depth = 0;
        Ecl_StartSub(&enemy->script.state, (int16_t)enemy->script.death_sub);
        enemy->script.death_sub = -1;
    }
}

static void M_UpdateHitFlash(ENEMY *const enemy, const int32_t life_before)
{
    if (enemy->hit_flash_frames != 0) {
        enemy->hit_flash_frames--;
        enemy->vm.color_op = false;
    } else if (life_before > enemy->life) {
        enemy->vm.color_op = true;
        enemy->hit_flash_frames = M_HIT_FLASH_FRAMES;
    } else {
        enemy->vm.color_op = false;
    }
}

static void M_MoveOrbits(ENEMY *const enemy)
{
    for (int32_t i = 0; i < enemy->orbits.count; i++) {
        EFFECT *const effect = enemy->orbits.effects[i];
        if (effect == nullptr) {
            continue;
        }
        effect->origin = enemy->move.pos;
        if (effect->radius < enemy->orbits.radius) {
            effect->radius += M_ORBIT_GROWTH;
        }
        effect->angle = Angle_AddNormalize(effect->angle, M_ORBIT_TURN);
    }
}

static bool M_IsInPlayArea(const ENEMY *const enemy)
{
    const ANM_SPRITE *const sprite = enemy->vm.sprite_data;
    ASSERT(sprite != nullptr);
    return GameManager_IsInPlayArea(
        enemy->move.pos.x, enemy->move.pos.y, sprite->width, sprite->height);
}

static void M_UpdateEnemy(ENEMY *const enemy)
{
    M_Move(enemy);
    Enemy_ClampPos(enemy);
    const bool inside = M_IsInPlayArea(enemy);
    if (!enemy->flags.on_screen && inside) {
        enemy->flags.on_screen = true;
    }
    if (enemy->flags.on_screen && !inside) {
        enemy->flags.active = false;
        M_Despawn(enemy);
        return;
    }
    if (enemy->callbacks.life >= 0) {
        M_CheckLifeCallback(enemy);
    }
    if (enemy->callbacks.time >= 0) {
        M_CheckTimeCallback(enemy);
    }
    if (Ecl_RunEnemy(enemy) == ECL_RUN_DELETE) {
        enemy->flags.active = false;
        M_Despawn(enemy);
        return;
    }
    Enemy_ClampPos(enemy);
    enemy->vm.color = enemy->color;
    Anm_Step(&enemy->vm);
    enemy->color = enemy->vm.color;
    for (int32_t i = 0; i < ENEMY_SPRITE_COUNT; i++) {
        ANM_VM *const vm = &enemy->vms[i];
        if (vm->script_index >= 0 && Anm_Step(vm)) {
            vm->script_index = -1;
        }
    }
    if (enemy->flags.on_screen && !enemy->flags.hidden) {
        const int32_t life_before = enemy->life;
        const bool by_bomb = M_TakeDamage(enemy);
        if (enemy->life <= 0 && enemy->flags.shootable) {
            M_Die(enemy, by_bomb);
        }
        if (enemy->flags.boss && !Gui_IsMsgRunning()) {
            g_Gui.boss.health = (float)enemy->life / (float)enemy->max_life;
        }
        M_UpdateHitFlash(enemy, life_before);
    }
    M_MoveOrbits(enemy);
    if (!g_GameManager.mode.frozen) {
        Timer_Tick(&enemy->callbacks.timer);
    }
}

static CHAIN_STATUS M_Update(void *const arg)
{
    M_RunTimeline();
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        if (g_Enemies.enemies[i].flags.active) {
            M_UpdateEnemy(&g_Enemies.enemies[i]);
        }
    }
    Timer_Tick(&g_Enemies.timeline.time);
    return CHAIN_CONTINUE;
}

static void M_PlaceSprite(
    ANM_VM *const vm, const ENEMY *const enemy, const bool turn, const float z)
{
    if (turn) {
        vm->rotation.z = enemy->move.angle;
    }
    vm->pos = Vec3_Add(enemy->move.pos, vm->pos_offset);
    vm->pos.z = z;
}

static void M_PlaceExtraSprites(
    ENEMY *const enemy, const int32_t first, const int32_t end,
    const ANM_DRAW *const draw)
{
    for (int32_t i = first; i < end; i++) {
        ANM_VM *const vm = &enemy->vms[i];
        if (vm->script_index >= 0) {
            M_PlaceSprite(vm, enemy, vm->auto_rotate != 0, 0.495f);
            Anm_Draw3DRounded(vm, draw);
        }
    }
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    ANM_DRAW draw = GameManager_GetDrawContext();
    draw.interpolate = true;
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        ENEMY *const enemy = &g_Enemies.enemies[i];
        if (!enemy->flags.active || enemy->flags.hidden) {
            continue;
        }
        M_PlaceExtraSprites(enemy, 0, ENEMY_SPRITE_COUNT / 2, &draw);
        M_PlaceSprite(&enemy->vm, enemy, enemy->flags.rotate_sprite, 0.494f);
        Anm_Draw3DRounded(&enemy->vm, &draw);
        M_PlaceExtraSprites(
            enemy, ENEMY_SPRITE_COUNT / 2, ENEMY_SPRITE_COUNT, &draw);
    }
    return CHAIN_CONTINUE;
}

static RESULT M_Added(void *const arg)
{
    const int32_t stage = g_GameManager.run.stage;
    ASSERT(stage >= 0 && stage < M_STAGE_COUNT);
    const M_ANM_PATHS *const paths = &m_AnmPaths[stage];
    if (paths->anm != nullptr) {
        MUST(Anm_Load(M_ANM_SLOT, paths->anm, M_FIRST_SPRITE));
    }
    if (paths->anm2 != nullptr) {
        MUST(Anm_Load(M_ANM_SLOT2, paths->anm2, M_FIRST_SPRITE));
    }
    g_Enemies.drops.counter = Rng_U16InRange(&g_Rng, 3);
    g_Enemies.drops.index = Rng_U16InRange(&g_Rng, 8);
    g_Enemies.spell_card.state = ENEMY_SPELL_NONE;
    g_Enemies.timeline.index = 0;
    return OK;
}

static void M_Deleted(void *const arg)
{
    Anm_Release(M_ANM_SLOT2);
    Anm_Release(M_ANM_SLOT);
}

RESULT Enemy_Register(void)
{
    M_Reset();
    Chain_InitElem(&m_CalcElem, M_Update, nullptr);
    m_CalcElem.added = M_Added;
    m_CalcElem.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &m_CalcElem, M_CALC_PRIORITY));
    Chain_InitElem(&m_DrawElem, M_Draw, nullptr);
    return Chain_AddDraw(&g_Chain, &m_DrawElem, M_DRAW_PRIORITY);
}

void Enemy_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawElem);
}

ENEMY *Enemy_Spawn(
    const int32_t sub, const VEC3 pos, const int16_t life, const int16_t item,
    const int32_t score)
{
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        ENEMY *const enemy = &g_Enemies.enemies[i];
        if (enemy->flags.active) {
            continue;
        }
        *enemy = m_Template;
        Enemy_RenewPresentation(enemy);
        if (life >= 0) {
            enemy->life = life;
        }
        enemy->move.pos = pos;
        Ecl_StartSub(&enemy->script.state, (int16_t)sub);
        Ecl_RunEnemy(enemy);
        enemy->color = enemy->vm.color;
        enemy->death.item = (int8_t)item;
        if (life >= 0) {
            enemy->life = life;
        }
        if (score >= 0) {
            enemy->score = score;
        }
        enemy->max_life = enemy->life;
        return enemy;
    }
    return &m_Spare;
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

void Enemy_KillAll(void)
{
    for (int32_t i = 0; i < ENEMY_COUNT; i++) {
        ENEMY *const enemy = &g_Enemies.enemies[i];
        if (!enemy->flags.active || enemy->flags.boss) {
            continue;
        }
        enemy->life = 0;
        if (!enemy->flags.shootable && enemy->script.death_sub >= 0) {
            Ecl_StartSub(
                &enemy->script.state, (int16_t)enemy->script.death_sub);
            enemy->script.death_sub = -1;
        }
    }
}
