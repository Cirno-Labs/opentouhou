#include "th01/boss_b10j.h"
#include "th01/session.h"
#include "th01/sound.h"
#include "th01/rng.h"

#include "th01/boss.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/assert.h"
#include "th01/shape.h"
#include "th01/sprite.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

B10J g_B10J;

static RESULT M_LoadMissiles(
    VFS *const vfs, const ALLOCATOR *const a, PTN *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS3_M.PTN", VFS_IN_MEMORY, &reader));
    const RESULT result = Ptn_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadPillars(VFS *const vfs, B10J_PILLARS *const out)
{
    uint8_t *masks;
    size_t size;
    MUST(SpriteSheet_Load(
        vfs, "pillar.bmp", 32, 8, &g_HeapAllocator, &masks, &size));
    if (size != sizeof(out->masks)) {
        Mem_Free(masks);
        return FAIL("Invalid pillar.bmp size");
    }
    memcpy(out->masks, masks, sizeof(out->masks));
    Mem_Free(masks);
    return OK;
}

RESULT B10J_ValidateAssets(const B10J_ASSETS *const a)
{
    FAIL_IF(
        a == nullptr || a->still.pixels == nullptr
            || a->still.coverage == nullptr || a->still.width != 128
            || a->still.height != 160 || a->still.count != 1,
        "Invalid Mima still bank");
    FAIL_IF(
        a->animation.pixels == nullptr || a->animation.coverage == nullptr
            || a->animation.width != 128 || a->animation.height != 64
            || a->animation.count != 5,
        "Invalid Mima animation bank");
    FAIL_IF(
        a->background.width != 640 || a->background.height != 400
            || a->background.pixels == nullptr || !a->background.has_palette,
        "Invalid Mima background");
    MUST(Missiles_ValidateSprites(&a->missiles));
    FAIL_IF(
        a->next_scene.id != 4 || a->next_scene.layouts.stages == nullptr,
        "Missing Mima scene");
    return OK;
}

RESULT B10J_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const a, B10J_ASSETS *const out)
{
    *out = (B10J_ASSETS) {};
    B10J_ASSETS loaded = {};
    MUST(Boss_LoadBos(vfs, "BOSS3_1.BOS", a, &loaded.still));
    MUST(Boss_LoadBos(vfs, "BOSS3_2.BOS", a, &loaded.animation));
    MUST(Boss_LoadBackground(vfs, "BOSS3.GRP", a, &loaded.background));
    MUST(M_LoadMissiles(vfs, a, &loaded.missiles));
    MUST(Scene_Load(vfs, 4, a, &loaded.next_scene));
    MUST(M_LoadPillars(vfs, &loaded.pillars));
    MUST(B10J_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B10J_Init(void)
{
    memset(&g_B10J, 0, sizeof(g_B10J));
    g_B10J.meteor_active = true;
    g_B10J.spread_interval = 4;
    g_B10J.spread_speed = 8;
}

void B10J_BeginSetup(const B10J_CONTEXT *const c)
{
    ASSERT(
        !g_B10J.setup_active && !g_B10J.entrance_active && !g_B10J.defeating);
    ASSERT(Result_Ignore(B10J_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(BossEntity_Bind(&g_B10J.still, 0, &c->assets->still)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&g_B10J.animation, 1, &c->assets->animation)));
    Palette_Set(c->hit->palette, &c->assets->palette);
    g_B10J.post_defeat = c->hit->palette->saved;
    g_B10J.boss_palette = c->hit->palette->saved;
    g_B10J.still.image = 0;
    g_B10J.animation.image = 1;
    Palette_BeginFade(
        &g_B10J.setup_fade, c->hit->palette, PALETTE_FADE_WHITE_IN);
    Frame_BeginWait(&g_B10J.setup_wait);
    g_B10J.setup_active = true;
    g_B10J.finished = false;
}

bool B10J_ResumeSetup(const B10J_CONTEXT *const c)
{
    if (!g_B10J.setup_active) {
        return true;
    }
    if (!Frame_WaitReady(&g_B10J.setup_wait)) {
        return false;
    }
    Palette_FadeStep(&g_B10J.setup_fade, c->hit->palette);
    if (g_B10J.setup_fade.remaining != 0) {
        Frame_BeginWait(&g_B10J.setup_wait);
        return false;
    }
    BossEntity_SetPosition(
        &g_B10J.still, 256, 64, (BOSS_BOUNDS) { 0, 736, 64, 304 });
    BossEntity_SetOrbHitbox(&g_B10J.still, (BOSS_BOUNDS) { 16, 112, 16, 144 });
    g_B10J.still.orb_inactive = false;
    g_B10J.frame = 0;
    g_B10J.phase = B10J_PHASE_START;
    g_B10J.hp = 12;
    g_HudHp.first_white = 6;
    g_HudHp.first_redwhite = 2;
    Particles_Init();
    Missiles_Reset();
    *c->stage_palette = c->hit->palette->saved;
    g_B10J.setup_active = false;
    return true;
}

static void M_Snap(const B10J_CONTEXT *const c)
{
    Video_Read(
        1, (GFX_RECT) { g_B10J.still.left, g_B10J.still.top, 128, 160 },
        g_B10J.background, 128);
    Video_SelectDrawingPage(0);
}

static void M_Erase(const B10J_CONTEXT *const c)
{
    const VIDEO_IMAGE bg = { .pixels = g_B10J.background,
                             .width = 128,
                             .height = 160,
                             .stride = 128,
                             .key = -1 };
    Video_Blit(
        &bg, (GFX_RECT) { 0, 0, 128, 160 }, g_B10J.still.left,
        g_B10J.still.top);
}

static void M_EraseBoth(const B10J_CONTEXT *const c)
{
    Video_SelectDrawingPage(1);
    M_Erase(c);
    Video_SelectDrawingPage(0);
    M_Erase(c);
}

static void M_PutStill(const B10J_CONTEXT *const c)
{
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        BossEntity_Put(
            &g_B10J.still, g_B10J.still.left, g_B10J.still.top,
            g_B10J.still.image);
    }
}

static void M_AnimationBoth(const B10J_CONTEXT *const c, const int16_t image)
{
    g_B10J.animation.left = g_B10J.still.left;
    g_B10J.animation.top = Math16_Wrap(g_B10J.still.top + 48);
    g_B10J.animation.image = image;
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        BossEntity_Put(
            &g_B10J.animation, g_B10J.animation.left, g_B10J.animation.top,
            image);
    }
}

