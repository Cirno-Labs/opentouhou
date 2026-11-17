#include "th01/boss_b15j.h"

#include "core/base/assert.h"

static RESULT M_LoadBos(
    VFS *const vfs, const char *const name, const ALLOCATOR *const a,
    BOS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Bos_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadBackground(
    VFS *const vfs, const ALLOCATOR *const a, PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS4.GRP", VFS_IN_MEMORY, &reader));
    const RESULT result = Pi_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadRipples(
    VFS *const vfs, const ALLOCATOR *const a, PTN *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "TAMAYEN.PTN", VFS_IN_MEMORY, &reader));
    const RESULT result = Ptn_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

RESULT B15J_ValidateAssets(const B15J_ASSETS *const a)
{
    FAIL_IF(
        a == nullptr || a->souls.pixels == nullptr
            || a->souls.coverage == nullptr || a->souls.width != 32
            || a->souls.height != 32 || a->souls.count != 3
            || a->tears.pixels == nullptr || a->tears.coverage == nullptr
            || a->tears.width != 16 || a->tears.height != 16
            || a->tears.count != 3,
        "Invalid Kikuri sprite banks");
    FAIL_IF(
        a->ripples.pixels == nullptr || a->ripples.count != 1
            || a->background.width != 640 || a->background.height != 400
            || a->background.pixels == nullptr || !a->background.has_palette,
        "Invalid Kikuri background or ripples");
    FAIL_IF(
        a->next_scene.id != 6 || a->next_scene.layouts.stages == nullptr,
        "Missing Kikuri next scene");
    return OK;
}

RESULT B15J_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const a, B15J_ASSETS *const out)
{
    *out = (B15J_ASSETS) {};
    B15J_ASSETS loaded = {};
    MUST(M_LoadBos(vfs, "TAMASII.BOS", a, &loaded.souls));
    MUST(M_LoadBos(vfs, "TAMASII2.BOS", a, &loaded.tears));
    MUST(M_LoadRipples(vfs, a, &loaded.ripples));
    MUST(M_LoadBackground(vfs, a, &loaded.background));
    MUST(Scene_Load(vfs, 6, a, &loaded.next_scene));
    MUST(B15J_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B15J_Setup(B15J *const b, const B15J_CONTEXT *const c)
{
    ASSERT(!b->entrance_active && !b->defeating);
    ASSERT(Result_Ignore(B15J_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->souls[0], c->sprites, 0, &c->assets->souls)));
    BossEntity_CopyMetadata(&b->souls[1], &b->souls[0]);
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->tears[0], c->sprites, 1, &c->assets->tears)));
    for (int32_t i = 1; i < 10; i++) {
        BossEntity_CopyMetadata(&b->tears[i], &b->tears[0]);
    }
    b->boss_palette = c->hit->palette->saved;
    for (int32_t i = 0; i < 10; i++) {
        b->tear_frame[i] = 0;
    }
    c->pellets->interlace = true;
    b->phase = 0;
    b->frame = 0;
    b->hp = 14;
    b->finished = false;
    c->hit->health->first_white = 10;
    c->hit->health->first_redwhite = 6;
    for (int32_t i = 0; i < 2; i++) {
        BossEntity_SetPosition(
            &b->souls[i], 0, 0, (BOSS_BOUNDS) { 32, 576, 64, 400 });
    }
    b->post_defeat = (PALETTE) {};
    Particles_Init(c->particles);
}

static VIDEO *M_Video(const B15J_CONTEXT *const c)
{
    return c->hit->display->video;
}

static PLAYER *M_Player(const B15J_CONTEXT *const c)
{
    return c->hit->display->player;
}

static int8_t M_Byte(const int32_t value)
{
    const uint8_t bits = (uint8_t)value;
    return bits < 128 ? (int8_t)bits : (int8_t)((int16_t)bits - 256);
}

