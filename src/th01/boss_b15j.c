#include "th01/boss_b15j.h"
#include "th01/session.h"
#include "th01/sound.h"
#include "th01/rng.h"

#include "th01/boss.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/assert.h"

#include <string.h>

B15J g_B15J;

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
    MUST(Boss_LoadBos(vfs, "TAMASII.BOS", a, &loaded.souls));
    MUST(Boss_LoadBos(vfs, "TAMASII2.BOS", a, &loaded.tears));
    MUST(M_LoadRipples(vfs, a, &loaded.ripples));
    MUST(Boss_LoadBackground(vfs, "BOSS4.GRP", a, &loaded.background));
    MUST(Scene_Load(vfs, 6, a, &loaded.next_scene));
    MUST(B15J_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B15J_Init(void)
{
    memset(&g_B15J, 0, sizeof(g_B15J));
}

void B15J_Setup(const B15J_CONTEXT *const c)
{
    ASSERT(!g_B15J.entrance_active && !g_B15J.defeating);
    ASSERT(Result_Ignore(B15J_ValidateAssets(c->assets)));
    ASSERT(
        Result_Ignore(BossEntity_Bind(&g_B15J.souls[0], 0, &c->assets->souls)));
    BossEntity_CopyMetadata(&g_B15J.souls[1], &g_B15J.souls[0]);
    ASSERT(
        Result_Ignore(BossEntity_Bind(&g_B15J.tears[0], 1, &c->assets->tears)));
    for (int32_t i = 1; i < 10; i++) {
        BossEntity_CopyMetadata(&g_B15J.tears[i], &g_B15J.tears[0]);
    }
    g_B15J.boss_palette = g_Palette.saved;
    for (int32_t i = 0; i < 10; i++) {
        g_B15J.tear_frame[i] = 0;
    }
    g_Pellets.interlace = true;
    g_B15J.phase = B15J_PHASE_ENTRANCE;
    g_B15J.frame = 0;
    g_B15J.hp = 14;
    g_B15J.finished = false;
    g_HudHp.first_white = 10;
    g_HudHp.first_redwhite = 6;
    for (int32_t i = 0; i < 2; i++) {
        BossEntity_SetPosition(
            &g_B15J.souls[i], 0, 0, (BOSS_BOUNDS) { 32, 576, 64, 400 });
    }
    g_B15J.post_defeat = (PALETTE) {};
    Particles_Init();
}

static int8_t M_Byte(const int32_t value)
{
    const uint8_t bits = (uint8_t)value;
    return bits < 128 ? (int8_t)bits : (int8_t)((int16_t)bits - 256);
}

static void M_Common(void)
{
    g_B15J.frame = Math16_Wrap(g_B15J.frame + 1);
    g_B15J.hit.frame = Math16_Wrap(g_B15J.hit.frame + 1);
}

static void M_Hit(const B15J_CONTEXT *const c)
{
    const ORB *const orb = &g_Orb;
    const bool colliding =
        orb->left > 256 && orb->left < 368 && orb->top < 132 && orb->top < 196;
    const uint8_t flash[] = { 6, 11, 8, 2 };
    BossHit_UpdateAndRender(
        &g_B15J.hit, c->shots_output, &g_B15J.hp, &g_B15J.boss_palette, flash,
        4, 7000, colliding, (BOSS_SHOT_HITBOX) {});
}

static void M_CopyLine(const int16_t y, const uint16_t mask)
{
    if (y < 0 || y >= 400) {
        return;
    }
    for (int32_t x = 0; x < 640; x++) {
        const int32_t shift = (x & 8) + 7 - (x & 7);
        g_Video.pages[0][y * 640 + x] =
            (mask & (1u << shift)) != 0 ? g_Video.pages[1][y * 640 + x] : 0;
    }
    Video_SelectDrawingPage(0);
}

static void M_SymmetricLine(
    const int16_t y, const int16_t offset, const uint16_t mask)
{
    M_CopyLine(Math16_Wrap(y - offset), mask);
    M_CopyLine(Math16_Wrap(399 - y + offset), mask);
}

static void M_Defaults(const B15J_CONTEXT *const c)
{
    Sprite_PtnPut(c->miko, 0, -1, Boss_Player()->left, 368);
    Orb_RenderAction(
        g_Hud.sprites,
        (ORB_ACTION) { ORB_ACTION_PUT, g_Orb.left, g_Orb.top, 0 });
}

static int16_t M_InvertRings(const B15J_CONTEXT *const c, const int16_t base)
{
    int16_t outside = 0;
    for (int32_t i = 32; i < 160; i++) {
        const int16_t radius = Math16_Wrap(base * (i / 32));
        const int16_t left = Math16_PolarY(304, radius, i * 8);
        const int16_t top = Math16_PolarX(180, radius, i * 8);
        outside += !Video_Invert32(left, top, 2);
    }
    return outside;
}

static bool M_Entrance(const B15J_CONTEXT *const c)
{
    if (!g_B15J.entrance_active) {
        g_B15J.entrance_active = true;
        g_B15J.entrance_stage = 0;
        g_B15J.entrance_waiting = false;
        g_B15J.frame = 0;
        g_B15J.hit.frame = 0;
        g_B15J.hit.invincible = false;
        g_B15J.boss_palette = g_Palette.saved;
        const int32_t colors[] = { 2, 6, 8, 11 };
        for (int32_t i = 0; i < 4; i++) {
            Palette_SetColor(&g_Palette, colors[i], 15, 15, 15);
        }
    }
    while (true) {
        if (g_B15J.entrance_waiting) {
            if (!Frame_DelayReady(1)) {
                return false;
            }
            g_B15J.entrance_waiting = false;
        }
        g_B15J.frame = Math16_Wrap(g_B15J.frame + 1);
        if (g_B15J.entrance_stage == 0) {
            if (g_B15J.frame % 3 == 0) {
                const int16_t y = g_B15J.hit.frame;
                if (y < 200) {
                    M_SymmetricLine(y, 0, 0x2222);
                }
                if (g_B15J.frame * 2 > 8 && y < 208) {
                    M_SymmetricLine(y, 8, 0xaaaa);
                }
                if (g_B15J.frame * 2 > 16 && y < 216) {
                    M_SymmetricLine(y, 16, 0xeeee);
                }
                if (g_B15J.frame * 2 > 24 && y <= 224) {
                    M_SymmetricLine(y, 24, 0xffff);
                }
                g_B15J.hit.frame = Math16_Wrap(y + 1);
                if (g_B15J.hit.frame > 224) {
                    g_B15J.hit.frame = 0;
                    g_B15J.frame = 0;
                    g_B15J.entrance_stage = 1;
                    continue;
                }
            }
            M_Defaults(c);
            g_B15J.entrance_waiting = true;
        } else {
            bool finish = false;
            if (g_B15J.frame == 70) {
                g_B15J.ring_radius = 0;
            } else if (g_B15J.frame == 71) {
                M_InvertRings(c, g_B15J.ring_radius);
            } else if (g_B15J.frame > 71 && g_B15J.frame % 4 == 0) {
                M_InvertRings(c, g_B15J.ring_radius);
                g_B15J.ring_radius = Math16_Wrap(g_B15J.ring_radius + 16);
                finish = M_InvertRings(c, g_B15J.ring_radius) >= 128;
            }
            if (finish) {
                g_B15J.phase = B15J_PHASE_SPIRAL;
                g_B15J.patterns_done = 0;
                g_B15J.frame = 0;
                Palette_Set(&g_Palette, &g_B15J.boss_palette);
                g_B15J.hp_bar_filled = false;
                g_StagePalette = g_Palette.saved;
                g_B15J.entrance_active = false;
                return true;
            }
            if (g_B15J.frame > 71 && g_B15J.frame % 30 != 0) {
                const int32_t colors[] = { 2, 6, 8, 11 };
                for (int32_t component = 0; component < 3; component++) {
                    for (int32_t i = 0; i < 4; i++) {
                        const int32_t color = colors[i];
                        if (g_B15J.boss_palette.colors[color][component]
                            < g_Palette.saved.colors[color][component]) {
                            g_Palette.saved.colors[color][component]--;
                        }
                    }
                }
                Palette_Set(&g_Palette, &g_Palette.saved);
            }
            g_B15J.entrance_waiting = g_B15J.frame % 2 != 0;
        }
    }
}

static void M_SoulMove(
    const B15J_CONTEXT *const c, const int32_t i, const int16_t dx,
    const int16_t dy)
{
    BOSS_ENTITY *const soul = &g_B15J.souls[i];
    BossEntity_LockedMovePut(soul, dx, dy, 1, true);
    if (g_B15J.frame % 12 == 0) {
        soul->image = soul->image >= 2 ? 0 : Math16_Wrap(soul->image + 1);
    }
}

static void M_AddTear(const int16_t x, const int16_t y)
{
    for (int32_t i = 0; i < 10; i++) {
        if (g_B15J.tear_frame[i] == 0) {
            BossEntity_SetPosition(
                &g_B15J.tears[i], x, y, (BOSS_BOUNDS) { 32, 576, 64, 400 });
            g_B15J.tears[i].image = 0;
            g_B15J.tear_frame[i] = 1;
            return;
        }
    }
}

static void M_RippleHit(
    const B15J_CONTEXT *const c, const int16_t left, const int16_t extra)
{
    PLAYER *const p = Boss_Player();
    if (!p->invincible && p->left >= Math16_Wrap(left - 16)
        && p->left <= Math16_Wrap(left + extra)) {
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
    const int16_t y = Math16_Wrap(top - dy);
    Sprite_Restore16(left, y, 16, 16);
    Sprite_PtnPut(&c->assets->ripples, 0, quarter, left, y);
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
            Sound_Effect(7);
        }
        return;
    }
    if (*frame < 20) {
        M_RippleHit(c, Math16_Wrap(left - 16), 32);
    } else if (*frame < 60) {
        M_RippleHit(c, Math16_Wrap(left - 32), 64);
    }
    if (*frame < 55) {
        M_RipplePart(c, left, top, stage);
    }
    if (*frame < 65) {
        M_RipplePart(c, Math16_Wrap(left - 16), top, stage - 2);
        M_RipplePart(c, Math16_Wrap(left + 16), top, stage - 2);
    }
    if (*frame >= 20) {
        M_RipplePart(c, Math16_Wrap(left - 32), top, stage - 4);
        M_RipplePart(c, Math16_Wrap(left + 32), top, stage - 4);
    }
}