static void M_Cast(const B10J_CONTEXT *const c)
{
    g_B10J.meteor_active = false;
    M_AnimationBoth(c, 0);
}

static void M_Activate(const B10J_CONTEXT *const c)
{
    if (!g_B10J.meteor_active) {
        g_B10J.meteor_active = true;
        M_AnimationBoth(c, 1);
    }
}

static void M_Meteor(const B10J_CONTEXT *const c)
{
    if (g_B10J.meteor_active && g_B10J.frame % 8 == 0) {
        g_B10J.animation.left = g_B10J.still.left;
        g_B10J.animation.top = Math16_Wrap(g_B10J.still.top + 48);
        g_B10J.animation.image = (int16_t)(1 + (g_B10J.frame / 8) % 4);
        BossEntity_RestorePut(
            &g_B10J.animation, g_B10J.animation.left, g_B10J.animation.top,
            g_B10J.animation.image);
    }
}

static void M_Spread(
    const B10J_CONTEXT *const c, const int16_t left, const int16_t top)
{
    if (g_B10J.frame < 10) {
        return;
    }
    if (g_B10J.frame == 10) {
        g_B10J.still.left = left;
        g_B10J.still.top = top;
        M_Snap(c);
        return;
    }
    if (g_B10J.frame % g_B10J.spread_interval != 0) {
        return;
    }
    const int16_t bottom = Math16_Wrap(
        (g_B10J.spread_speed / g_B10J.spread_interval) * (g_B10J.frame - 10));
    const int16_t start = Math16_Wrap(80 - bottom);
    if (start < 0) {
        g_B10J.frame = 0;
        M_PutStill(c);
        return;
    }
    for (int16_t row = 0; row < bottom; row++) {
        BossEntity_RestorePutLine(
            &g_B10J.still, left, Math16_Wrap(top + start + row),
            g_B10J.still.image, row);
        BossEntity_RestorePutLine(
            &g_B10J.still, left, Math16_Wrap(top + 160 - start - row),
            g_B10J.still.image, (int16_t)(159 - row));
    }
}

