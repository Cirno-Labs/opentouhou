#include "th01/boss_b10m.h"
#include "th01/session.h"
#include "th01/sound.h"
#include "th01/rng.h"

#include "th01/boss.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "th01/line_bullets.h"

#include "core/base/assert.h"

#include <string.h>

B10M g_B10M;

static RESULT M_LoadMissiles(
    VFS *const vfs, const ALLOCATOR *const a, PTN *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS3_M.PTN", VFS_IN_MEMORY, &reader));
    RESULT result = Ptn_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

RESULT B10M_ValidateAssets(const B10M_ASSETS *const a)
{
    FAIL_IF(
        a == nullptr || a->eyes.pixels == nullptr || a->eyes.coverage == nullptr
            || a->eyes.width != 64 || a->eyes.height != 48
            || a->eyes.count != 7,
        "Invalid YuugenMagan eye bank");
    FAIL_IF(
        a->background.width != 640 || a->background.height != 400
            || a->background.pixels == nullptr || !a->background.has_palette,
        "Invalid YuugenMagan background");
    MUST(Missiles_ValidateSprites(&a->missiles));
    FAIL_IF(
        a->next_scene.id != 3 || a->next_scene.layouts.stages == nullptr,
        "Missing YuugenMagan next scene");
    return OK;
}

RESULT B10M_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const a, B10M_ASSETS *const out)
{
    *out = (B10M_ASSETS) {};
    B10M_ASSETS loaded = {};
    MUST(Boss_LoadBos(vfs, "BOSS2.BOS", a, &loaded.eyes));
    MUST(Boss_LoadBackground(vfs, "BOSS2.GRP", a, &loaded.background));
    MUST(M_LoadMissiles(vfs, a, &loaded.missiles));
    MUST(Scene_Load(vfs, 3, a, &loaded.next_scene));
    MUST(B10M_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B10M_Init(void)
{
    memset(&g_B10M, 0, sizeof(g_B10M));
}

void B10M_Setup(const B10M_CONTEXT *const c)
{
    ASSERT(!g_B10M.defeating);
    ASSERT(Result_Ignore(B10M_ValidateAssets(c->assets)));
    ASSERT(
        Result_Ignore(BossEntity_Bind(&g_B10M.eyes[0], 0, &c->assets->eyes)));
    for (int32_t i = 1; i < 5; i++) {
        BossEntity_CopyMetadata(&g_B10M.eyes[i], &g_B10M.eyes[0]);
    }
    Palette_Set(&g_Palette, &c->assets->palette);
    g_B10M.boss_palette = g_Palette.saved;
    g_B10M.post_defeat = g_Palette.saved;
    g_StagePalette = g_B10M.post_defeat;
    const int16_t left[] = { 64, 512, 192, 384, 288 };
    const int16_t top[] = { 128, 128, 176, 176, 64 };
    for (int32_t i = 0; i < 5; i++) {
        g_B10M.eyes[i].image = 0;
        BossEntity_SetPosition(
            &g_B10M.eyes[i], left[i], top[i],
            (BOSS_BOUNDS) { 0, 736, 64, 304 });
        BossEntity_SetOrbHitbox(
            &g_B10M.eyes[i], (BOSS_BOUNDS) { -4, 68, -4, 48 });
        g_B10M.eyes[i].orb_inactive = true;
    }
    g_B10M.frame = 0;
    g_B10M.phase = B10M_PHASE_ENTRANCE;
    g_B10M.hp = 16;
    g_B10M.finished = false;
    g_HudHp.first_white = 12;
    g_HudHp.first_redwhite = 8;
    Particles_Init();
    Missiles_Reset();
}

static int8_t M_Byte(const int32_t value)
{
    const uint8_t bits = (uint8_t)value;
    return bits < 128 ? (int8_t)bits : (int8_t)((int16_t)bits - 256);
}

static void M_Lock(const B10M_CONTEXT *const c, const uint8_t flags)
{
    for (int32_t i = 0; i < 5; i++) {
        if ((flags & (1 << i)) != 0) {
            BossEntity_LockedMovePut(&g_B10M.eyes[i], 0, 0, 3, true);
        }
    }
}

static void M_Track(const B10M_CONTEXT *const c, const uint8_t flags)
{
    for (int32_t i = 0; i < 5; i++) {
        BOSS_ENTITY *const e = &g_B10M.eyes[i];
        if ((flags & (1 << i)) != 0) {
            const int16_t center = Math16_Wrap(e->left + 32);
            e->image = Math16_Wrap(center - Boss_Player()->left) > 32 ? 4
                : Math16_Wrap(Boss_Player()->left - center) > 32      ? 5
                                                                      : 3;
        }
    }
}

static void M_Image(const uint8_t flags, const int16_t image)
{
    for (int32_t i = 0; i < 5; i++) {
        if ((flags & (1 << i)) != 0) {
            g_B10M.eyes[i].image = image;
        }
    }
}

static void M_Active(const uint8_t flags, const bool active)
{
    for (int32_t i = 0; i < 5; i++) {
        if ((flags & (1 << i)) != 0) {
            g_B10M.eyes[i].orb_inactive = !active;
        }
    }
}

static void M_Next(const int8_t phase)
{
    g_B10M.phase = phase;
    g_B10M.frame = 0;
    g_B10M.hit.frame = 0;
}

static void M_Common(void)
{
    g_B10M.frame = Math16_Wrap(g_B10M.frame + 1);
    g_B10M.hit.frame = Math16_Wrap(g_B10M.hit.frame + 1);
}

static void M_Snap(const B10M_CONTEXT *const c)
{
    Palette_Set(&g_Palette, &g_StagePalette);
    g_B10M.boss_palette = g_Palette.saved;
}

static void M_Group(
    const B10M_CONTEXT *const c, const int32_t eye, const PELLET_GROUP group,
    const int16_t speed, const bool iris)
{
    Pellets_AddGroup(
        g_Session.settings.difficulty, Math16_Wrap(g_B10M.eyes[eye].left + 28),
        Math16_Wrap(g_B10M.eyes[eye].top + (iris ? 28 : 32)), group, speed);
}

static void M_Single(
    const B10M_CONTEXT *const c, const int32_t eye, const uint8_t angle,
    const PELLET_MOTION motion)
{
    Pellets_AddSingle(
        g_Session.settings.difficulty, Math16_Wrap(g_B10M.eyes[eye].left + 28),
        Math16_Wrap(g_B10M.eyes[eye].top + 32), angle, 64, motion,
        motion == PELLET_REGULAR ? 0 : 64, 0, 0);
}

static void M_Hit(const B10M_CONTEXT *const c)
{
    bool colliding = false;
    for (int32_t i = 0; i < 5; i++) {
        if (BossEntity_HitTestOrb(&g_B10M.eyes[i])
            && g_B10M.eyes[i].image != 0) {
            colliding = true;
            break;
        }
    }
    const uint8_t flash[] = { 1, 11 };
    BossHit_UpdateAndRender(
        &g_B10M.hit, c->shots_output, &g_B10M.hp, &g_B10M.boss_palette, flash,
        2, 5000, colliding, (BOSS_SHOT_HITBOX) {});
}

static void M_MissileAction(void *const user, const MISSILES_ACTION action)
{
    const B10M_CONTEXT *const c = user;
    if (action.kind == MISSILES_ACTION_EFFECT) {
        Sound_Effect(action.effect);
    } else {
        Missiles_RenderAction(&c->assets->missiles, action);
    }
}

static void M_Toggle(
    const B10M_CONTEXT *const c, const uint8_t close, const uint8_t open,
    const int16_t dec, const int16_t inc, int16_t *const frame)
{
    M_Lock(c, close | open);
    *frame = Math16_Wrap(*frame + 1);
    if (*frame % 10 != 0) {
        if (dec < 3 && g_Palette.saved.colors[15][dec] > 0) {
            g_Palette.saved.colors[15][dec]--;
            g_StagePalette.colors[15][dec] =
                M_Byte(g_StagePalette.colors[15][dec] - 1);
        }
        if (inc < 3) {
            g_Palette.saved.colors[15][inc] =
                M_Byte(g_Palette.saved.colors[15][inc] + 1);
            g_StagePalette.colors[15][inc] =
                M_Byte(g_StagePalette.colors[15][inc] + 1);
        }
        Palette_Set(&g_Palette, &g_StagePalette);
    }
    if (*frame == 20) {
        M_Image(close, 2);
        M_Image(open, 1);
        M_Active(open, true);
    } else if (*frame == 40) {
        M_Image(close, 1);
        M_Image(open, 2);
        M_Active(close, false);
    } else if (*frame == 60) {
        M_Image(close, 0);
        M_Image(open, 6);
    }
}

static void M_DownLaser(
    const B10M_CONTEXT *const c, const int32_t i, const bool put)
{
    const int16_t x = Math16_Wrap(g_B10M.eyes[i].left + 28);
    const int16_t y = Math16_Wrap(g_B10M.eyes[i].top + 28);
    if (put) {
        Video_VerticalLine(x, y, 398, 7);
    } else {
        Video_LineErase(x, y, x, 400);
    }
}

static void M_Entrance(const B10M_CONTEXT *const c)
{
    if (!g_B10M.hp_bar_filled) {
        g_B10M.hp_bar_filled = HudHp_Increment(g_B10M.hp, g_B10M.frame);
    }
    g_B10M.frame = Math16_Wrap(g_B10M.frame + 1);
    M_Lock(c, 31);
    for (int32_t i = 0; i < 5; i++) {
        const int16_t starts[] = { 160, 160, 180, 180, 200 };
        const int16_t ends[] = { 300, 300, 320, 320, 32767 };
        if (g_B10M.frame > starts[i] && g_B10M.frame < ends[i]) {
            M_DownLaser(c, i, true);
            const int16_t x = Math16_Wrap(g_B10M.eyes[i].left + 28);
            const int16_t width = i == 1 ? 8 : 32;
            if (Boss_Player()->left > Math16_Wrap(x - 24)
                && Boss_Player()->left < Math16_Wrap(x + 24 - width)) {
                Boss_Player()->is_hit = true;
            }
        }
    }
    if (Boss_Player()->invincible) {
        Boss_Player()->is_hit = false;
    }
    if (g_B10M.frame % 40 == 0) {
        g_StagePalette.colors[15][0] = M_Byte(g_StagePalette.colors[15][0] + 1);
        g_StagePalette.colors[15][1] = M_Byte(g_StagePalette.colors[15][1] + 1);
        Palette_Set(&g_Palette, &g_StagePalette);
    }
    const int16_t open_at[] = { 100, 120, 140, 160, 180, 200 };
    bool opening = false;
    for (int32_t i = 0; i < 6; i++) {
        opening |= g_B10M.frame == open_at[i];
    }
    if (opening) {
        const int16_t starts[] = { 100, 100, 120, 120, 140 };
        for (int32_t i = 0; i < 5; i++) {
            const int16_t delta = Math16_Wrap(g_B10M.frame - starts[i]);
            if (delta >= 0 && delta <= 60) {
                g_B10M.eyes[i].image = delta < 20 ? 1
                    : delta < 40                  ? 2
                    : delta < 60                  ? 6
                                                  : 3;
            }
        }
        if (g_B10M.frame == 100) {
            M_Active(3, true);
        } else if (g_B10M.frame == 120) {
            M_Active(12, true);
        } else if (g_B10M.frame == 140) {
            M_Active(16, true);
        }
    } else if (
        g_B10M.frame == 240 || g_B10M.frame == 260 || g_B10M.frame == 280) {
        M_Image(31, g_B10M.frame == 240 ? 2 : g_B10M.frame == 260 ? 1 : 0);
        if (g_B10M.frame == 240) {
            M_Active(31, false);
        }
        if ((g_B10M.frame == 240
             && g_Session.settings.difficulty == SETTINGS_DIFFICULTY_HARD)
            || (g_B10M.frame == 260
                && g_Session.settings.difficulty
                    == SETTINGS_DIFFICULTY_LUNATIC)) {
            M_Group(c, 4, PELLET_GROUP_FIVE_WIDE_AIMED, 54, true);
            M_Group(c, 3, PELLET_GROUP_FIVE_WIDE_AIMED, 54, true);
            M_Group(c, 2, PELLET_GROUP_FIVE_WIDE_AIMED, 54, true);
            if (g_Session.settings.difficulty == SETTINGS_DIFFICULTY_LUNATIC) {
                M_Group(c, 0, PELLET_GROUP_FIVE_WIDE_AIMED, 54, true);
                M_Group(c, 1, PELLET_GROUP_FIVE_WIDE_AIMED, 54, true);
            }
        }
    } else if (g_B10M.frame == 300) {
        M_Image(3, 1);
        M_DownLaser(c, 0, false);
        M_DownLaser(c, 1, false);
    } else if (g_B10M.frame == 320) {
        M_Image(3, 2);
        M_Active(3, true);
        M_DownLaser(c, 2, false);
        M_DownLaser(c, 3, false);
    } else if (g_B10M.frame > 330) {
        g_B10M.phase = B10M_PHASE_LATERAL;
        g_B10M.hit.invincible = false;
        g_B10M.frame = 0;
        M_Snap(c);
        g_B10M.hp_bar_filled = false;
        g_B10M.eyes[0].image = 4;
        g_B10M.eyes[1].image = 3;
        g_B10M.pattern_interval = Boss_Rank(350, 300, 200, 130);
        M_DownLaser(c, 4, false);
    }
}

static void M_Lateral(const B10M_CONTEXT *const c)
{
    M_Lock(c, 3);
    M_Track(c, 3);
    const int16_t frame = g_B10M.frame % g_B10M.pattern_interval;
    const int16_t times[] = { 10, 25, 40, 60, 80, 110 };
    const int16_t speeds[] = { 24, 28, 32, 44, 50, 68 };
    for (int32_t i = 0; i < 6; i++) {
        if (frame == times[i]) {
            M_Group(
                c, 0,
                i == 5 ? PELLET_GROUP_SINGLE_AIMED
                       : PELLET_GROUP_TWO_WIDE_AIMED,
                speeds[i], false);
            M_Group(
                c, 1,
                i == 5 ? PELLET_GROUP_SINGLE_AIMED
                       : PELLET_GROUP_TWO_WIDE_AIMED,
                speeds[i], false);
            break;
        }
    }
}

static void M_FireMissiles(const B10M_CONTEXT *const c)
{
    const uint8_t angles[] = { g_B10M.angle, g_B10M.angle_southeast };
    for (int32_t i = 0; i < 2; i++) {
        const MATH16_VECTOR v = Math16_Vector(8, angles[i]);
        Missiles_Add(
            Math16_Wrap(g_B10M.eyes[i + 2].left + 28),
            Math16_Wrap(g_B10M.eyes[i + 2].top + 28), v.x, v.y, 0);
    }
}

static void M_Missiles(const B10M_CONTEXT *const c, const bool clock)
{
    M_Lock(c, 12);
    M_Track(c, 12);
    if (g_B10M.frame >= 90 && (g_B10M.subphase == 0 || g_B10M.subphase == 4)) {
        M_Image(12, 1);
    }
    if (g_B10M.frame == 100 || g_B10M.subphase % 4 == 1) {
        if (g_B10M.subphase == 0) {
            g_B10M.subphase =
                (int16_t)((MasterRng_Next(&g_Rng) % (clock ? 2 : 3)) * 4 + 1);
            g_B10M.target_left = Math16_Wrap(Boss_Player()->left - 8);
            g_B10M.angle =
                (uint8_t)(Math16_Angle(
                              Math16_Wrap(400 - g_B10M.eyes[2].top + 28),
                              Math16_Wrap(
                                  g_B10M.target_left - g_B10M.eyes[2].left
                                  + 28))
                          - 16);
            g_B10M.angle_southeast =
                (uint8_t)(Math16_Angle(
                              Math16_Wrap(400 - g_B10M.eyes[3].top + 28),
                              Math16_Wrap(
                                  g_B10M.target_left - g_B10M.eyes[3].left
                                  + 28))
                          + 16);
        }
        if (g_B10M.frame % 8 == 0) {
            M_FireMissiles(c);
        }
        if (g_B10M.frame == 200) {
            g_B10M.subphase++;
            g_B10M.pattern_state = 0;
        }
    }
    if (g_B10M.subphase % 4 == 2) {
        if (g_B10M.frame % 8 == 1) {
            g_B10M.pattern_state = Math16_Wrap(g_B10M.pattern_state + 1);
            if (clock) {
                const int16_t delta = g_B10M.subphase == 2 ? -2 : 2;
                g_B10M.angle = (uint8_t)(g_B10M.angle + delta);
                g_B10M.angle_southeast =
                    (uint8_t)(g_B10M.angle_southeast + delta);
            } else {
                g_B10M.angle = (uint8_t)(g_B10M.angle - 2);
                g_B10M.angle_southeast = (uint8_t)(g_B10M.angle_southeast + 2);
            }
            M_FireMissiles(c);
            if (g_B10M.pattern_state > 10) {
                g_B10M.subphase++;
                g_B10M.pattern_state = 0;
            }
        }
    } else if (g_B10M.subphase % 4 == 3 && g_B10M.frame % 8 == 1) {
        g_B10M.pattern_state = Math16_Wrap(g_B10M.pattern_state + 1);
        if (clock) {
            const int16_t delta = g_B10M.subphase == 3 ? 3 : -3;
            g_B10M.angle = (uint8_t)(g_B10M.angle + delta);
            g_B10M.angle_southeast = (uint8_t)(g_B10M.angle_southeast + delta);
        } else if (g_B10M.subphase == 3) {
            g_B10M.angle = (uint8_t)(g_B10M.angle + 4);
        } else if (g_B10M.subphase == 7) {
            g_B10M.angle_southeast = (uint8_t)(g_B10M.angle_southeast - 4);
        } else if (g_B10M.subphase == 11) {
            g_B10M.angle = (uint8_t)(g_B10M.angle + 2);
            g_B10M.angle_southeast = (uint8_t)(g_B10M.angle_southeast - 2);
        }
        M_FireMissiles(c);
        if (g_B10M.pattern_state > 10) {
            g_B10M.subphase = 0;
            g_B10M.pattern_state = 0;
            g_B10M.frame = 0;
            g_B10M.iterations = M_Byte(g_B10M.iterations + 1);
        }
    }
}

static void M_Spray(const B10M_CONTEXT *const c)
{
    g_B10M.pattern_state = Math16_Wrap(g_B10M.pattern_state + 1);
    g_B10M.angle--;
    M_Lock(c, 3);
    M_Track(c, 3);
    if (g_B10M.frame == 100 && g_B10M.subphase == 0) {
        g_B10M.pattern_state = 0;
        g_B10M.subphase = 1;
    }
    if (g_B10M.subphase == 1) {
        if (g_B10M.pattern_state % g_B10M.pattern_interval == 0) {
            M_Single(c, 0, (uint8_t)(128 - g_B10M.angle), PELLET_REGULAR);
            M_Single(c, 1, g_B10M.angle, PELLET_REGULAR);
        }
        if (g_B10M.pattern_state / 5 >= 36) {
            g_B10M.subphase = 2;
            g_B10M.frame = 0;
        }
    }
    if (g_B10M.subphase == 2 && g_B10M.frame > 50) {
        g_B10M.pattern_state = -15;
        g_B10M.subphase = 3;
        g_B10M.angle = 128;
    }
    if (g_B10M.subphase == 3) {
        if (g_B10M.pattern_state % g_B10M.pattern_interval == 0) {
            M_Single(
                c, 0, (uint8_t)(128 - g_B10M.angle),
                PELLET_FALL_STRAIGHT_FROM_TOP_THEN_REGULAR);
            M_Single(
                c, 1, g_B10M.angle, PELLET_FALL_STRAIGHT_FROM_TOP_THEN_REGULAR);
        }
        if (g_B10M.pattern_state / 5 >= 20) {
            g_B10M.subphase = 0;
            g_B10M.frame = 0;
            g_B10M.iterations = M_Byte(g_B10M.iterations + 1);
            g_B10M.angle = 0;
        }
    }
}

static void M_Laser(
    const B10M_CONTEXT *const c, const bool left, const bool restore_only)
{
    const int16_t x = Math16_Wrap(g_B10M.eyes[4].left + 28);
    const int16_t y = Math16_Wrap(g_B10M.eyes[4].top + 28);
    const int16_t end =
        left ? Math16_Wrap(640 - g_B10M.pattern_state) : g_B10M.pattern_state;
    if (restore_only) {
        LineBullets_Erase(x, y, end, 400);
    } else {
        LineBullets_Erase(x, y, Math16_Wrap(end - (left ? -16 : 16)), 400);
        LineBullets_PutPatternedAndHitTest(x, y, end, 400, 7, 0xe0);
    }
}

static void M_Lasers(const B10M_CONTEXT *const c)
{
    M_Lock(c, 16);
    if (g_B10M.frame >= 30 && g_B10M.subphase % 2 == 0) {
        g_B10M.eyes[4].image = 1;
    }
    const int16_t iteration = g_B10M.subphase / 2;
    if (!((g_B10M.frame == 40 && g_B10M.subphase % 2 == 0)
          || g_B10M.subphase % 2 == 1)
        || iteration > 2) {
        return;
    }
    if (g_B10M.subphase % 2 == 0) {
        g_B10M.subphase++;
        g_B10M.pentagram.angle = 0;
        if (iteration != 0) {
            g_B10M.pattern_state = 0;
        }
    }
    if (g_B10M.frame % 4 != 1) {
        return;
    }
    if (iteration == 2) {
        g_B10M.eyes[4].image = 6;
        M_Laser(c, false, false);
        M_Laser(c, true, false);
        if (g_B10M.pattern_state >= 288) {
            M_Laser(c, false, true);
            M_Laser(c, true, true);
            g_B10M.subphase = 6;
            g_B10M.frame = 0;
            g_B10M.pattern_state = 0;
            if (g_Session.settings.difficulty == SETTINGS_DIFFICULTY_HARD) {
                M_Group(c, 4, PELLET_GROUP_THREE_NARROW_AIMED, 3, true);
            }
            if (g_Session.settings.difficulty == SETTINGS_DIFFICULTY_LUNATIC) {
                M_Group(c, 4, PELLET_GROUP_FIVE_NARROW_AIMED, 4, true);
            }
        }
    } else {
        g_B10M.eyes[4].image = g_B10M.pattern_state <= 256
            ? (iteration == 1 ? 5 : 4)
            : g_B10M.pattern_state >= 384 ? (iteration == 1 ? 4 : 5)
                                          : 3;
        M_Laser(c, iteration == 1, false);
        if (g_B10M.pattern_state >= 592) {
            M_Laser(c, iteration == 1, true);
            g_B10M.subphase = (int16_t)((iteration + 1) * 2);
            g_B10M.frame = 0;
            g_B10M.pattern_state = 0;
            if (g_Session.settings.difficulty > SETTINGS_DIFFICULTY_NORMAL) {
                M_Group(c, 4, PELLET_GROUP_THREE_NARROW_AIMED, 3, true);
            }
        }
    }
    g_B10M.pattern_state = Math16_Wrap(g_B10M.pattern_state + 16);
}

static MATH16_VECTOR M_ShrinkPoint(const int32_t i, const int16_t distance)
{
    int16_t x = Math16_Wrap(g_B10M.eyes[i].left + 28);
    int16_t y = Math16_Wrap(g_B10M.eyes[i].top + 28);
    if (i == 0) {
        x = Math16_Wrap(x + distance);
    } else if (i == 1) {
        x = Math16_Wrap(x - distance);
    } else if (i == 2) {
        x = Math16_Wrap(x + distance / 3);
    } else if (i == 3) {
        x = Math16_Wrap(x - distance / 3);
    }
    y = Math16_Wrap(y + distance / (i < 2 ? 3 : 2));
    return (MATH16_VECTOR) { x, y };
}

static void M_Shrink(
    const B10M_CONTEXT *const c, const int16_t distance, const bool restore)
{
    const int32_t order[] = { 4, 2, 1, 0, 3, 4 };
    for (int32_t i = 0; i < 5; i++) {
        const MATH16_VECTOR a = M_ShrinkPoint(order[i], distance);
        const MATH16_VECTOR z = M_ShrinkPoint(order[i + 1], distance);
        if (restore) {
            LineBullets_Erase(a.x, a.y, z.x, z.y);
        } else {
            LineBullets_PutAndHitTest(a.x, a.y, z.x, z.y, 7);
        }
    }
}

static void M_Star(const B10M_CONTEXT *const c, const bool restore)
{
    const int32_t order[] = { 0, 2, 4, 1, 3, 0 };
    const B10M_PENTAGRAM *const p = &g_B10M.pentagram;
    for (int32_t i = 0; i < 5; i++) {
        const int32_t a = order[i], z = order[i + 1];
        if (restore) {
            LineBullets_Erase(p->x[a], p->y[a], p->x[z], p->y[z]);
        } else {
            LineBullets_PutAndHitTest(p->x[a], p->y[a], p->x[z], p->y[z], 7);
        }
    }
}

static void M_Corners(const int16_t angle)
{
    for (int32_t i = 0; i < 5; i++) {
        g_B10M.pentagram.x[i] = Math16_PolarX(
            g_B10M.pentagram.center.x, g_B10M.pentagram.radius, angle + i * 51);
        g_B10M.pentagram.y[i] = Math16_PolarY(
            g_B10M.pentagram.center.y, g_B10M.pentagram.radius, angle + i * 51);
    }
}

static void M_Regular(const B10M_CONTEXT *const c, const int16_t angle)
{
    M_Star(c, true);
    M_Corners(angle);
    M_Star(c, false);
}

static void M_EnterStarRise(const B10M_CONTEXT *const c)
{
    M_Next(B10M_PHASE_STAR_RISE);
    g_B10M.subphase = 0;
    g_B10M.iterations = 1;
    g_B10M.pattern_state = 0;
    g_B10M.pentagram.center.y = Math16_Wrap(g_B10M.pentagram.y[0] + 64);
    g_B10M.pentagram.center.x = g_B10M.pentagram.x[0];
    g_B10M.pentagram.radius = 64;
    M_Corners(0);
    g_B10M.pentagram.y[1] = g_B10M.pentagram.y[4];
    g_B10M.pattern_interval = Boss_Rank(24, 14, 10, 8);
}

static void M_SpawnStar(const B10M_CONTEXT *const c)
{
    M_Lock(c, 31);
    M_Track(c, 31);
    if (g_B10M.frame == 100 || g_B10M.subphase == 1) {
        if (g_B10M.subphase == 0) {
            g_B10M.subphase = 1;
            g_B10M.target_left = Boss_Player()->left;
        }
        M_Shrink(c, 0, false);
        if (g_B10M.frame == 140) {
            g_B10M.subphase = 2;
            g_B10M.pattern_state = 0;
        }
    }
    if (g_B10M.subphase == 2 && g_B10M.frame % 4 == 1) {
        M_Shrink(c, g_B10M.pattern_state, true);
        g_B10M.pattern_state = Math16_Wrap(g_B10M.pattern_state + 8);
        M_Shrink(c, g_B10M.pattern_state, false);
        if (g_B10M.pattern_state > 160) {
            const int32_t order[] = { 4, 2, 1, 0, 3 };
            for (int32_t i = 0; i < 5; i++) {
                const MATH16_VECTOR point =
                    M_ShrinkPoint(order[i], g_B10M.pattern_state);
                g_B10M.pentagram.x[i] = point.x;
                g_B10M.pentagram.y[i] = point.y;
            }
            g_B10M.subphase = 3;
        }
    }
}

static void M_Spin(const bool clockwise)
{
    const int16_t delta = Math16_Wrap(g_B10M.frame / 64 + 2);
    g_B10M.pentagram.angle =
        Math16_Wrap(g_B10M.pentagram.angle + (clockwise ? delta : -delta));
}

static void M_Aim(const B10M_CONTEXT *const c)
{
    g_B10M.pentagram.velocity = Math16_Aim(
        g_B10M.pentagram.center, (MATH16_VECTOR) { Boss_Player()->left, 400 },
        4, 0);
}

static void M_Back(const B10M_CONTEXT *const c, const int16_t phase)
{
    g_B10M.pentagram.phase = phase;
    M_Next(B10M_PHASE_TO_STAR);
    g_B10M.subphase = 3;
    g_B10M.iterations = 3;
    g_B10M.pattern_state = 0;
    M_Star(c, true);
}

static bool M_SlamStar(const B10M_CONTEXT *const c, const int16_t threshold)
{
    if (g_B10M.frame % 4 == 0) {
        if (g_B10M.pentagram.center.y < 400 - threshold) {
            M_Aim(c);
        }
        M_Spin(true);
        g_B10M.pentagram.center.x = Math16_Wrap(
            g_B10M.pentagram.center.x + g_B10M.pentagram.velocity.x);
        g_B10M.pentagram.center.y = Math16_Wrap(
            g_B10M.pentagram.center.y + g_B10M.pentagram.velocity.y);
        M_Regular(c, g_B10M.pentagram.angle);
    }
    return g_B10M.pentagram.center.y >= 420;
}

static void M_AttackStar(const B10M_CONTEXT *const c)
{
    g_B10M.pattern_state = Math16_Wrap(g_B10M.pattern_state + 1);
    M_Lock(c, (uint8_t)g_B10M.iterations);
    if (g_B10M.hit.invincible && g_B10M.after_hit_frame == 0) {
        g_B10M.iterations = M_Byte(g_B10M.iterations * 2);
        g_B10M.after_hit_frame = 1;
        if (g_B10M.iterations == 32) {
            g_B10M.iterations = 1;
        }
    } else if (g_B10M.after_hit_frame > 0) {
        M_Toggle(
            c, g_B10M.iterations == 1 ? 16 : (uint8_t)(g_B10M.iterations >> 1),
            (uint8_t)g_B10M.iterations, 3, 3, &g_B10M.after_hit_frame);
        if (g_B10M.after_hit_frame >= 70) {
            g_B10M.after_hit_frame = 0;
            g_B10M.pattern_state = 0;
        }
    } else {
        M_Track(c, (uint8_t)g_B10M.iterations);
    }
    B10M_PENTAGRAM *const p = &g_B10M.pentagram;
    if (g_B10M.subphase == 0 && g_B10M.frame % g_B10M.pattern_interval == 0
        && p->phase != B10M_STAR_SLAM && p->phase != B10M_STAR_GROWN_SLAM) {
        for (int32_t i = 0; i < 5; i++) {
            g_B10M.angle = Math16_Angle(
                Math16_Wrap(p->y[i] - p->center.y),
                Math16_Wrap(p->x[i] - p->center.x));
            Pellets_AddSingle(
                g_Session.settings.difficulty, p->x[i], p->y[i], g_B10M.angle,
                48, PELLET_REGULAR, 0, 0, 0);
        }
    }
    if (g_B10M.frame == 100
        && (p->phase == B10M_STAR_WAIT || p->phase == B10M_STAR_RETURN)) {
        p->phase++;
        g_B10M.frame = 0;
    }
    if (p->phase == B10M_STAR_SPIN_1 || p->phase == B10M_STAR_SPIN_2
        || p->phase == B10M_STAR_SPIN_3) {
        if (g_B10M.frame % 4 == 0) {
            M_Spin(true);
            M_Regular(c, p->angle);
        }
        if (g_B10M.frame >= 200) {
            p->phase++;
            g_B10M.frame = 0;
        }
    }
    if (p->phase == B10M_STAR_REVERSE_1 || p->phase == B10M_STAR_REVERSE_2) {
        if (g_B10M.frame % 4 == 0) {
            M_Spin(false);
            M_Regular(c, p->angle);
        }
        if (g_B10M.frame >= 200) {
            p->phase++;
            g_B10M.frame = 0;
            M_Aim(c);
        }
    } else if (p->phase == B10M_STAR_SLAM) {
        if (M_SlamStar(c, 100)) {
            M_Back(c, B10M_STAR_RETURN);
        }
    } else if (p->phase == B10M_STAR_GROW) {
        if (g_B10M.frame % 4 == 0) {
            p->radius = Math16_Wrap(p->radius + 1);
            M_Spin(true);
            M_Regular(c, p->angle);
        }
        if (g_B10M.frame >= 200) {
            p->phase = B10M_STAR_GROWN_SLAM;
            g_B10M.frame = 0;
            M_Aim(c);
        }
    }
    if (p->phase == B10M_STAR_GROWN_SLAM && M_SlamStar(c, 140)) {
        M_Back(c, B10M_STAR_WAIT);
    }
}

static bool M_Death(const B10M_CONTEXT *const c)
{
    const BOSS_DEFEAT_CONTEXT defeat = {
        .post_defeat = &g_B10M.post_defeat,
        .miko = c->miko,
        .origin_top = &g_Orb.top,
    };
    if (!BossDefeat_Resume(&g_B10M.defeat, &defeat)) {
        return false;
    }
    Scene_Activate(&c->assets->next_scene);
    if (g_B10M.phase == B10M_PHASE_STAR_SPAWN && !g_B10M.hit.invincible) {
        M_EnterStarRise(c);
    }
    g_B10M.defeating = false;
    g_B10M.finished = true;
    return true;
}

bool B10M_UpdateAndRender(const B10M_CONTEXT *const c)
{
    if (g_B10M.finished) {
        return true;
    }
    if (g_B10M.defeating) {
        return M_Death(c);
    }
    Missiles_UpdateAndRender(
        (uint16_t)g_Session.frame_rand,
        (MISSILES_OUTPUT) { M_MissileAction, (void *)c });
    if (g_B10M.phase == B10M_PHASE_ENTRANCE) {
        M_Entrance(c);
    } else if (g_B10M.phase == B10M_PHASE_LATERAL) {
        M_Common();
        M_Lateral(c);
        M_Hit(c);
        if (g_B10M.hp <= 15 || g_B10M.frame > 1100) {
            M_Next(B10M_PHASE_TO_MISSILES);
        }
    } else if (
        g_B10M.phase == B10M_PHASE_TO_MISSILES
        || g_B10M.phase == B10M_PHASE_TO_SPRAY
        || g_B10M.phase == B10M_PHASE_TO_MISSILES_CLOCK
        || g_B10M.phase == B10M_PHASE_TO_LASERS
        || g_B10M.phase == B10M_PHASE_TO_STAR) {
        switch (g_B10M.phase) {
        case B10M_PHASE_TO_MISSILES:
            M_Toggle(c, 3, 12, 0, 2, &g_B10M.frame);
            break;
        case B10M_PHASE_TO_SPRAY:
            M_Toggle(c, 12, 3, 1, 0, &g_B10M.frame);
            break;
        case B10M_PHASE_TO_MISSILES_CLOCK:
            M_Toggle(c, 3, 12, 0, 3, &g_B10M.frame);
            break;
        case B10M_PHASE_TO_LASERS:
            M_Toggle(c, 12, 16, 2, 1, &g_B10M.frame);
            break;
        case B10M_PHASE_TO_STAR:
            M_Toggle(
                c, 0, 31, g_B10M.subphase, g_B10M.iterations, &g_B10M.frame);
            break;
        }
        if (g_B10M.frame >= 70) {
            const int8_t previous = g_B10M.phase;
            M_Next((int8_t)(previous + 1));
            g_B10M.subphase = 0;
            if (previous != B10M_PHASE_TO_LASERS) {
                g_B10M.iterations = previous == B10M_PHASE_TO_STAR ? 31 : 0;
            }
            g_B10M.pattern_state = 0;
            if (previous == B10M_PHASE_TO_MISSILES) {
                g_B10M.pattern_interval = Boss_Rank(8, 12, 16, 20);
            } else if (previous == B10M_PHASE_TO_SPRAY) {
                g_B10M.pattern_interval = Boss_Rank(12, 8, 4, 2);
                g_B10M.angle = 0;
            } else if (previous == B10M_PHASE_TO_MISSILES_CLOCK) {
                g_B10M.pattern_interval = Boss_Rank(10, 16, 20, 24);
                g_B10M.pentagram.angle = 0;
            } else {
                g_B10M.pentagram.angle = 0;
            }
            M_Snap(c);
        }
    } else if (
        g_B10M.phase == B10M_PHASE_MISSILES
        || g_B10M.phase == B10M_PHASE_MISSILES_CLOCK) {
        M_Common();
        M_Missiles(c, g_B10M.phase == B10M_PHASE_MISSILES_CLOCK);
        M_Hit(c);
        if (g_B10M.hp <= (g_B10M.phase == B10M_PHASE_MISSILES ? 12 : 8)
            || g_B10M.iterations >= 5) {
            M_Next((int8_t)(g_B10M.phase + 1));
        }
    } else if (g_B10M.phase == B10M_PHASE_SPRAY) {
        M_Common();
        M_Spray(c);
        M_Hit(c);
        if (g_B10M.hp <= 10 || g_B10M.iterations > 4) {
            M_Next(B10M_PHASE_TO_MISSILES_CLOCK);
        }
    } else if (g_B10M.phase == B10M_PHASE_LASERS) {
        M_Common();
        M_Lasers(c);
        M_Hit(c);
        if (g_B10M.hp <= 2 || g_B10M.subphase == 6) {
            M_Next(B10M_PHASE_TO_STAR);
            g_B10M.subphase = 1;
            g_B10M.iterations = 0;
            g_B10M.pentagram.phase = B10M_STAR_WAIT;
        }
    } else if (
        g_B10M.phase == B10M_PHASE_STAR_SPAWN
        || g_B10M.phase == B10M_PHASE_STAR_ATTACK) {
        const bool attack = g_B10M.phase == B10M_PHASE_STAR_ATTACK;
        M_Common();
        if (g_B10M.phase == B10M_PHASE_STAR_SPAWN) {
            M_SpawnStar(c);
        } else {
            M_AttackStar(c);
        }
        M_Hit(c);
        if (g_B10M.hp <= 0) {
            Sound_Fade();
            Pellets_EraseAndResetMoving();
            if (attack) {
                Missiles_Reset();
            }
            ShootoutLasers_EraseAndReset(5);
            BossDefeat_Begin(&g_B10M.defeat, BOSS_DEFEAT_REGULAR);
            g_B10M.defeating = true;
            return M_Death(c);
        }
        if (g_B10M.phase == B10M_PHASE_STAR_SPAWN && g_B10M.subphase == 3
            && !g_B10M.hit.invincible) {
            M_EnterStarRise(c);
        }
    } else if (g_B10M.phase == B10M_PHASE_STAR_RISE) {
        g_B10M.hit.frame = Math16_Wrap(g_B10M.hit.frame + 1);
        M_Toggle(c, 30, 1, 1, 0, &g_B10M.frame);
        if (g_B10M.hit.frame % 4 == 0) {
            g_B10M.pentagram.center.y =
                Math16_Wrap(g_B10M.pentagram.center.y - 3);
            M_Regular(c, 192);
        }
        if (g_B10M.frame >= 70) {
            M_Next(B10M_PHASE_STAR_ATTACK);
            g_B10M.subphase = 0;
            g_B10M.iterations = 1;
            g_B10M.pattern_state = 0;
            g_B10M.pentagram.angle = 0;
            g_B10M.after_hit_frame = 0;
            M_Snap(c);
        }
    }
    return true;
}