static int16_t M_Rank(
    const B15J_CONTEXT *const c, const int16_t e, const int16_t n,
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

static void M_Effect(const B15J_CONTEXT *const c, const int16_t number)
{
    c->hit->effect(c->hit->user, number);
}

static void M_Common(B15J *const b)
{
    b->frame = Math_Wrap16(b->frame + 1);
    b->hit.frame = Math_Wrap16(b->hit.frame + 1);
}

static void M_Hit(B15J *const b, const B15J_CONTEXT *const c)
{
    const ORB *const orb = c->hit->orb;
    const bool colliding =
        orb->left > 256 && orb->left < 368 && orb->top < 132 && orb->top < 196;
    BOSS_HIT_CONTEXT hit = *c->hit;
    hit.hp = &b->hp;
    hit.boss_palette = &b->boss_palette;
    const uint8_t flash[] = { 6, 11, 8, 2 };
    BossHit_UpdateAndRender(
        &b->hit, &hit, flash, 4, 7000, colliding, (BOSS_SHOT_HITBOX) {});
}

static void M_CopyLine(VIDEO *const video, const int16_t y, const uint16_t mask)
{
    if (y < 0 || y >= 400) {
        return;
    }
    for (int32_t x = 0; x < 640; x++) {
        const int32_t shift = (x & 8) + 7 - (x & 7);
        video->pages[0][y * 640 + x] =
            (mask & (1u << shift)) != 0 ? video->pages[1][y * 640 + x] : 0;
    }
    Video_SelectDrawingPage(video, 0);
}

static void M_SymmetricLine(
    VIDEO *const video, const int16_t y, const int16_t offset,
    const uint16_t mask)
{
    M_CopyLine(video, Math_Wrap16(y - offset), mask);
    M_CopyLine(video, Math_Wrap16(399 - y + offset), mask);
}

static void M_Defaults(const B15J_CONTEXT *const c)
{
    Sprite_PtnPut(M_Video(c), c->miko, 0, -1, M_Player(c)->left, 368);
    Orb_RenderAction(
        M_Video(c), c->hit->display->sprites,
        (ORB_ACTION) { ORB_ACTION_PUT, c->hit->orb->left, c->hit->orb->top,
                       0 });
}

static int16_t M_InvertRings(const B15J_CONTEXT *const c, const int16_t base)
{
    int16_t outside = 0;
    for (int32_t i = 32; i < 160; i++) {
        const int16_t radius = Math_Wrap16(base * (i / 32));
        const int16_t left = Math_PolarY(304, radius, i * 8);
        const int16_t top = Math_PolarX(180, radius, i * 8);
        outside += !Video_Invert32(M_Video(c), left, top, 2);
    }
    return outside;
}

static bool M_Entrance(
    B15J *const b, const B15J_CONTEXT *const c, FRAME_VSYNC *const vsync)
{
    if (!b->entrance_active) {
        b->entrance_active = true;
        b->entrance_stage = 0;
        b->entrance_waiting = false;
        b->frame = 0;
        b->hit.frame = 0;
        b->hit.invincible = false;
        b->boss_palette = c->hit->palette->saved;
        const int32_t colors[] = { 2, 6, 8, 11 };
        for (int32_t i = 0; i < 4; i++) {
            Palette_SetColor(
                c->hit->palette, M_Video(c), colors[i], 15, 15, 15);
        }
    }
    while (true) {
        if (b->entrance_waiting) {
            if (!Frame_DelayReady(vsync, 1)) {
                return false;
            }
            b->entrance_waiting = false;
        }
        b->frame = Math_Wrap16(b->frame + 1);
        if (b->entrance_stage == 0) {
            if (b->frame % 3 == 0) {
                const int16_t y = b->hit.frame;
                if (y < 200) {
                    M_SymmetricLine(M_Video(c), y, 0, 0x2222);
                }
                if (b->frame * 2 > 8 && y < 208) {
                    M_SymmetricLine(M_Video(c), y, 8, 0xaaaa);
                }
                if (b->frame * 2 > 16 && y < 216) {
                    M_SymmetricLine(M_Video(c), y, 16, 0xeeee);
                }
                if (b->frame * 2 > 24 && y <= 224) {
                    M_SymmetricLine(M_Video(c), y, 24, 0xffff);
                }
                b->hit.frame = Math_Wrap16(y + 1);
                if (b->hit.frame > 224) {
                    b->hit.frame = 0;
                    b->frame = 0;
                    b->entrance_stage = 1;
                    continue;
                }
            }
            M_Defaults(c);
            b->entrance_waiting = true;
        } else {
            bool finish = false;
            if (b->frame == 70) {
                b->ring_radius = 0;
            } else if (b->frame == 71) {
                M_InvertRings(c, b->ring_radius);
            } else if (b->frame > 71 && b->frame % 4 == 0) {
                M_InvertRings(c, b->ring_radius);
                b->ring_radius = Math_Wrap16(b->ring_radius + 16);
                finish = M_InvertRings(c, b->ring_radius) >= 128;
            }
            if (finish) {
                b->phase = 2;
                b->patterns_done = 0;
                b->frame = 0;
                Palette_Set(c->hit->palette, M_Video(c), &b->boss_palette);
                b->hp_bar_filled = false;
                *c->stage_palette = c->hit->palette->saved;
                b->entrance_active = false;
                return true;
            }
            if (b->frame > 71 && b->frame % 30 != 0) {
                const int32_t colors[] = { 2, 6, 8, 11 };
                for (int32_t component = 0; component < 3; component++) {
                    for (int32_t i = 0; i < 4; i++) {
                        const int32_t color = colors[i];
                        if (b->boss_palette.colors[color][component]
                            < c->hit->palette->saved.colors[color][component]) {
                            c->hit->palette->saved.colors[color][component]--;
                        }
                    }
                }
                Palette_Set(
                    c->hit->palette, M_Video(c), &c->hit->palette->saved);
            }
            b->entrance_waiting = b->frame % 2 != 0;
        }
    }
}

static void M_SoulMove(
    B15J *const b, const B15J_CONTEXT *const c, const int32_t i,
    const int16_t dx, const int16_t dy)
{
    BOSS_ENTITY *const soul = &b->souls[i];
    BossEntity_LockedMovePut(soul, c->sprites, M_Video(c), dx, dy, 1, true);
    if (b->frame % 12 == 0) {
        soul->image = soul->image >= 2 ? 0 : Math_Wrap16(soul->image + 1);
    }
}

static void M_AddTear(B15J *const b, const int16_t x, const int16_t y)
{
    for (int32_t i = 0; i < 10; i++) {
        if (b->tear_frame[i] == 0) {
            BossEntity_SetPosition(
                &b->tears[i], x, y, (BOSS_BOUNDS) { 32, 576, 64, 400 });
            b->tears[i].image = 0;
            b->tear_frame[i] = 1;
            return;
        }
    }
}

static void M_RippleHit(
    const B15J_CONTEXT *const c, const int16_t left, const int16_t extra)
{
    PLAYER *const p = M_Player(c);
    if (!p->invincible && p->left >= Math_Wrap16(left - 16)
        && p->left <= Math_Wrap16(left + extra)) {
        p->is_hit = true;
    }
}

static void M_RipplePart(
    const B15J_CONTEXT *const c, const int16_t left, const int16_t top,
    const int16_t frame)
{
    const int16_t dy = frame == 0 ? 0
        : frame <= 5              ? 8 * (frame - 1)
        : frame <= 9              ? 8 * (9 - frame)
                                  : 0;
    const int16_t quarter = frame == 0 ? 0
        : frame <= 4                   ? 1
        : frame <= 9                   ? 0
                                       : 3;
    const int16_t y = Math_Wrap16(top - dy);
    Sprite_Restore16(M_Video(c), left, y, 16, 16);
    Sprite_PtnPut(M_Video(c), &c->assets->ripples, 0, quarter, left, y);
}

static void M_Ripple(
    const B15J_CONTEXT *const c, const int16_t left, const int16_t top,
    int8_t *const frame)
{
    *frame = M_Byte(*frame + 1);
    const int16_t stage = *frame / 5;
    if (*frame >= 75) {
        *frame = 0;
        return;
    }
    if (*frame < 10) {
        M_RipplePart(c, left, top, *frame < 5 ? 0 : 1);
        M_RippleHit(c, left, 0);
        if (*frame < 5) {
            M_Effect(c, 7);
        }
        return;
    }
    if (*frame < 20) {
        M_RippleHit(c, Math_Wrap16(left - 16), 32);
    } else if (*frame < 60) {
        M_RippleHit(c, Math_Wrap16(left - 32), 64);
    }
    if (*frame < 55) {
        M_RipplePart(c, left, top, stage);
    }
    if (*frame < 65) {
        M_RipplePart(c, Math_Wrap16(left - 16), top, stage - 2);
        M_RipplePart(c, Math_Wrap16(left + 16), top, stage - 2);
    }
    if (*frame >= 20) {
        M_RipplePart(c, Math_Wrap16(left - 32), top, stage - 4);
        M_RipplePart(c, Math_Wrap16(left + 32), top, stage - 4);
    }
}

static void M_Tears(B15J *const b, const B15J_CONTEXT *const c)
{
    for (int32_t i = 0; i < 10; i++) {
        if (b->tear_frame[i] != 0) {
            if (b->tears[i].top <= 384) {
                BossEntity_LockedMovePut(
                    &b->tears[i], c->sprites, M_Video(c), 0, 8, 1, true);
            } else {
                M_Ripple(c, b->tears[i].left, 384, &b->tear_frame[i]);
            }
        }
    }
}

static void M_MoveSouls(
    B15J *const b, const B15J_CONTEXT *const c, const int16_t vx,
    const int16_t vy)
{
    const int16_t x = Math_PolarX(0, vx, b->frame);
    const int16_t y = Math_PolarX(0, vy, b->frame);
    M_Tears(b, c);
    M_SoulMove(b, c, 0, x, y);
    M_SoulMove(b, c, 1, Math_Wrap16(-x), y);
}

static void M_GroupSoul(
    const B15J *const b, const B15J_CONTEXT *const c, const int32_t i,
    const PELLET_GROUP group, const int16_t speed, const int16_t dy)
{
    Pellets_AddGroup(
        c->pellets, c->pellet_shared, M_Player(c), c->rng,
        c->hit->display->rank, Math_Wrap16(b->souls[i].left + 12),
        Math_Wrap16(b->souls[i].top + dy), group, speed);
}

static void M_Single(
    const B15J_CONTEXT *const c, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed, const PELLET_MOTION motion,
    const int16_t motion_speed, const int16_t cx, const int16_t cy)
{
    Pellets_AddSingle(
        c->pellets, c->pellet_shared, c->hit->display->rank, x, y, angle, speed,
        motion, motion_speed, cx, cy);
}

static void M_SymmetricSpiral(B15J *const b, const B15J_CONTEXT *const c)
{
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        b->spiral_angle = 0;
        b->spiral_drift = 0;
        b->pattern_state = M_Rank(c, 4, 3, 2, 1);
        b->spiral_distance = M_Rank(c, 90, 90, 90, 50);
    }
    if (b->frame % b->pattern_state == 0) {
        for (int32_t i = 0; i < 2; i++) {
            const uint8_t angle = (uint8_t)(b->spiral_angle + i * 128);
            M_Single(
                c, Math_PolarX(320, b->spiral_distance, angle),
                Math_PolarY(180, b->spiral_distance, angle),
                (uint8_t)(angle + b->spiral_drift), 50, PELLET_REGULAR, 0, 0,
                0);
        }
        if (b->frame >= 900) {
            b->frame = 0;
        }
        b->spiral_angle = (uint8_t)(b->spiral_angle - 8);
        b->spiral_drift++;
    }
}