static void M_BrokenTransition(const B10J_CONTEXT *const c)
{
    if (g_B10J.frame < 10 || g_B10J.frame % 4 != 0) {
        return;
    }
    const int16_t half = Math16_Wrap((g_B10J.frame - 10) * 2);
    if (half >= 80) {
        g_B10J.frame = 0;
        return;
    }
    Sprite_Restore16(
        g_B10J.still.left, Math16_Wrap(g_B10J.still.top + half),
        g_B10J.still.sprites.width_bytes, 8);
    Sprite_Restore16(
        g_B10J.still.left, Math16_Wrap(g_B10J.still.top + 152 - half),
        g_B10J.still.sprites.width_bytes, 8);
}

static void M_Corners(
    int16_t x[4], int16_t y[4], const int16_t radius, const uint8_t angle)
{
    for (int32_t i = 0; i < 4; i++) {
        x[i] = Math16_PolarX(
            Math16_Wrap(g_B10J.still.left + 64), radius, angle + i * 64);
        y[i] = Math16_PolarY(
            Math16_Wrap(g_B10J.still.top + 80), radius, angle + i * 64);
    }
}

static void M_Loop(
    const B10J_CONTEXT *const c, const int16_t x[4], const int16_t y[4],
    const bool restore)
{
    for (int32_t i = 0; i < 4; i++) {
        const int32_t next = (i + 1) % 4;
        if (restore) {
            Video_LineErase(x[i], y[i], x[next], y[next]);
        } else {
            Video_Line(x[i], y[i], x[next], y[next], 7);
        }
    }
}

static void M_Add(
    const B10J_CONTEXT *const c, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed, const bool chase)
{
    Pellets_AddSingle(
        g_Session.settings.difficulty, x, y, angle, speed,
        chase ? PELLET_CHASE : PELLET_REGULAR, chase ? speed : 0, 0, 0);
}

static void M_SquareInit(B10J_SQUARE *const sq)
{
    sq->angle = 0;
    sq->radius = 32;
}

static void M_AimedStatic(const B10J_CONTEXT *const c)
{
    B10J_SQUARE *const sq = &g_B10J.squares[0];
    if (g_B10J.frame < 100) {
        return;
    }
    if (g_B10J.frame == 100) {
        M_SquareInit(sq);
        g_B10J.pattern_state = Boss_Rank(64, 72, 80, 88);
        Sound_Effect(8);
    }
    int16_t x[4], y[4];
    if (g_B10J.frame % 8 == 0) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        sq->angle = (uint8_t)(sq->angle + (g_B10J.frame < 260 ? 12 : -12));
        if (sq->radius < 112) {
            sq->radius += 8;
        } else {
            for (int32_t i = 0; i < 4; i++) {
                const uint8_t angle = g_B10J.frame > 280
                    ? Math16_Angle(
                          Math16_Wrap(y[i] - g_B10J.still.top - 80),
                          Math16_Wrap(x[i] - g_B10J.still.left - 64))
                    : Math16_Angle(
                          Math16_Wrap(384 - y[i]),
                          Math16_Wrap(Boss_Player()->left + 12 - x[i]));
                M_Add(
                    c, x[i], y[i], angle,
                    g_B10J.frame > 280 ? g_B10J.pattern_state
                                       : (int16_t)(g_B10J.pattern_state / 2),
                    false);
                Sound_Effect(7);
            }
        }
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, false);
    }
    if (g_B10J.frame > 360) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        g_B10J.frame = 0;
    }
}