static void M_Tears(const B15J_CONTEXT *const c)
{
    for (int32_t i = 0; i < 10; i++) {
        if (g_B15J.tear_frame[i] != 0) {
            if (g_B15J.tears[i].top <= 384) {
                BossEntity_LockedMovePut(&g_B15J.tears[i], 0, 8, 1, true);
            } else {
                M_Ripple(c, g_B15J.tears[i].left, 384, &g_B15J.tear_frame[i]);
            }
        }
    }
}

static void M_MoveSouls(
    const B15J_CONTEXT *const c, const int16_t vx, const int16_t vy)
{
    const int16_t x = Math16_PolarX(0, vx, g_B15J.frame);
    const int16_t y = Math16_PolarX(0, vy, g_B15J.frame);
    M_Tears(c);
    M_SoulMove(c, 0, x, y);
    M_SoulMove(c, 1, Math16_Wrap(-x), y);
}

static void M_GroupSoul(
    const B15J_CONTEXT *const c, const int32_t i, const PELLET_GROUP group,
    const int16_t speed, const int16_t dy)
{
    Pellets_AddGroup(
        g_Session.settings.difficulty, Math16_Wrap(g_B15J.souls[i].left + 12),
        Math16_Wrap(g_B15J.souls[i].top + dy), group, speed);
}