static void M_SpinRings(const B15J *const b, const B15J_CONTEXT *const c)
{
    if (b->frame % M_Rank(c, 80, 60, 55, 50) != 0) {
        return;
    }
    for (int32_t i = 0; i < 8; i++) {
        const uint8_t angle =
            Math_Angle(164, Math_Wrap16(M_Player(c)->left - 300));
        M_Single(
            c, Math_PolarX(316, 16, i * 32), Math_PolarY(224, 16, i * 32),
            angle, 48, PELLET_SPIN, 32, 312, 224);
    }
}

static void M_ActivateSouls(B15J *const b, const B15J_CONTEXT *const c)
{
    if (b->frame == 200) {
        for (int32_t i = 0; i < 2; i++) {
            BossEntity_SetPosition(
                &b->souls[i], 304, 208, (BOSS_BOUNDS) { 32, 576, 64, 400 });
        }
    } else if (b->frame > 200 && b->frame % 2 == 0) {
        M_SoulMove(b, c, 0, -4, -3);
        M_SoulMove(b, c, 1, 4, -3);
        if (b->souls[0].top < 96) {
            b->subphase = 1;
        }
    }
}

static void M_SoulSpreads(B15J *const b, const B15J_CONTEXT *const c)
{
    M_SoulMove(b, c, 0, 0, 0);
    M_SoulMove(b, c, 1, 0, 0);
    if (b->frame % 80 == 0) {
        const PELLET_GROUP group = (PELLET_GROUP)M_Rank(
            c, PELLET_GROUP_TWO_WIDE_AIMED, PELLET_GROUP_THREE_WIDE_AIMED,
            PELLET_GROUP_FIVE_WIDE_AIMED, PELLET_GROUP_FIVE_WIDE);
        M_GroupSoul(b, c, 0, group, 55, 8);
        M_GroupSoul(b, c, 1, group, 55, 8);
    }
    b->subphase = b->frame > 600 ? 2 : 1;
}