static void M_AimedMissiles(const B10J_CONTEXT *const c)
{
    B10J_SQUARE *const sq = &g_B10J.squares[1];
    if (g_B10J.frame < 100) {
        return;
    }
    if (g_B10J.frame == 100) {
        M_SquareInit(sq);
        g_B10J.pattern_state = Boss_Rank(96, 104, 112, 118);
        Sound_Effect(8);
    }
    int16_t x[4], y[4];
    if (g_B10J.frame % 8 == 0) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        sq->angle -= 12;
        if (sq->radius < 80) {
            sq->radius += 8;
        } else if (g_B10J.frame == 224) {
            g_B10J.target_left = Boss_Player()->left;
        } else if (g_B10J.frame > 240) {
            const MATH16_VECTOR velocity = Math16_Aim(
                (MATH16_VECTOR) { Math16_Wrap(g_B10J.still.left + 64),
                                  Math16_Wrap(g_B10J.still.top + 80) },
                (MATH16_VECTOR) { Math16_Wrap(g_B10J.target_left + 8), 384 },
                g_B10J.pattern_state, 0);
            for (int32_t i = 0; i < 4; i++) {
                Missiles_Add(
                    x[i], y[i], Math16_ToPixel(velocity.x),
                    Math16_ToPixel(velocity.y), 0);
            }
            Sound_Effect(6);
        }
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, false);
    }
    if (g_B10J.frame > 320) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        g_B10J.frame = 0;
    }
}

static void M_TwoSquares(const B10J_CONTEXT *const c)
{
    B10J_SQUARE *const sq = &g_B10J.squares[2];
    if (g_B10J.frame == 50) {
        M_Cast(c);
    }
    if (g_B10J.frame < 100) {
        return;
    }
    if (g_B10J.frame == 100) {
        M_SquareInit(sq);
        g_B10J.pattern_state = Boss_Rank(64, 72, 80, 88);
        Sound_Effect(8);
    }
    int16_t x[4], y[4], x2[4], y2[4];
    if (g_B10J.frame % 8 == 0) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Corners(x2, y2, sq->radius, (uint8_t)-sq->angle);
        M_Loop(c, x, y, true);
        M_Loop(c, x2, y2, true);
        sq->angle -= 6;
        g_Pellets.spawn_with_cloud = true;
        if (sq->radius < 112) {
            sq->radius += 8;
        } else {
            for (int32_t i = 0; i < 4; i++) {
                M_Add(
                    c, x[i], y[i],
                    Math16_Angle(
                        Math16_Wrap(y[i] - g_B10J.still.top - 80),
                        Math16_Wrap(x[i] - g_B10J.still.left - 64)),
                    g_B10J.pattern_state, false);
                M_Add(
                    c, x2[i], y2[i],
                    Math16_Angle(
                        Math16_Wrap(y2[i] - g_B10J.still.top - 80),
                        Math16_Wrap(x2[i] - g_B10J.still.left - 64)),
                    g_B10J.pattern_state, false);
                Sound_Effect(7);
            }
        }
        M_Corners(x, y, sq->radius, sq->angle);
        M_Corners(x2, y2, sq->radius, (uint8_t)-sq->angle);
        M_Loop(c, x, y, false);
        M_Loop(c, x2, y2, false);
        g_Pellets.spawn_with_cloud = false;
    }
    if (g_B10J.frame > 320) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Corners(x2, y2, sq->radius, (uint8_t)-sq->angle);
        M_Loop(c, x, y, true);
        M_Loop(c, x2, y2, true);
        g_B10J.frame = 0;
        M_Activate(c);
    }
}

static void M_InitHop(const B10J_CONTEXT *const c)
{
    g_B10J.hop = 255;
    g_B10J.spread_interval = 4;
    g_B10J.direction = (uint8_t)(MasterRng_Next(&g_Rng) % 2);
    g_B10J.spread_speed = 16;
}