static void M_Single(
    const B15J_CONTEXT *const c, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed, const PELLET_MOTION motion,
    const int16_t motion_speed, const int16_t cx, const int16_t cy)
{
    Pellets_AddSingle(
        g_Session.settings.difficulty, x, y, angle, speed, motion, motion_speed,
        cx, cy);
}

static void M_SymmetricSpiral(const B15J_CONTEXT *const c)
{
    if (g_B15J.frame < 100) {
        return;
    }
    if (g_B15J.frame == 100) {
        g_B15J.spiral_angle = 0;
        g_B15J.spiral_drift = 0;
        g_B15J.pattern_state = Boss_Rank(4, 3, 2, 1);
        g_B15J.spiral_distance = Boss_Rank(90, 90, 90, 50);
    }
    if (g_B15J.frame % g_B15J.pattern_state == 0) {
        for (int32_t i = 0; i < 2; i++) {
            const uint8_t angle = (uint8_t)(g_B15J.spiral_angle + i * 128);
            M_Single(
                c, Math16_PolarX(320, g_B15J.spiral_distance, angle),
                Math16_PolarY(180, g_B15J.spiral_distance, angle),
                (uint8_t)(angle + g_B15J.spiral_drift), 50, PELLET_REGULAR, 0,
                0, 0);
        }
        if (g_B15J.frame >= 900) {
            g_B15J.frame = 0;
        }
        g_B15J.spiral_angle = (uint8_t)(g_B15J.spiral_angle - 8);
        g_B15J.spiral_drift++;
    }
}