static void M_Laser(
    const B15J_CONTEXT *const c, const int32_t i, const int16_t x,
    const int16_t target, const int16_t speed, const int16_t width)
{
    ShootoutLasers_Spawn(
        &c->lasers->slots[i], x, 147, target, 400, speed, 10, 20, width);
}

static void M_CrossLasers(const B15J *const b, const B15J_CONTEXT *const c)
{
    if (b->frame > 200 && b->frame % 200 == 0) {
        M_Laser(c, 0, 307, Math_Wrap16(M_Player(c)->left + 60), 50, 8);
        M_Laser(c, 1, 336, Math_Wrap16(M_Player(c)->left - 60), 50, 8);
        M_Effect(c, 6);
    }
}

static void M_DropTears(B15J *const b, const B15J_CONTEXT *const c)
{
    if (b->frame < 10) {
        b->pattern_state = M_Rank(c, 200, 160, 140, 120);
    }
    if (b->frame % b->pattern_state == 0) {
        for (int32_t i = 0; i < 2; i++) {
            M_AddTear(
                b, Math_Wrap16(b->souls[i].left + 12),
                Math_Wrap16(b->souls[i].top + 8));
        }
    }
    M_MoveSouls(b, c, 6, 2);
}

static void M_FourSpiral(B15J *const b, const B15J_CONTEXT *const c)
{
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        b->four_angle = 32;
        b->pattern_state = M_Rank(c, 14, 8, 6, 4);
    }
    if (b->frame % b->pattern_state == 0) {
        for (int32_t i = 0; i < 4; i += 2) {
            const int16_t x = Math_PolarX(320, 90, b->frame * 4 + 128 * i);
            const int16_t y = Math_PolarY(180, 90, b->frame * 4 + 128 * i);
            for (int32_t j = 0; j < 4; j++) {
                M_Single(
                    c, x, y, (uint8_t)(b->four_angle + j * 64), 36,
                    PELLET_REGULAR, 0, 0, 0);
            }
        }
        if (b->frame <= 600) {
            b->four_angle = (uint8_t)(b->four_angle - 5);
        } else {
            b->frame = 0;
            b->subphase = 1;
        }
    }
}