static void M_Hop(const B10J_CONTEXT *const c)
{
    if (g_B10J.hop == 255) {
        M_EraseBoth(c);
        g_B10J.meteor_active = false;
        g_B10J.hop = 0;
        g_B10J.frame = 1;
    }
    if (g_B10J.hop == 0) {
        g_B10J.still.orb_inactive = true;
        M_BrokenTransition(c);
    } else {
        if (g_B10J.frame == 4) {
            g_B10J.still.left = g_B10J.direction == 0
                ? (int16_t)((g_B10J.hop - 1) * 128)
                : (int16_t)(640 - g_B10J.hop * 128);
            g_B10J.still.top = 96;
            M_Snap(c);
        }
        g_B10J.still.orb_inactive = g_B10J.frame < 12;
        M_Spread(c, g_B10J.still.left, g_B10J.still.top);
    }
    if (g_B10J.frame != 0) {
        return;
    }
    if (g_B10J.hop != 0) {
        if (g_B10J.hop != 4) {
            M_EraseBoth(c);
        }
        g_B10J.pattern_state = Boss_Rank(30, 35, 40, 45);
        const int16_t count = Boss_Rank(5, 8, 10, 12);
        uint8_t angle = 0;
        for (int16_t i = 0; i < count;
             i++, angle = (uint8_t)(angle + 256 / count)) {
            M_Add(
                c, Math16_Wrap(g_B10J.still.left + 64),
                Math16_Wrap(g_B10J.still.top + 80), angle, g_B10J.pattern_state,
                true);
        }
    }
    if (g_B10J.hop >= 4) {
        M_PutStill(c);
        Palette_Set(c->hit->palette, c->stage_palette);
        g_B10J.hop = 255;
        g_B10J.direction = (uint8_t)(1 - g_B10J.direction);
        g_B10J.meteor_active = true;
        g_B10J.frame = 0;
    } else {
        for (int32_t i = 0; i < 16; i++) {
            const int8_t *const color = c->hit->palette->saved.colors[i];
            Palette_SetColor(
                c->hit->palette, i, color[0] - 4, color[1] - 4, color[2] - 4);
        }
        g_B10J.hop++;
        g_B10J.frame = 1;
    }
}

static void M_Pillar(
    const B10J_CONTEXT *const c, const int16_t left, const int16_t bottom,
    const int16_t height)
{
    const int32_t segments = height > 8 ? height - 8 : 0;
    uint16_t offset = (uint16_t)(bottom * 80 + left / 8 - (left % 8 < 0));
    for (int32_t row = 0; row < segments + 8; row++) {
        const int32_t part = row < segments ? 0 : 1;
        const int32_t index = 7 - ((row < segments ? row : row - segments) % 8);
        const uint8_t *const mask = c->assets->pillars.masks[part][index];
        for (int32_t byte = 0; byte < 4; byte++) {
            const uint32_t address = (offset + (uint32_t)byte) & 0x7fffu;
            if (address >= 32000) {
                continue;
            }
            for (int32_t bit = 0; bit < 8; bit++) {
                if ((mask[byte] & (0x80u >> bit)) != 0) {
                    g_Video.pages[g_Video.drawing_page][address * 8 + bit] = 10;
                }
            }
        }
        offset = (uint16_t)(offset - 80);
    }
}

static void M_Pillars(const B10J_CONTEXT *const c)
{
    if (g_B10J.frame == 50) {
        M_Cast(c);
    }
    if (g_B10J.frame < 100) {
        return;
    }
    if (g_B10J.frame == 100) {
        for (int32_t i = 0; i < 8; i++) {
            g_B10J.pillar_time[i] = 128;
            g_B10J.pillar_bottom[i] = 400;
        }
        g_B10J.pillar_x[0] = (int16_t)(16 + MasterRng_Next(&g_Rng) % 48);
        g_B10J.pillar_x[1] = (int16_t)(624 - MasterRng_Next(&g_Rng) % 48);
        g_B10J.pillar_x[2] = (int16_t)(64 + MasterRng_Next(&g_Rng) % 224);
        g_B10J.pillar_x[3] = (int16_t)(336 + MasterRng_Next(&g_Rng) % 224);
        g_B10J.pillar_x[4] = (int16_t)(128 + MasterRng_Next(&g_Rng) % 160);
        g_B10J.pillar_x[5] = (int16_t)(336 + MasterRng_Next(&g_Rng) % 160);
        g_B10J.pillar_x[6] = (int16_t)(64 + MasterRng_Next(&g_Rng) % 224);
        g_B10J.pillar_x[7] = (int16_t)(336 + MasterRng_Next(&g_Rng) % 224);
        g_B10J.pattern_state = Boss_Rank(40, 45, 50, 55);
        Sound_Effect(8);
    }
    for (int32_t i = 0; i < 8; i++) {
        if (g_B10J.frame % 8 == i && g_B10J.frame > 100 + i * 20
            && g_B10J.pillar_time[i] > 32) {
            Shape_EllipseArcErase(
                g_B10J.pillar_x[i], g_B10J.pillar_bottom[i],
                g_B10J.pillar_time[i], g_B10J.pillar_time[i], 4, 0, 255);
            g_B10J.pillar_time[i] -= 8;
        }
    }
    if (g_B10J.frame <= 240 && (g_B10J.frame - 100) % 20 == 0) {
        Sound_Effect(12);
    }
    if (g_B10J.frame == 240) {
        M_Activate(c);
    }
    for (int32_t i = 0; i < 8; i++) {
        if (g_B10J.frame % 8 != i || g_B10J.frame <= 100 + i * 20) {
            continue;
        }
        const int16_t x = g_B10J.pillar_x[i], bottom = g_B10J.pillar_bottom[i];
        if (g_B10J.pillar_time[i] > 32) {
            Shape_EllipseArcPut(
                x, bottom, g_B10J.pillar_time[i], g_B10J.pillar_time[i], 7, 4,
                0, 255);
            Video_Line(320, 200, x, bottom, 1);
        } else if (g_B10J.pillar_time[i] > 16) {
            if (g_B10J.pillar_time[i] == 32) {
                Sound_Effect(7);
                Video_LineErase(320, 200, x, bottom);
            }
            M_Pillar(
                c, Math16_Wrap(x - 16), bottom,
                Math16_Wrap(16 * (34 - g_B10J.pillar_time[i])));
            if (!Boss_Player()->invincible
                && Boss_Player()->left > Math16_Wrap(x - 40)
                && Boss_Player()->left < Math16_Wrap(x + 8)) {
                Boss_Player()->is_hit = true;
            }
            g_B10J.pillar_time[i] -= 8;
        } else if (g_B10J.pillar_time[i] != -999) {
            Pellets_AddGroup(
                g_Session.settings.difficulty,
                Math16_Wrap(g_B10J.still.left + 64),
                Math16_Wrap(g_B10J.still.top + 80),
                PELLET_GROUP_THREE_WIDE_AIMED, 36);
            Sprite_Restore16(Math16_Wrap(x - 16), 176, 32, 224);
            g_B10J.pillar_time[i] = -999;
        }
    }
    bool done = true;
    for (int32_t i = 0; i < 8; i++) {
        done = done && g_B10J.pillar_time[i] == -999;
    }
    if (done) {
        g_B10J.frame = 0;
    }
}

