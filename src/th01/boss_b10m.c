#include "th01/boss_b10m.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "th01/line_bullets.h"

#include "core/base/assert.h"

static RESULT M_LoadBos(
    VFS *const vfs, const ALLOCATOR *const a, BOS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS2.BOS", VFS_IN_MEMORY, &reader));
    RESULT result = Bos_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadBackground(
    VFS *const vfs, const ALLOCATOR *const a, PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS2.GRP", VFS_IN_MEMORY, &reader));
    RESULT result = Pi_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

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
    MUST(M_LoadBos(vfs, a, &loaded.eyes));
    MUST(M_LoadBackground(vfs, a, &loaded.background));
    MUST(M_LoadMissiles(vfs, a, &loaded.missiles));
    MUST(Scene_Load(vfs, 3, a, &loaded.next_scene));
    MUST(B10M_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B10M_Setup(B10M *const b, const B10M_CONTEXT *const c)
{
    ASSERT(!b->defeating);
    ASSERT(Result_Ignore(B10M_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->eyes[0], c->sprites, 0, &c->assets->eyes)));
    for (int32_t i = 1; i < 5; i++) {
        BossEntity_CopyMetadata(&b->eyes[i], &b->eyes[0]);
    }
    Palette_Set(c->hit->palette, &c->assets->palette);
    b->boss_palette = c->hit->palette->saved;
    b->post_defeat = c->hit->palette->saved;
    *c->stage_palette = b->post_defeat;
    const int16_t left[] = { 64, 512, 192, 384, 288 };
    const int16_t top[] = { 128, 128, 176, 176, 64 };
    for (int32_t i = 0; i < 5; i++) {
        b->eyes[i].image = 0;
        BossEntity_SetPosition(
            &b->eyes[i], left[i], top[i], (BOSS_BOUNDS) { 0, 736, 64, 304 });
        BossEntity_SetOrbHitbox(&b->eyes[i], (BOSS_BOUNDS) { -4, 68, -4, 48 });
        b->eyes[i].orb_inactive = true;
    }
    b->frame = 0;
    b->phase = 0;
    b->hp = 16;
    b->finished = false;
    c->hit->health->first_white = 12;
    c->hit->health->first_redwhite = 8;
    Particles_Init(c->particles);
    Missiles_Reset(c->missiles);
}

static PLAYER *M_Player(const B10M_CONTEXT *const c)
{
    return c->hit->display->player;
}

static int8_t M_Byte(const int32_t value)
{
    const uint8_t bits = (uint8_t)value;
    return bits < 128 ? (int8_t)bits : (int8_t)((int16_t)bits - 256);
}

static int16_t M_Rank(
    const B10M_CONTEXT *const c, const int16_t e, const int16_t n,
    const int16_t h, const int16_t l)
{
    switch (c->hit->display->rank) {
    case SETTINGS_DIFFICULTY_EASY:
        return e;
    case SETTINGS_DIFFICULTY_HARD:
        return h;
    case SETTINGS_DIFFICULTY_LUNATIC:
        return l;
    default:
        return n;
    }
}

static void M_Lock(
    B10M *const b, const B10M_CONTEXT *const c, const uint8_t flags)
{
    for (int32_t i = 0; i < 5; i++) {
        if ((flags & (1 << i)) != 0) {
            BossEntity_LockedMovePut(&b->eyes[i], c->sprites, 0, 0, 3, true);
        }
    }
}

static void M_Track(
    B10M *const b, const B10M_CONTEXT *const c, const uint8_t flags)
{
    for (int32_t i = 0; i < 5; i++) {
        BOSS_ENTITY *const e = &b->eyes[i];
        if ((flags & (1 << i)) != 0) {
            const int16_t center = Math16_Wrap(e->left + 32);
            e->image = Math16_Wrap(center - M_Player(c)->left) > 32 ? 4
                : Math16_Wrap(M_Player(c)->left - center) > 32      ? 5
                                                                    : 3;
        }
    }
}

static void M_Image(B10M *const b, const uint8_t flags, const int16_t image)
{
    for (int32_t i = 0; i < 5; i++) {
        if ((flags & (1 << i)) != 0) {
            b->eyes[i].image = image;
        }
    }
}

static void M_Active(B10M *const b, const uint8_t flags, const bool active)
{
    for (int32_t i = 0; i < 5; i++) {
        if ((flags & (1 << i)) != 0) {
            b->eyes[i].orb_inactive = !active;
        }
    }
}

static void M_Next(B10M *const b, const int8_t phase)
{
    b->phase = phase;
    b->frame = 0;
    b->hit.frame = 0;
}

static void M_Common(B10M *const b)
{
    b->frame = Math16_Wrap(b->frame + 1);
    b->hit.frame = Math16_Wrap(b->hit.frame + 1);
}

static void M_Snap(B10M *const b, const B10M_CONTEXT *const c)
{
    Palette_Set(c->hit->palette, c->stage_palette);
    b->boss_palette = c->hit->palette->saved;
}

static void M_Group(
    const B10M *const b, const B10M_CONTEXT *const c, const int32_t eye,
    const PELLET_GROUP group, const int16_t speed, const bool iris)
{
    Pellets_AddGroup(
        c->pellets, c->pellet_shared, M_Player(c), c->rng,
        c->hit->display->rank, Math16_Wrap(b->eyes[eye].left + 28),
        Math16_Wrap(b->eyes[eye].top + (iris ? 28 : 32)), group, speed);
}

static void M_Single(
    const B10M *const b, const B10M_CONTEXT *const c, const int32_t eye,
    const uint8_t angle, const PELLET_MOTION motion)
{
    Pellets_AddSingle(
        c->pellets, c->pellet_shared, c->hit->display->rank,
        Math16_Wrap(b->eyes[eye].left + 28), Math16_Wrap(b->eyes[eye].top + 32),
        angle, 64, motion, motion == PELLET_REGULAR ? 0 : 64, 0, 0);
}

static void M_Hit(B10M *const b, const B10M_CONTEXT *const c)
{
    bool colliding = false;
    for (int32_t i = 0; i < 5; i++) {
        if (BossEntity_HitTestOrb(&b->eyes[i], c->hit->orb)
            && b->eyes[i].image != 0) {
            colliding = true;
            break;
        }
    }
    BOSS_HIT_CONTEXT hit = *c->hit;
    hit.hp = &b->hp;
    hit.boss_palette = &b->boss_palette;
    const uint8_t flash[] = { 1, 11 };
    BossHit_UpdateAndRender(
        &b->hit, &hit, flash, 2, 5000, colliding, (BOSS_SHOT_HITBOX) {});
}

static void M_MissileAction(void *const user, const MISSILES_ACTION action)
{
    const B10M_CONTEXT *const c = user;
    if (action.kind == MISSILES_ACTION_EFFECT) {
        c->hit->effect(c->hit->user, action.effect);
    } else {
        Missiles_RenderAction(&c->assets->missiles, action);
    }
}

static void M_Toggle(
    B10M *const b, const B10M_CONTEXT *const c, const uint8_t close,
    const uint8_t open, const int16_t dec, const int16_t inc,
    int16_t *const frame)
{
    M_Lock(b, c, close | open);
    *frame = Math16_Wrap(*frame + 1);
    if (*frame % 10 != 0) {
        if (dec < 3 && c->hit->palette->saved.colors[15][dec] > 0) {
            c->hit->palette->saved.colors[15][dec]--;
            c->stage_palette->colors[15][dec] =
                M_Byte(c->stage_palette->colors[15][dec] - 1);
        }
        if (inc < 3) {
            c->hit->palette->saved.colors[15][inc] =
                M_Byte(c->hit->palette->saved.colors[15][inc] + 1);
            c->stage_palette->colors[15][inc] =
                M_Byte(c->stage_palette->colors[15][inc] + 1);
        }
        Palette_Set(c->hit->palette, c->stage_palette);
    }
    if (*frame == 20) {
        M_Image(b, close, 2);
        M_Image(b, open, 1);
        M_Active(b, open, true);
    } else if (*frame == 40) {
        M_Image(b, close, 1);
        M_Image(b, open, 2);
        M_Active(b, close, false);
    } else if (*frame == 60) {
        M_Image(b, close, 0);
        M_Image(b, open, 6);
    }
}

static void M_DownLaser(
    const B10M *const b, const B10M_CONTEXT *const c, const int32_t i,
    const bool put)
{
    const int16_t x = Math16_Wrap(b->eyes[i].left + 28);
    const int16_t y = Math16_Wrap(b->eyes[i].top + 28);
    if (put) {
        Video_VerticalLine(x, y, 398, 7);
    } else {
        Video_LineErase(x, y, x, 400);
    }
}

static void M_Entrance(B10M *const b, const B10M_CONTEXT *const c)
{
    if (!b->hp_bar_filled) {
        b->hp_bar_filled = HudHp_Increment(c->hit->health, b->hp, b->frame);
    }
    b->frame = Math16_Wrap(b->frame + 1);
    M_Lock(b, c, 31);
    for (int32_t i = 0; i < 5; i++) {
        const int16_t starts[] = { 160, 160, 180, 180, 200 };
        const int16_t ends[] = { 300, 300, 320, 320, 32767 };
        if (b->frame > starts[i] && b->frame < ends[i]) {
            M_DownLaser(b, c, i, true);
            const int16_t x = Math16_Wrap(b->eyes[i].left + 28);
            const int16_t width = i == 1 ? 8 : 32;
            if (M_Player(c)->left > Math16_Wrap(x - 24)
                && M_Player(c)->left < Math16_Wrap(x + 24 - width)) {
                M_Player(c)->is_hit = true;
            }
        }
    }
    if (M_Player(c)->invincible) {
        M_Player(c)->is_hit = false;
    }
    if (b->frame % 40 == 0) {
        c->stage_palette->colors[15][0] =
            M_Byte(c->stage_palette->colors[15][0] + 1);
        c->stage_palette->colors[15][1] =
            M_Byte(c->stage_palette->colors[15][1] + 1);
        Palette_Set(c->hit->palette, c->stage_palette);
    }
    const int16_t open_at[] = { 100, 120, 140, 160, 180, 200 };
    bool opening = false;
    for (int32_t i = 0; i < 6; i++) {
        opening |= b->frame == open_at[i];
    }
    if (opening) {
        const int16_t starts[] = { 100, 100, 120, 120, 140 };
        for (int32_t i = 0; i < 5; i++) {
            const int16_t delta = Math16_Wrap(b->frame - starts[i]);
            if (delta >= 0 && delta <= 60) {
                b->eyes[i].image = delta < 20 ? 1
                    : delta < 40              ? 2
                    : delta < 60              ? 6
                                              : 3;
            }
        }
        if (b->frame == 100) {
            M_Active(b, 3, true);
        } else if (b->frame == 120) {
            M_Active(b, 12, true);
        } else if (b->frame == 140) {
            M_Active(b, 16, true);
        }
    } else if (b->frame == 240 || b->frame == 260 || b->frame == 280) {
        M_Image(b, 31, b->frame == 240 ? 2 : b->frame == 260 ? 1 : 0);
        if (b->frame == 240) {
            M_Active(b, 31, false);
        }
        if ((b->frame == 240
             && c->hit->display->rank == SETTINGS_DIFFICULTY_HARD)
            || (b->frame == 260
                && c->hit->display->rank == SETTINGS_DIFFICULTY_LUNATIC)) {
            M_Group(b, c, 4, PELLET_GROUP_FIVE_WIDE_AIMED, 54, true);
            M_Group(b, c, 3, PELLET_GROUP_FIVE_WIDE_AIMED, 54, true);
            M_Group(b, c, 2, PELLET_GROUP_FIVE_WIDE_AIMED, 54, true);
            if (c->hit->display->rank == SETTINGS_DIFFICULTY_LUNATIC) {
                M_Group(b, c, 0, PELLET_GROUP_FIVE_WIDE_AIMED, 54, true);
                M_Group(b, c, 1, PELLET_GROUP_FIVE_WIDE_AIMED, 54, true);
            }
        }
    } else if (b->frame == 300) {
        M_Image(b, 3, 1);
        M_DownLaser(b, c, 0, false);
        M_DownLaser(b, c, 1, false);
    } else if (b->frame == 320) {
        M_Image(b, 3, 2);
        M_Active(b, 3, true);
        M_DownLaser(b, c, 2, false);
        M_DownLaser(b, c, 3, false);
    } else if (b->frame > 330) {
        b->phase = 1;
        b->hit.invincible = false;
        b->frame = 0;
        M_Snap(b, c);
        b->hp_bar_filled = false;
        b->eyes[0].image = 4;
        b->eyes[1].image = 3;
        b->pattern_interval = M_Rank(c, 350, 300, 200, 130);
        M_DownLaser(b, c, 4, false);
    }
}

static void M_Lateral(B10M *const b, const B10M_CONTEXT *const c)
{
    M_Lock(b, c, 3);
    M_Track(b, c, 3);
    const int16_t frame = b->frame % b->pattern_interval;
    const int16_t times[] = { 10, 25, 40, 60, 80, 110 };
    const int16_t speeds[] = { 24, 28, 32, 44, 50, 68 };
    for (int32_t i = 0; i < 6; i++) {
        if (frame == times[i]) {
            M_Group(
                b, c, 0,
                i == 5 ? PELLET_GROUP_SINGLE_AIMED
                       : PELLET_GROUP_TWO_WIDE_AIMED,
                speeds[i], false);
            M_Group(
                b, c, 1,
                i == 5 ? PELLET_GROUP_SINGLE_AIMED
                       : PELLET_GROUP_TWO_WIDE_AIMED,
                speeds[i], false);
            break;
        }
    }
}

static void M_FireMissiles(B10M *const b, const B10M_CONTEXT *const c)
{
    const uint8_t angles[] = { b->angle, b->angle_southeast };
    for (int32_t i = 0; i < 2; i++) {
        const MATH16_VECTOR v = Math16_Vector(8, angles[i]);
        Missiles_Add(
            c->missiles, Math16_Wrap(b->eyes[i + 2].left + 28),
            Math16_Wrap(b->eyes[i + 2].top + 28), v.x, v.y, 0);
    }
}

static void M_Missiles(
    B10M *const b, const B10M_CONTEXT *const c, const bool clock)
{
    M_Lock(b, c, 12);
    M_Track(b, c, 12);
    if (b->frame >= 90 && (b->subphase == 0 || b->subphase == 4)) {
        M_Image(b, 12, 1);
    }
    if (b->frame == 100 || b->subphase % 4 == 1) {
        if (b->subphase == 0) {
            b->subphase =
                (int16_t)((MasterRng_Next(c->rng) % (clock ? 2 : 3)) * 4 + 1);
            b->target_left = Math16_Wrap(M_Player(c)->left - 8);
            b->angle = (uint8_t)(Math16_Angle(
                                     Math16_Wrap(400 - b->eyes[2].top + 28),
                                     Math16_Wrap(
                                         b->target_left - b->eyes[2].left + 28))
                                 - 16);
            b->angle_southeast =
                (uint8_t)(Math16_Angle(
                              Math16_Wrap(400 - b->eyes[3].top + 28),
                              Math16_Wrap(
                                  b->target_left - b->eyes[3].left + 28))
                          + 16);
        }
        if (b->frame % 8 == 0) {
            M_FireMissiles(b, c);
        }
        if (b->frame == 200) {
            b->subphase++;
            b->pattern_state = 0;
        }
    }
    if (b->subphase % 4 == 2) {
        if (b->frame % 8 == 1) {
            b->pattern_state = Math16_Wrap(b->pattern_state + 1);
            if (clock) {
                const int16_t delta = b->subphase == 2 ? -2 : 2;
                b->angle = (uint8_t)(b->angle + delta);
                b->angle_southeast = (uint8_t)(b->angle_southeast + delta);
            } else {
                b->angle = (uint8_t)(b->angle - 2);
                b->angle_southeast = (uint8_t)(b->angle_southeast + 2);
            }
            M_FireMissiles(b, c);
            if (b->pattern_state > 10) {
                b->subphase++;
                b->pattern_state = 0;
            }
        }
    } else if (b->subphase % 4 == 3 && b->frame % 8 == 1) {
        b->pattern_state = Math16_Wrap(b->pattern_state + 1);
        if (clock) {
            const int16_t delta = b->subphase == 3 ? 3 : -3;
            b->angle = (uint8_t)(b->angle + delta);
            b->angle_southeast = (uint8_t)(b->angle_southeast + delta);
        } else if (b->subphase == 3) {
            b->angle = (uint8_t)(b->angle + 4);
        } else if (b->subphase == 7) {
            b->angle_southeast = (uint8_t)(b->angle_southeast - 4);
        } else if (b->subphase == 11) {
            b->angle = (uint8_t)(b->angle + 2);
            b->angle_southeast = (uint8_t)(b->angle_southeast - 2);
        }
        M_FireMissiles(b, c);
        if (b->pattern_state > 10) {
            b->subphase = 0;
            b->pattern_state = 0;
            b->frame = 0;
            b->iterations = M_Byte(b->iterations + 1);
        }
    }
}

static void M_Spray(B10M *const b, const B10M_CONTEXT *const c)
{
    b->pattern_state = Math16_Wrap(b->pattern_state + 1);
    b->angle--;
    M_Lock(b, c, 3);
    M_Track(b, c, 3);
    if (b->frame == 100 && b->subphase == 0) {
        b->pattern_state = 0;
        b->subphase = 1;
    }
    if (b->subphase == 1) {
        if (b->pattern_state % b->pattern_interval == 0) {
            M_Single(b, c, 0, (uint8_t)(128 - b->angle), PELLET_REGULAR);
            M_Single(b, c, 1, b->angle, PELLET_REGULAR);
        }
        if (b->pattern_state / 5 >= 36) {
            b->subphase = 2;
            b->frame = 0;
        }
    }
    if (b->subphase == 2 && b->frame > 50) {
        b->pattern_state = -15;
        b->subphase = 3;
        b->angle = 128;
    }
    if (b->subphase == 3) {
        if (b->pattern_state % b->pattern_interval == 0) {
            M_Single(
                b, c, 0, (uint8_t)(128 - b->angle),
                PELLET_FALL_STRAIGHT_FROM_TOP_THEN_REGULAR);
            M_Single(
                b, c, 1, b->angle, PELLET_FALL_STRAIGHT_FROM_TOP_THEN_REGULAR);
        }
        if (b->pattern_state / 5 >= 20) {
            b->subphase = 0;
            b->frame = 0;
            b->iterations = M_Byte(b->iterations + 1);
            b->angle = 0;
        }
    }
}

static void M_Laser(
    B10M *const b, const B10M_CONTEXT *const c, const bool left,
    const bool restore_only)
{
    const int16_t x = Math16_Wrap(b->eyes[4].left + 28);
    const int16_t y = Math16_Wrap(b->eyes[4].top + 28);
    const int16_t end =
        left ? Math16_Wrap(640 - b->pattern_state) : b->pattern_state;
    if (restore_only) {
        LineBullets_Erase(x, y, end, 400);
    } else {
        LineBullets_Erase(x, y, Math16_Wrap(end - (left ? -16 : 16)), 400);
        LineBullets_PutPatternedAndHitTest(
            M_Player(c), x, y, end, 400, 7, 0xe0);
    }
}

static void M_Lasers(B10M *const b, const B10M_CONTEXT *const c)
{
    M_Lock(b, c, 16);
    if (b->frame >= 30 && b->subphase % 2 == 0) {
        b->eyes[4].image = 1;
    }
    const int16_t iteration = b->subphase / 2;
    if (!((b->frame == 40 && b->subphase % 2 == 0) || b->subphase % 2 == 1)
        || iteration > 2) {
        return;
    }
    if (b->subphase % 2 == 0) {
        b->subphase++;
        b->pentagram.angle = 0;
        if (iteration != 0) {
            b->pattern_state = 0;
        }
    }
    if (b->frame % 4 != 1) {
        return;
    }
    if (iteration == 2) {
        b->eyes[4].image = 6;
        M_Laser(b, c, false, false);
        M_Laser(b, c, true, false);
        if (b->pattern_state >= 288) {
            M_Laser(b, c, false, true);
            M_Laser(b, c, true, true);
            b->subphase = 6;
            b->frame = 0;
            b->pattern_state = 0;
            if (c->hit->display->rank == SETTINGS_DIFFICULTY_HARD) {
                M_Group(b, c, 4, PELLET_GROUP_THREE_NARROW_AIMED, 3, true);
            }
            if (c->hit->display->rank == SETTINGS_DIFFICULTY_LUNATIC) {
                M_Group(b, c, 4, PELLET_GROUP_FIVE_NARROW_AIMED, 4, true);
            }
        }
    } else {
        b->eyes[4].image = b->pattern_state <= 256 ? (iteration == 1 ? 5 : 4)
            : b->pattern_state >= 384              ? (iteration == 1 ? 4 : 5)
                                                   : 3;
        M_Laser(b, c, iteration == 1, false);
        if (b->pattern_state >= 592) {
            M_Laser(b, c, iteration == 1, true);
            b->subphase = (int16_t)((iteration + 1) * 2);
            b->frame = 0;
            b->pattern_state = 0;
            if (c->hit->display->rank > SETTINGS_DIFFICULTY_NORMAL) {
                M_Group(b, c, 4, PELLET_GROUP_THREE_NARROW_AIMED, 3, true);
            }
        }
    }
    b->pattern_state = Math16_Wrap(b->pattern_state + 16);
}

static MATH16_VECTOR M_ShrinkPoint(
    const B10M *const b, const int32_t i, const int16_t distance)
{
    int16_t x = Math16_Wrap(b->eyes[i].left + 28);
    int16_t y = Math16_Wrap(b->eyes[i].top + 28);
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
    const B10M *const b, const B10M_CONTEXT *const c, const int16_t distance,
    const bool restore)
{
    const int32_t order[] = { 4, 2, 1, 0, 3, 4 };
    for (int32_t i = 0; i < 5; i++) {
        const MATH16_VECTOR a = M_ShrinkPoint(b, order[i], distance);
        const MATH16_VECTOR z = M_ShrinkPoint(b, order[i + 1], distance);
        if (restore) {
            LineBullets_Erase(a.x, a.y, z.x, z.y);
        } else {
            LineBullets_PutAndHitTest(M_Player(c), a.x, a.y, z.x, z.y, 7);
        }
    }
}

static void M_Star(
    const B10M *const b, const B10M_CONTEXT *const c, const bool restore)
{
    const int32_t order[] = { 0, 2, 4, 1, 3, 0 };
    const B10M_PENTAGRAM *const p = &b->pentagram;
    for (int32_t i = 0; i < 5; i++) {
        const int32_t a = order[i], z = order[i + 1];
        if (restore) {
            LineBullets_Erase(p->x[a], p->y[a], p->x[z], p->y[z]);
        } else {
            LineBullets_PutAndHitTest(
                M_Player(c), p->x[a], p->y[a], p->x[z], p->y[z], 7);
        }
    }
}

static void M_Corners(B10M *const b, const int16_t angle)
{
    for (int32_t i = 0; i < 5; i++) {
        b->pentagram.x[i] = Math16_PolarX(
            b->pentagram.center.x, b->pentagram.radius, angle + i * 51);
        b->pentagram.y[i] = Math16_PolarY(
            b->pentagram.center.y, b->pentagram.radius, angle + i * 51);
    }
}

static void M_Regular(
    B10M *const b, const B10M_CONTEXT *const c, const int16_t angle)
{
    M_Star(b, c, true);
    M_Corners(b, angle);
    M_Star(b, c, false);
}

static void M_Enter12(B10M *const b, const B10M_CONTEXT *const c)
{
    M_Next(b, 12);
    b->subphase = 0;
    b->iterations = 1;
    b->pattern_state = 0;
    b->pentagram.center.y = Math16_Wrap(b->pentagram.y[0] + 64);
    b->pentagram.center.x = b->pentagram.x[0];
    b->pentagram.radius = 64;
    M_Corners(b, 0);
    b->pentagram.y[1] = b->pentagram.y[4];
    b->pattern_interval = M_Rank(c, 24, 14, 10, 8);
}

static void M_SpawnStar(B10M *const b, const B10M_CONTEXT *const c)
{
    M_Lock(b, c, 31);
    M_Track(b, c, 31);
    if (b->frame == 100 || b->subphase == 1) {
        if (b->subphase == 0) {
            b->subphase = 1;
            b->target_left = M_Player(c)->left;
        }
        M_Shrink(b, c, 0, false);
        if (b->frame == 140) {
            b->subphase = 2;
            b->pattern_state = 0;
        }
    }
    if (b->subphase == 2 && b->frame % 4 == 1) {
        M_Shrink(b, c, b->pattern_state, true);
        b->pattern_state = Math16_Wrap(b->pattern_state + 8);
        M_Shrink(b, c, b->pattern_state, false);
        if (b->pattern_state > 160) {
            const int32_t order[] = { 4, 2, 1, 0, 3 };
            for (int32_t i = 0; i < 5; i++) {
                const MATH16_VECTOR point =
                    M_ShrinkPoint(b, order[i], b->pattern_state);
                b->pentagram.x[i] = point.x;
                b->pentagram.y[i] = point.y;
            }
            b->subphase = 3;
        }
    }
}

static void M_Spin(B10M *const b, const bool clockwise)
{
    const int16_t delta = Math16_Wrap(b->frame / 64 + 2);
    b->pentagram.angle =
        Math16_Wrap(b->pentagram.angle + (clockwise ? delta : -delta));
}

static void M_Aim(B10M *const b, const B10M_CONTEXT *const c)
{
    b->pentagram.velocity = Math16_Aim(
        b->pentagram.center, (MATH16_VECTOR) { M_Player(c)->left, 400 }, 4, 0);
}

static void M_Back(
    B10M *const b, const B10M_CONTEXT *const c, const int16_t phase)
{
    b->pentagram.phase = phase;
    M_Next(b, 10);
    b->subphase = 3;
    b->iterations = 3;
    b->pattern_state = 0;
    M_Star(b, c, true);
}

static bool M_SlamStar(
    B10M *const b, const B10M_CONTEXT *const c, const int16_t threshold)
{
    if (b->frame % 4 == 0) {
        if (b->pentagram.center.y < 400 - threshold) {
            M_Aim(b, c);
        }
        M_Spin(b, true);
        b->pentagram.center.x =
            Math16_Wrap(b->pentagram.center.x + b->pentagram.velocity.x);
        b->pentagram.center.y =
            Math16_Wrap(b->pentagram.center.y + b->pentagram.velocity.y);
        M_Regular(b, c, b->pentagram.angle);
    }
    return b->pentagram.center.y >= 420;
}

static void M_AttackStar(B10M *const b, const B10M_CONTEXT *const c)
{
    b->pattern_state = Math16_Wrap(b->pattern_state + 1);
    M_Lock(b, c, (uint8_t)b->iterations);
    if (b->hit.invincible && b->after_hit_frame == 0) {
        b->iterations = M_Byte(b->iterations * 2);
        b->after_hit_frame = 1;
        if (b->iterations == 32) {
            b->iterations = 1;
        }
    } else if (b->after_hit_frame > 0) {
        M_Toggle(
            b, c, b->iterations == 1 ? 16 : (uint8_t)(b->iterations >> 1),
            (uint8_t)b->iterations, 3, 3, &b->after_hit_frame);
        if (b->after_hit_frame >= 70) {
            b->after_hit_frame = 0;
            b->pattern_state = 0;
        }
    } else {
        M_Track(b, c, (uint8_t)b->iterations);
    }
    B10M_PENTAGRAM *const p = &b->pentagram;
    if (b->subphase == 0 && b->frame % b->pattern_interval == 0 && p->phase != 6
        && p->phase != 9) {
        for (int32_t i = 0; i < 5; i++) {
            b->angle = Math16_Angle(
                Math16_Wrap(p->y[i] - p->center.y),
                Math16_Wrap(p->x[i] - p->center.x));
            Pellets_AddSingle(
                c->pellets, c->pellet_shared, c->hit->display->rank, p->x[i],
                p->y[i], b->angle, 48, PELLET_REGULAR, 0, 0, 0);
        }
    }
    if (b->frame == 100 && (p->phase == 0 || p->phase == 7)) {
        p->phase++;
        b->frame = 0;
    }
    if (p->phase == 1 || p->phase == 3 || p->phase == 5) {
        if (b->frame % 4 == 0) {
            M_Spin(b, true);
            M_Regular(b, c, p->angle);
        }
        if (b->frame >= 200) {
            p->phase++;
            b->frame = 0;
        }
    }
    if (p->phase == 2 || p->phase == 4) {
        if (b->frame % 4 == 0) {
            M_Spin(b, false);
            M_Regular(b, c, p->angle);
        }
        if (b->frame >= 200) {
            p->phase++;
            b->frame = 0;
            M_Aim(b, c);
        }
    } else if (p->phase == 6) {
        if (M_SlamStar(b, c, 100)) {
            M_Back(b, c, 7);
        }
    } else if (p->phase == 8) {
        if (b->frame % 4 == 0) {
            p->radius = Math16_Wrap(p->radius + 1);
            M_Spin(b, true);
            M_Regular(b, c, p->angle);
        }
        if (b->frame >= 200) {
            p->phase = 9;
            b->frame = 0;
            M_Aim(b, c);
        }
    }
    if (p->phase == 9 && M_SlamStar(b, c, 140)) {
        M_Back(b, c, 0);
    }
}

static bool M_Death(B10M *const b, const B10M_CONTEXT *const c)
{
    const BOSS_DEFEAT_CONTEXT defeat = {
        .palette = c->hit->palette,
        .post_defeat = &b->post_defeat,
        .miko = c->miko,
        .player = M_Player(c),
        .origin_top = &c->hit->orb->top,
        .stage_cleared = c->stage_cleared,
    };
    if (!BossDefeat_Resume(&b->defeat, &defeat)) {
        return false;
    }
    Scene_Activate(c->scene, &c->assets->next_scene);
    if (b->phase == 11 && !b->hit.invincible) {
        M_Enter12(b, c);
    }
    b->defeating = false;
    b->finished = true;
    return true;
}

RESULT B10M_ValidateContext(const B10M_CONTEXT *const c)
{
    FAIL_IF(
        c == nullptr || c->assets == nullptr || c->sprites == nullptr
            || c->hit == nullptr || c->hit->display == nullptr
            || c->hit->palette == nullptr || c->hit->health == nullptr
            || c->hit->hud == nullptr || c->hit->shots_output.action == nullptr
            || c->hit->display->remaining_lives == nullptr
            || c->hit->display->high_score == nullptr
            || c->hit->display->font == nullptr
            || c->hit->display->sprites == nullptr
            || c->hit->display->player == nullptr
            || c->hit->display->gameplay == nullptr || c->hit->shots == nullptr
            || c->hit->orb == nullptr || c->hit->test_damage == nullptr
            || c->hit->effect == nullptr || c->stage_palette == nullptr
            || c->particles == nullptr || c->pellets == nullptr
            || c->pellet_shared == nullptr || c->rng == nullptr
            || c->missiles == nullptr || c->lasers == nullptr
            || c->miko == nullptr || c->scene == nullptr
            || c->stage_cleared == nullptr || c->fade_music == nullptr,
        "Missing YuugenMagan context");
    MUST(B10M_ValidateAssets(c->assets));
    FAIL_IF(
        c->hit->health->backgrounds == nullptr
            || c->hit->hud->numbers == nullptr,
        "Missing YuugenMagan HUD storage");
    MUST(Shots_ValidateSprites(c->hit->display->sprites));
    for (uint32_t digit = 0xff10; digit <= 0xff19; digit++) {
        FAIL_IF(
            GameText_Find(c->hit->display->font, digit) == nullptr,
            "Missing YuugenMagan score glyph");
    }
    MUST(BossDefeat_ValidateSprites(c->miko));
    return OK;
}

bool B10M_UpdateAndRender(B10M *const b, const B10M_CONTEXT *const c)
{
    if (b->finished) {
        return true;
    }
    if (b->defeating) {
        return M_Death(b, c);
    }
    Missiles_UpdateAndRender(
        c->missiles, M_Player(c), (uint16_t)c->hit->frame_rand,
        (MISSILES_OUTPUT) { M_MissileAction, (void *)c });
    if (b->phase == 0) {
        M_Entrance(b, c);
    } else if (b->phase == 1) {
        M_Common(b);
        M_Lateral(b, c);
        M_Hit(b, c);
        if (b->hp <= 15 || b->frame > 1100) {
            M_Next(b, 2);
        }
    } else if (
        b->phase == 2 || b->phase == 4 || b->phase == 6 || b->phase == 8
        || b->phase == 10) {
        switch (b->phase) {
        case 2:
            M_Toggle(b, c, 3, 12, 0, 2, &b->frame);
            break;
        case 4:
            M_Toggle(b, c, 12, 3, 1, 0, &b->frame);
            break;
        case 6:
            M_Toggle(b, c, 3, 12, 0, 3, &b->frame);
            break;
        case 8:
            M_Toggle(b, c, 12, 16, 2, 1, &b->frame);
            break;
        case 10:
            M_Toggle(b, c, 0, 31, b->subphase, b->iterations, &b->frame);
            break;
        }
        if (b->frame >= 70) {
            const int8_t previous = b->phase;
            M_Next(b, (int8_t)(previous + 1));
            b->subphase = 0;
            if (previous != 8) {
                b->iterations = previous == 10 ? 31 : 0;
            }
            b->pattern_state = 0;
            if (previous == 2) {
                b->pattern_interval = M_Rank(c, 8, 12, 16, 20);
            } else if (previous == 4) {
                b->pattern_interval = M_Rank(c, 12, 8, 4, 2);
                b->angle = 0;
            } else if (previous == 6) {
                b->pattern_interval = M_Rank(c, 10, 16, 20, 24);
                b->pentagram.angle = 0;
            } else {
                b->pentagram.angle = 0;
            }
            M_Snap(b, c);
        }
    } else if (b->phase == 3 || b->phase == 7) {
        M_Common(b);
        M_Missiles(b, c, b->phase == 7);
        M_Hit(b, c);
        if (b->hp <= (b->phase == 3 ? 12 : 8) || b->iterations >= 5) {
            M_Next(b, (int8_t)(b->phase + 1));
        }
    } else if (b->phase == 5) {
        M_Common(b);
        M_Spray(b, c);
        M_Hit(b, c);
        if (b->hp <= 10 || b->iterations > 4) {
            M_Next(b, 6);
        }
    } else if (b->phase == 9) {
        M_Common(b);
        M_Lasers(b, c);
        M_Hit(b, c);
        if (b->hp <= 2 || b->subphase == 6) {
            M_Next(b, 10);
            b->subphase = 1;
            b->iterations = 0;
            b->pentagram.phase = 0;
        }
    } else if (b->phase == 11 || b->phase == 13) {
        const bool attack = b->phase == 13;
        M_Common(b);
        if (b->phase == 11) {
            M_SpawnStar(b, c);
        } else {
            M_AttackStar(b, c);
        }
        M_Hit(b, c);
        if (b->hp <= 0) {
            c->fade_music(c->hit->user);
            Pellets_EraseAndResetMoving(c->pellets);
            if (attack) {
                Missiles_Reset(c->missiles);
            }
            ShootoutLasers_EraseAndReset(c->lasers, 5);
            BossDefeat_Begin(&b->defeat, BOSS_DEFEAT_REGULAR);
            b->defeating = true;
            return M_Death(b, c);
        }
        if (b->phase == 11 && b->subphase == 3 && !b->hit.invincible) {
            M_Enter12(b, c);
        }
    } else if (b->phase == 12) {
        b->hit.frame = Math16_Wrap(b->hit.frame + 1);
        M_Toggle(b, c, 30, 1, 1, 0, &b->frame);
        if (b->hit.frame % 4 == 0) {
            b->pentagram.center.y = Math16_Wrap(b->pentagram.center.y - 3);
            M_Regular(b, c, 192);
        }
        if (b->frame >= 70) {
            M_Next(b, 13);
            b->subphase = 0;
            b->iterations = 1;
            b->pattern_state = 0;
            b->pentagram.angle = 0;
            b->after_hit_frame = 0;
            M_Snap(b, c);
        }
    }
    return true;
}