static void M_LeftLasers(B15J *const b, const B15J_CONTEXT *const c)
{
    if (b->frame <= 200) {
        return;
    }
    if (b->frame == 250) {
        b->pattern_state = M_Rank(c, 52, 56, 60, 64);
    }
    if (b->frame >= 250 && b->frame <= 400 && (b->frame - 250) % 30 == 0) {
        M_Effect(c, 6);
        const int16_t offset = (int16_t)(Math_Random(c->rng) % 120 - 60);
        M_Laser(
            c, (b->frame - 250) / 30, 307,
            Math_Wrap16(M_Player(c)->left + offset), b->pattern_state, 4);
    }
    if (b->frame > 500) {
        b->frame = 0;
        b->subphase = 2;
    }
}

static void M_Rain(B15J *const b, const B15J_CONTEXT *const c)
{
    if (b->frame > 100 && b->frame % b->pattern_state == 0) {
        for (int32_t i = 0; i < 2; i++) {
            const int16_t x = Math_Wrap16(b->souls[i].left + 12);
            const int16_t y = Math_Wrap16(b->souls[i].top + 12);
            for (int32_t j = 0; j < 6; j++) {
                const int16_t speed = (int16_t)((j < 3 ? 3 - j : j - 2) * 16);
                M_Single(
                    c, x, y, j < 3 ? 0 : 128, speed, PELLET_GRAVITY, 1, 0, 0);
            }
            M_Effect(c, 7);
        }
    }
    if (b->frame > 250) {
        b->frame = 0;
        b->subphase = 3;
    }
}