static void M_Halfcircle(const B10J_CONTEXT *const c)
{
    B10J_SQUARE *const sq = &g_B10J.squares[3];
    if (g_B10J.frame < 100) {
        return;
    }
    if (g_B10J.frame == 100) {
        M_SquareInit(sq);
        g_B10J.missile_angle = 0;
        g_B10J.pattern_state = Boss_Rank(40, 45, 50, 55);
        Sound_Effect(8);
    }
    int16_t x[4], y[4];
    if (g_B10J.frame % 8 == 0) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        sq->angle -= 12;
        if (sq->radius < 80) {
            sq->radius += 8;
        } else if (g_B10J.frame > 180 && g_B10J.frame % 16 == 8) {
            const MATH16_VECTOR velocity = Math16_Vector(
                (int16_t)(g_B10J.pattern_state / 10), g_B10J.missile_angle);
            for (int32_t i = 0; i < 4; i++) {
                Missiles_Add(x[i], y[i], velocity.x, velocity.y, 0);
            }
            Sound_Effect(6);
            g_B10J.missile_angle += 13;
        }
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, false);
    }
    if (g_B10J.frame > 340) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        g_B10J.frame = 0;
    }
}

static void M_Spray(const B10J_CONTEXT *const c)
{
    B10J_SQUARE *const sq = &g_B10J.squares[4];
    if (g_B10J.frame < 100) {
        return;
    }
    if (g_B10J.frame == 100) {
        M_SquareInit(sq);
        g_B10J.pellet_angle = 128;
        g_B10J.pattern_state = Boss_Rank(32, 40, 48, 56);
        Sound_Effect(8);
    }
    int16_t x[4], y[4];
    if (g_B10J.frame % 8 == 0) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        sq->angle = (uint8_t)(sq->angle + (g_B10J.frame > 270 ? 12 : -12));
        if (sq->radius < 80) {
            sq->radius += 8;
        } else if (g_B10J.frame > 180) {
            g_B10J.pellet_angle = (uint8_t)(g_B10J.pellet_angle
                                            + (g_B10J.frame > 270 ? 12 : -12));
            for (int32_t i = 0; i < 4; i++) {
                M_Add(
                    c, x[i], y[i], g_B10J.pellet_angle, g_B10J.pattern_state,
                    false);
            }
            Sound_Effect(6);
        }
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, false);
    }
    if (g_B10J.frame > 370) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        g_B10J.frame = 0;
    }
}