static void M_SpinRings(const B15J_CONTEXT *const c)
{
    if (g_B15J.frame % Boss_Rank(80, 60, 55, 50) != 0) {
        return;
    }
    for (int32_t i = 0; i < 8; i++) {
        const uint8_t angle =
            Math16_Angle(164, Math16_Wrap(Boss_Player()->left - 300));
        M_Single(
            c, Math16_PolarX(316, 16, i * 32), Math16_PolarY(224, 16, i * 32),
            angle, 48, PELLET_SPIN, 32, 312, 224);
    }
}

static void M_ActivateSouls(const B15J_CONTEXT *const c)
{
    if (g_B15J.frame == 200) {
        for (int32_t i = 0; i < 2; i++) {
            BossEntity_SetPosition(
                &g_B15J.souls[i], 304, 208, (BOSS_BOUNDS) { 32, 576, 64, 400 });
        }
    } else if (g_B15J.frame > 200 && g_B15J.frame % 2 == 0) {
        M_SoulMove(c, 0, -4, -3);
        M_SoulMove(c, 1, 4, -3);
        if (g_B15J.souls[0].top < 96) {
            g_B15J.subphase = 1;
        }
    }
}

static void M_SoulSpreads(const B15J_CONTEXT *const c)
{
    M_SoulMove(c, 0, 0, 0);
    M_SoulMove(c, 1, 0, 0);
    if (g_B15J.frame % 80 == 0) {
        const PELLET_GROUP group = (PELLET_GROUP)Boss_Rank(
            PELLET_GROUP_TWO_WIDE_AIMED, PELLET_GROUP_THREE_WIDE_AIMED,
            PELLET_GROUP_FIVE_WIDE_AIMED, PELLET_GROUP_FIVE_WIDE);
        M_GroupSoul(c, 0, group, 55, 8);
        M_GroupSoul(c, 1, group, 55, 8);
    }
    g_B15J.subphase = g_B15J.frame > 600 ? 2 : 1;
}

static void M_Laser(
    const B15J_CONTEXT *const c, const int32_t i, const int16_t x,
    const int16_t target, const int16_t speed, const int16_t width)
{
    ShootoutLasers_Spawn(
        &g_ShootoutLasers.slots[i], x, 147, target, 400, speed, 10, 20, width);
}

static void M_CrossLasers(const B15J_CONTEXT *const c)
{
    if (g_B15J.frame > 200 && g_B15J.frame % 200 == 0) {
        M_Laser(c, 0, 307, Math16_Wrap(Boss_Player()->left + 60), 50, 8);
        M_Laser(c, 1, 336, Math16_Wrap(Boss_Player()->left - 60), 50, 8);
        Sound_Effect(6);
    }
}

static void M_DropTears(const B15J_CONTEXT *const c)
{
    if (g_B15J.frame < 10) {
        g_B15J.pattern_state = Boss_Rank(200, 160, 140, 120);
    }
    if (g_B15J.frame % g_B15J.pattern_state == 0) {
        for (int32_t i = 0; i < 2; i++) {
            M_AddTear(
                Math16_Wrap(g_B15J.souls[i].left + 12),
                Math16_Wrap(g_B15J.souls[i].top + 8));
        }
    }
    M_MoveSouls(c, 6, 2);
}