static void M_VerticalLasers(B15J *const b, const B15J_CONTEXT *const c)
{
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        b->pattern_state = M_Rank(c, 60, 64, 68, 72);
        b->random_range = M_Rank(c, 0, 10, 16, 20);
    }
    if (b->frame % 10 == 0) {
        const int32_t i = (b->frame - 100) / 10;
        const int16_t offset =
            (int16_t)(Math_Random(c->rng) % (b->random_range * 2 + 1)
                      - b->random_range);
        const int16_t x = Math_Wrap16(i * 64 + offset);
        ShootoutLasers_Spawn(
            &c->lasers->slots[i], x, 64, x, 400, b->pattern_state, 10, 20, 8);
        M_Effect(c, 6);
    }
    if (b->frame >= 190) {
        b->frame = 0;
        b->subphase = 0;
    }
}

static bool M_Death(
    B15J *const b, const B15J_CONTEXT *const c, FRAME_VSYNC *const vsync)
{
    const BOSS_DEFEAT_CONTEXT defeat = {
        .video = M_Video(c),
        .palette = c->hit->palette,
        .post_defeat = &b->post_defeat,
        .miko = c->miko,
        .player = M_Player(c),
        .origin_top = &c->hit->orb->top,
        .stage_cleared = c->stage_cleared,
    };
    if (!BossDefeat_Resume(&b->defeat, &defeat, vsync)) {
        return false;
    }
    Scene_Activate(c->scene, &c->assets->next_scene);
    b->defeating = false;
    b->finished = true;
    return true;
}

RESULT B15J_ValidateContext(const B15J_CONTEXT *const c)
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
            || c->hit->display->video == nullptr
            || c->hit->display->player == nullptr
            || c->hit->display->gameplay == nullptr || c->hit->shots == nullptr
            || c->hit->orb == nullptr || c->hit->test_damage == nullptr
            || c->hit->effect == nullptr || c->stage_palette == nullptr
            || c->particles == nullptr || c->pellets == nullptr
            || c->pellet_shared == nullptr || c->rng == nullptr
            || c->lasers == nullptr || c->miko == nullptr || c->scene == nullptr
            || c->stage_cleared == nullptr || c->fade_music == nullptr,
        "Missing Kikuri context");
    MUST(B15J_ValidateAssets(c->assets));
    FAIL_IF(
        c->hit->health->backgrounds == nullptr
            || c->hit->hud->numbers == nullptr,
        "Missing Kikuri HUD storage");
    MUST(Shots_ValidateSprites(c->hit->display->sprites));
    for (uint32_t digit = 0xff10; digit <= 0xff19; digit++) {
        FAIL_IF(
            GameText_Find(c->hit->display->font, digit) == nullptr,
            "Missing Kikuri score glyph");
    }
    MUST(BossDefeat_ValidateSprites(c->miko));
    return OK;
}