static void M_Lasers(const B10J_CONTEXT *const c)
{
    B10J_SQUARE *const sq = &g_B10J.squares[5];
    if (g_B10J.frame == 50) {
        M_Cast(c);
    }
    if (g_B10J.frame < 100) {
        return;
    }
    if (g_B10J.frame == 100) {
        M_SquareInit(sq);
        g_B10J.pattern_state = Boss_Rank(50, 54, 58, 62);
        Sound_Effect(8);
    }
    if (g_B10J.frame % 8 == 0) {
        M_Corners(g_B10J.laser_x, g_B10J.laser_y, sq->radius, sq->angle);
        M_Loop(c, g_B10J.laser_x, g_B10J.laser_y, true);
        sq->angle += 3;
        if (sq->radius < 80) {
            sq->radius += 8;
        }
        M_Corners(g_B10J.laser_x, g_B10J.laser_y, sq->radius, sq->angle);
        M_Loop(c, g_B10J.laser_x, g_B10J.laser_y, false);
    }
    if (g_B10J.frame > 180 && g_B10J.frame < 300) {
        const int32_t i = g_B10J.frame % 4;
        ShootoutLasers_Spawn(
            &g_ShootoutLasers.slots[i], g_B10J.laser_x[i], g_B10J.laser_y[i],
            Math16_Wrap(Boss_Player()->left + 14), 400, g_B10J.pattern_state, 7,
            20, 4);
    }
    if (g_B10J.frame > 300) {
        M_Corners(g_B10J.laser_x, g_B10J.laser_y, sq->radius, sq->angle);
        M_Loop(c, g_B10J.laser_x, g_B10J.laser_y, true);
        g_B10J.frame = 0;
    }
}

static void M_Hit(const B10J_CONTEXT *const c)
{
    const uint8_t flash[] = { 3, 9 };
    BossHit_UpdateAndRender(
        &g_B10J.hit, c->hit, &g_B10J.hp, &g_B10J.boss_palette, flash, 2, 5000,
        BossEntity_HitTestOrb(&g_B10J.still),
        (BOSS_SHOT_HITBOX) { Math16_Wrap(g_B10J.still.left + 16),
                             Math16_Wrap(g_B10J.still.top + 48), 96, 48 });
}

static void M_MissileAction(void *const user, const MISSILES_ACTION action)
{
    const B10J_CONTEXT *const c = user;
    if (action.kind == MISSILES_ACTION_EFFECT) {
        Sound_Effect(action.effect);
    } else {
        Missiles_RenderAction(&c->assets->missiles, action);
    }
}

static bool M_Entrance(const B10J_CONTEXT *const c)
{
    for (;;) {
        if (g_B10J.entrance_waiting) {
            if (!Frame_DelayReady(2)) {
                return false;
            }
            g_B10J.entrance_waiting = false;
        }
        g_B10J.frame = Math16_Wrap(g_B10J.frame + 1);
        M_Spread(c, 256, 120);
        if (g_B10J.frame == 0) {
            g_B10J.phase = B10J_PHASE_FIRST;
            g_B10J.pattern = 0;
            g_B10J.hp_bar_filled = false;
            *c->stage_palette = c->hit->palette->saved;
            g_B10J.boss_palette = c->hit->palette->saved;
            M_InitHop(c);
            g_B10J.entrance_active = false;
            return true;
        }
        g_B10J.entrance_waiting = true;
    }
}

static bool M_Death(const B10J_CONTEXT *const c)
{
    const BOSS_DEFEAT_CONTEXT defeat = { .palette = c->hit->palette,
                                         .post_defeat = &g_B10J.post_defeat,
                                         .miko = c->miko,
                                         .origin_top = &g_Orb.top,
                                         .stage_cleared = c->stage_cleared };
    if (!BossDefeat_Resume(&g_B10J.defeat, &defeat)) {
        return false;
    }
    Scene_Activate(&c->assets->next_scene);
    g_B10J.defeating = false;
    g_B10J.finished = true;
    return true;
}