static void M_FourSpiral(const B15J_CONTEXT *const c)
{
    if (g_B15J.frame < 100) {
        return;
    }
    if (g_B15J.frame == 100) {
        g_B15J.four_angle = 32;
        g_B15J.pattern_state = Boss_Rank(14, 8, 6, 4);
    }
    if (g_B15J.frame % g_B15J.pattern_state == 0) {
        for (int32_t i = 0; i < 4; i += 2) {
            const int16_t x =
                Math16_PolarX(320, 90, g_B15J.frame * 4 + 128 * i);
            const int16_t y =
                Math16_PolarY(180, 90, g_B15J.frame * 4 + 128 * i);
            for (int32_t j = 0; j < 4; j++) {
                M_Single(
                    c, x, y, (uint8_t)(g_B15J.four_angle + j * 64), 36,
                    PELLET_REGULAR, 0, 0, 0);
            }
        }
        if (g_B15J.frame <= 600) {
            g_B15J.four_angle = (uint8_t)(g_B15J.four_angle - 5);
        } else {
            g_B15J.frame = 0;
            g_B15J.subphase = 1;
        }
    }
}

static void M_LeftLasers(const B15J_CONTEXT *const c)
{
    if (g_B15J.frame <= 200) {
        return;
    }
    if (g_B15J.frame == 250) {
        g_B15J.pattern_state = Boss_Rank(52, 56, 60, 64);
    }
    if (g_B15J.frame >= 250 && g_B15J.frame <= 400
        && (g_B15J.frame - 250) % 30 == 0) {
        Sound_Effect(6);
        const int16_t offset = (int16_t)(MasterRng_Next(&g_Rng) % 120 - 60);
        M_Laser(
            c, (g_B15J.frame - 250) / 30, 307,
            Math16_Wrap(Boss_Player()->left + offset), g_B15J.pattern_state, 4);
    }
    if (g_B15J.frame > 500) {
        g_B15J.frame = 0;
        g_B15J.subphase = 2;
    }
}

static void M_Rain(const B15J_CONTEXT *const c)
{
    if (g_B15J.frame > 100 && g_B15J.frame % g_B15J.pattern_state == 0) {
        for (int32_t i = 0; i < 2; i++) {
            const int16_t x = Math16_Wrap(g_B15J.souls[i].left + 12);
            const int16_t y = Math16_Wrap(g_B15J.souls[i].top + 12);
            for (int32_t j = 0; j < 6; j++) {
                const int16_t speed = (int16_t)((j < 3 ? 3 - j : j - 2) * 16);
                M_Single(
                    c, x, y, j < 3 ? 0 : 128, speed, PELLET_GRAVITY, 1, 0, 0);
            }
            Sound_Effect(7);
        }
    }
    if (g_B15J.frame > 250) {
        g_B15J.frame = 0;
        g_B15J.subphase = 3;
    }
}

static void M_VerticalLasers(const B15J_CONTEXT *const c)
{
    if (g_B15J.frame < 100) {
        return;
    }
    if (g_B15J.frame == 100) {
        g_B15J.pattern_state = Boss_Rank(60, 64, 68, 72);
        g_B15J.random_range = Boss_Rank(0, 10, 16, 20);
    }
    if (g_B15J.frame % 10 == 0) {
        const int32_t i = (g_B15J.frame - 100) / 10;
        const int16_t offset =
            (int16_t)(MasterRng_Next(&g_Rng) % (g_B15J.random_range * 2 + 1)
                      - g_B15J.random_range);
        const int16_t x = Math16_Wrap(i * 64 + offset);
        ShootoutLasers_Spawn(
            &g_ShootoutLasers.slots[i], x, 64, x, 400, g_B15J.pattern_state, 10,
            20, 8);
        Sound_Effect(6);
    }
    if (g_B15J.frame >= 190) {
        g_B15J.frame = 0;
        g_B15J.subphase = 0;
    }
}

static bool M_Death(const B15J_CONTEXT *const c)
{
    const BOSS_DEFEAT_CONTEXT defeat = {
        .post_defeat = &g_B15J.post_defeat,
        .miko = c->miko,
        .origin_top = &g_Orb.top,
        .stage_cleared = c->stage_cleared,
    };
    if (!BossDefeat_Resume(&g_B15J.defeat, &defeat)) {
        return false;
    }
    Scene_Activate(&c->assets->next_scene);
    g_B15J.defeating = false;
    g_B15J.finished = true;
    return true;
}