bool B15J_UpdateAndRender(
    B15J *const b, const B15J_CONTEXT *const c, FRAME_VSYNC *const vsync)
{
    if (b->finished) {
        return true;
    }
    if (b->defeating) {
        return M_Death(b, c, vsync);
    }
    if (b->phase == 0) {
        return M_Entrance(b, c, vsync);
    }
    if (b->phase == 2) {
        if (!b->hp_bar_filled) {
            b->hp_bar_filled =
                HudHp_Increment(c->hit->health, M_Video(c), b->hp, b->frame);
        }
        M_Common(b);
        M_SymmetricSpiral(b, c);
        M_Hit(b, c);
        if (b->frame == 0) {
            b->patterns_done = Math_Wrap16(b->patterns_done + 1);
        }
        if (!b->hit.invincible && (b->hp <= 10 || b->patterns_done >= 6)) {
            b->phase = 3;
            b->frame = 0;
            b->hit.frame = 0;
            b->patterns_done = 0;
        }
    } else if (b->phase == 3) {
        b->frame = Math_Wrap16(b->frame + 1);
        if (b->frame % 20 == 19) {
            c->hit->palette->saved.colors[5][0] =
                M_Byte(c->hit->palette->saved.colors[5][0] + 1);
            c->hit->palette->saved.colors[5][2] =
                M_Byte(c->hit->palette->saved.colors[5][2] - 1);
            c->stage_palette->colors[5][0] =
                M_Byte(c->stage_palette->colors[5][0] + 1);
            c->stage_palette->colors[5][2] =
                M_Byte(c->stage_palette->colors[5][2] - 1);
            Palette_Set(c->hit->palette, M_Video(c), c->stage_palette);
        }
        if (b->frame >= 100) {
            b->phase = 4;
            b->frame = 0;
            b->subphase = 0;
            Palette_Set(c->hit->palette, M_Video(c), c->stage_palette);
            b->boss_palette = c->hit->palette->saved;
        }
    } else if (b->phase == 4) {
        M_Common(b);
        M_SpinRings(b, c);
        if (b->subphase == 0) {
            M_ActivateSouls(b, c);
        } else {
            M_SoulSpreads(b, c);
        }
        M_Hit(b, c);
        if (!b->hit.invincible && b->subphase == 2) {
            b->phase = 5;
            b->subphase = 0;
            b->frame = 0;
        }
    } else if (b->phase == 5) {
        M_Common(b);
        M_DropTears(b, c);
        M_CrossLasers(b, c);
        M_Hit(b, c);
        if (!b->hit.invincible && (b->hp <= 6 || b->frame > 1600)) {
            b->phase = 6;
            b->frame = 0;
            b->hit.frame = 0;
            b->patterns_done = 0;
        }
    } else if (b->phase == 6) {
        M_Common(b);
        if (b->frame % 200 == 0) {
            M_GroupSoul(b, c, 0, PELLET_GROUP_SINGLE_AIMED, 40, 12);
            M_GroupSoul(b, c, 1, PELLET_GROUP_SINGLE_AIMED, 40, 12);
        }
        M_MoveSouls(b, c, 4, 2);
        switch (b->subphase) {
        case 0:
            M_FourSpiral(b, c);
            break;
        case 1:
            M_LeftLasers(b, c);
            break;
        case 2:
            M_Rain(b, c);
            break;
        case 3:
            M_VerticalLasers(b, c);
            break;
        default:
            break;
        }
        if (b->frame == 0) {
            b->patterns_done = Math_Wrap16(b->patterns_done + 1);
        }
        if (b->patterns_done > 29) {
            b->hp = 1;
        }
        M_Hit(b, c);
        if (b->hp <= 0) {
            c->fade_music(c->hit->user);
            Pellets_EraseAndResetMoving(c->pellets, M_Video(c));
            ShootoutLasers_EraseAndReset(c->lasers, M_Video(c), 4);
            BossDefeat_Begin(&b->defeat, BOSS_DEFEAT_REGULAR, vsync);
            b->defeating = true;
            return M_Death(b, c, vsync);
        }
    }
    return true;
}