bool B10J_UpdateAndRender(const B10J_CONTEXT *const c)
{
    ASSERT(!g_B10J.setup_active);
    if (g_B10J.finished) {
        return true;
    }
    if (g_B10J.entrance_active) {
        return M_Entrance(c);
    }
    if (g_B10J.defeating) {
        return M_Death(c);
    }
    Missiles_UpdateAndRender(
        (uint16_t)c->hit->frame_rand,
        (MISSILES_OUTPUT) { M_MissileAction, (void *)c });
    Particles_UpdateAndRender(PARTICLES_TOP_RIGHT, 7);
    if (g_B10J.phase == B10J_PHASE_START) {
        Video_SelectDrawingPage(0);
        g_B10J.frame = 0;
        g_B10J.still.orb_inactive = false;
        g_B10J.spread_interval = 1;
        g_B10J.spread_speed = 2;
        g_B10J.meteor_active = true;
        g_B10J.entrance_active = true;
        return M_Entrance(c);
    }
    if (g_B10J.phase == B10J_PHASE_FIRST) {
        if (!g_B10J.hp_bar_filled) {
            g_B10J.hp_bar_filled = HudHp_Increment(g_B10J.hp, g_B10J.frame);
        }
        g_B10J.frame = Math16_Wrap(g_B10J.frame + 1);
        g_B10J.hit.frame = Math16_Wrap(g_B10J.hit.frame + 1);
        M_Meteor(c);
        switch (g_B10J.pattern) {
        case 0:
            M_AimedStatic(c);
            break;
        case 1:
            M_AimedMissiles(c);
            break;
        case 2:
            M_TwoSquares(c);
            break;
        case 3:
            M_Hop(c);
            break;
        default:
            break;
        }
        if (g_B10J.frame == 0) {
            g_B10J.pattern =
                g_B10J.pattern == 3 ? 0 : Math16_Wrap(g_B10J.pattern + 1);
            if (g_B10J.hp <= 6) {
                g_B10J.pattern = 99;
            }
        }
        M_Hit(c);
        if (g_B10J.pattern == 99 && !g_B10J.hit.invincible) {
            M_EraseBoth(c);
            g_B10J.spread_interval = 4;
            g_B10J.spread_speed = 8;
            g_B10J.still.left = 256;
            g_B10J.still.top = 120;
            M_Snap(c);
            g_B10J.phase = B10J_PHASE_SPREAD;
            g_B10J.frame = 0;
            g_B10J.hit.frame = 0;
            g_B10J.pattern = 0;
        }
    } else if (g_B10J.phase == B10J_PHASE_SPREAD) {
        g_B10J.frame = Math16_Wrap(g_B10J.frame + 1);
        g_B10J.hit.frame = Math16_Wrap(g_B10J.hit.frame + 1);
        M_Spread(c, 256, 120);
        if (g_B10J.frame == 0) {
            g_B10J.phase = B10J_PHASE_SECOND;
            g_B10J.frame = 0;
            g_B10J.hit.frame = 0;
            g_B10J.pattern = 0;
        }
    } else if (g_B10J.phase == B10J_PHASE_SECOND) {
        g_B10J.frame = Math16_Wrap(g_B10J.frame + 1);
        g_B10J.hit.frame = Math16_Wrap(g_B10J.hit.frame + 1);
        M_Meteor(c);
        switch (g_B10J.pattern) {
        case 0:
            M_Pillars(c);
            break;
        case 1:
            M_Halfcircle(c);
            break;
        case 2:
            M_Spray(c);
            break;
        case 3:
            M_Lasers(c);
            break;
        default:
            break;
        }
        if (g_B10J.frame == 0) {
            g_B10J.pattern =
                g_B10J.pattern == 3 ? 0 : Math16_Wrap(g_B10J.pattern + 1);
        }
        M_Hit(c);
        if (g_B10J.hp <= 0) {
            Video_SelectDrawingPage(1);
            M_Erase(c);
            Video_SelectDrawingPage(0);
            Sound_Fade();
            Pellets_EraseAndResetMoving();
            Missiles_Reset();
            ShootoutLasers_EraseAndReset(5);
            BossDefeat_Begin(&g_B10J.defeat, BOSS_DEFEAT_REGULAR);
            g_B10J.defeating = true;
            return M_Death(c);
        }
    }
    return true;
}