bool B15J_UpdateAndRender(const B15J_CONTEXT *const c)
{
    if (g_B15J.finished) {
        return true;
    }
    if (g_B15J.defeating) {
        return M_Death(c);
    }
    if (g_B15J.phase == B15J_PHASE_ENTRANCE) {
        return M_Entrance(c);
    }
    if (g_B15J.phase == B15J_PHASE_SPIRAL) {
        if (!g_B15J.hp_bar_filled) {
            g_B15J.hp_bar_filled = HudHp_Increment(g_B15J.hp, g_B15J.frame);
        }
        M_Common();
        M_SymmetricSpiral(c);
        M_Hit(c);
        if (g_B15J.frame == 0) {
            g_B15J.patterns_done = Math16_Wrap(g_B15J.patterns_done + 1);
        }
        if (!g_B15J.hit.invincible
            && (g_B15J.hp <= 10 || g_B15J.patterns_done >= 6)) {
            g_B15J.phase = B15J_PHASE_RECOLOR;
            g_B15J.frame = 0;
            g_B15J.hit.frame = 0;
            g_B15J.patterns_done = 0;
        }
    } else if (g_B15J.phase == B15J_PHASE_RECOLOR) {
        g_B15J.frame = Math16_Wrap(g_B15J.frame + 1);
        if (g_B15J.frame % 20 == 19) {
            g_Palette.saved.colors[5][0] =
                M_Byte(g_Palette.saved.colors[5][0] + 1);
            g_Palette.saved.colors[5][2] =
                M_Byte(g_Palette.saved.colors[5][2] - 1);
            g_StagePalette.colors[5][0] =
                M_Byte(g_StagePalette.colors[5][0] + 1);
            g_StagePalette.colors[5][2] =
                M_Byte(g_StagePalette.colors[5][2] - 1);
            Palette_Set(&g_Palette, &g_StagePalette);
        }
        if (g_B15J.frame >= 100) {
            g_B15J.phase = B15J_PHASE_SOULS;
            g_B15J.frame = 0;
            g_B15J.subphase = 0;
            Palette_Set(&g_Palette, &g_StagePalette);
            g_B15J.boss_palette = g_Palette.saved;
        }
    } else if (g_B15J.phase == B15J_PHASE_SOULS) {
        M_Common();
        M_SpinRings(c);
        if (g_B15J.subphase == 0) {
            M_ActivateSouls(c);
        } else {
            M_SoulSpreads(c);
        }
        M_Hit(c);
        if (!g_B15J.hit.invincible && g_B15J.subphase == 2) {
            g_B15J.phase = B15J_PHASE_TEARS;
            g_B15J.subphase = 0;
            g_B15J.frame = 0;
        }
    } else if (g_B15J.phase == B15J_PHASE_TEARS) {
        M_Common();
        M_DropTears(c);
        M_CrossLasers(c);
        M_Hit(c);
        if (!g_B15J.hit.invincible && (g_B15J.hp <= 6 || g_B15J.frame > 1600)) {
            g_B15J.phase = B15J_PHASE_PATTERNS;
            g_B15J.frame = 0;
            g_B15J.hit.frame = 0;
            g_B15J.patterns_done = 0;
        }
    } else if (g_B15J.phase == B15J_PHASE_PATTERNS) {
        M_Common();
        if (g_B15J.frame % 200 == 0) {
            M_GroupSoul(c, 0, PELLET_GROUP_SINGLE_AIMED, 40, 12);
            M_GroupSoul(c, 1, PELLET_GROUP_SINGLE_AIMED, 40, 12);
        }
        M_MoveSouls(c, 4, 2);
        switch (g_B15J.subphase) {
        case 0:
            M_FourSpiral(c);
            break;
        case 1:
            M_LeftLasers(c);
            break;
        case 2:
            M_Rain(c);
            break;
        case 3:
            M_VerticalLasers(c);
            break;
        default:
            break;
        }
        if (g_B15J.frame == 0) {
            g_B15J.patterns_done = Math16_Wrap(g_B15J.patterns_done + 1);
        }
        if (g_B15J.patterns_done > 29) {
            g_B15J.hp = 1;
        }
        M_Hit(c);
        if (g_B15J.hp <= 0) {
            Sound_Fade();
            Pellets_EraseAndResetMoving();
            ShootoutLasers_EraseAndReset(4);
            BossDefeat_Begin(&g_B15J.defeat, BOSS_DEFEAT_REGULAR);
            g_B15J.defeating = true;
            return M_Death(c);
        }
    }
    return true;
}
