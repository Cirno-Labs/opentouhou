#include "th01/boss/private.h"
#include "th01/missiles.h"
#include "th01/particles.h"
#include "th01/pellets.h"
#include "th01/rng.h"
#include "th01/scene.h"
#include "th01/session.h"
#include "th01/shape.h"
#include "th01/shootout_lasers.h"
#include "th01/sound.h"
#include "th01/sprite.h"

#include "core/base/assert.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/pi.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

typedef struct {
    uint8_t masks[2][8][4];
} B10J_PILLARS;

typedef struct {
    BOS still;
    BOS animation;
    PTN missiles;
    PI_IMAGE background;
    PALETTE palette;
    SCENE_DATA next_scene;
    B10J_PILLARS pillars;
} B10J_ASSETS;

typedef struct {
    uint8_t angle;
    int16_t radius;
} B10J_SQUARE;

typedef enum {
    B10J_PHASE_START,
    B10J_PHASE_FIRST,
    B10J_PHASE_SPREAD,
    B10J_PHASE_SECOND,
} B10J_PHASE;

typedef struct {
    BOSS_ENTITY still;
    BOSS_ENTITY animation;
    BOSS_HIT hit;
    PALETTE boss_palette;
    PALETTE post_defeat;
    PALETTE_FADE setup_fade;
    VSYNC_WAIT setup_wait;
    BOSS_DEFEAT defeat;
    uint8_t background[128 * 160];
    B10J_SQUARE squares[6];
    int16_t pillar_time[8];
    int16_t pillar_x[8];
    int16_t pillar_bottom[8];
    int16_t laser_x[4];
    int16_t laser_y[4];
    BOSS_STATE status;
    int16_t pattern;
    int16_t pattern_state;
    int16_t target_left;
    uint8_t hop;
    uint8_t direction;
    uint8_t missile_angle;
    uint8_t pellet_angle;
    uint8_t spread_interval;
    uint8_t spread_speed;
    bool meteor_active;
    bool hp_bar_filled;
    bool setup_active;
    bool entrance_active;
    bool entrance_waiting;
    bool defeating;
    bool finished;
} B10J;

static B10J m_B10J;

static const BOSS_DEFEAT_PARAMS m_Defeat = {
    .shift = 16,
    .step = 2,
    .whiteout_period = 7,
    .whitein_period = 5,
    .clears_stage = true,
};

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

static RESULT M_ValidateAssets(const B10J_ASSETS *const a)
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

static RESULT M_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const a, void *const data)
{
    B10J_ASSETS *const out = data;
    *out = (B10J_ASSETS) {};
    B10J_ASSETS loaded = {};
    MUST(BossAssets_LoadBos(vfs, "BOSS3_1.BOS", a, &loaded.still));
    MUST(BossAssets_LoadBos(vfs, "BOSS3_2.BOS", a, &loaded.animation));
    MUST(BossAssets_LoadBackground(vfs, "BOSS3.GRP", a, &loaded.background));
    MUST(BossAssets_LoadMissiles(vfs, a, &loaded.missiles));
    MUST(Scene_Load(vfs, 4, a, &loaded.next_scene));
    MUST(M_LoadPillars(vfs, &loaded.pillars));
    MUST(M_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

static void M_Init(void)
{
    memset(&m_B10J, 0, sizeof(m_B10J));
    m_B10J.meteor_active = true;
    m_B10J.spread_interval = 4;
    m_B10J.spread_speed = 8;
}

static void M_Setup(const void *const data, const BOSS_CONTEXT *const ctx)
{
    const B10J_ASSETS *const assets = data;
    Boss_Select(&m_B10J.status);
    ASSERT(
        !m_B10J.setup_active && !m_B10J.entrance_active && !m_B10J.defeating);
    ASSERT(Result_Ignore(M_ValidateAssets(assets)));
    ASSERT(Result_Ignore(BossEntity_Bind(&m_B10J.still, 0, &assets->still)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&m_B10J.animation, 1, &assets->animation)));
    Palette_SetGlobal(&assets->palette);
    m_B10J.post_defeat = g_Palette->saved;
    m_B10J.boss_palette = g_Palette->saved;
    m_B10J.still.image = 0;
    m_B10J.animation.image = 1;
    Palette_BeginGlobalFade(&m_B10J.setup_fade, PALETTE_FADE_WHITE_IN);
    VSyncIrq_BeginWait(&m_B10J.setup_wait);
    m_B10J.setup_active = true;
    m_B10J.finished = false;
}

static bool M_ResumeSetup(const void *const data, const BOSS_CONTEXT *const ctx)
{
    if (!m_B10J.setup_active) {
        return true;
    }
    if (!VSyncIrq_WaitReady(&m_B10J.setup_wait)) {
        return false;
    }
    Palette_GlobalFadeStep(&m_B10J.setup_fade);
    if (m_B10J.setup_fade.remaining != 0) {
        VSyncIrq_BeginWait(&m_B10J.setup_wait);
        return false;
    }
    BossEntity_SetPosition(
        &m_B10J.still, 256, 64, (BOSS_BOUNDS) { 0, 736, 64, 304 });
    BossEntity_SetOrbHitbox(&m_B10J.still, (BOSS_BOUNDS) { 16, 112, 16, 144 });
    m_B10J.still.orb_inactive = false;
    m_B10J.status.frame = 0;
    m_B10J.status.phase = B10J_PHASE_START;
    m_B10J.status.hp = 12;
    HudHp_Begin(2, 6);
    Particles_ResetStage();
    Missiles_ResetStage();
    Palette_SetStage(&g_Palette->saved);
    m_B10J.setup_active = false;
    return true;
}

static void M_Snap(const B10J_ASSETS *const assets, const PTN *const miko)
{
    Video_Read(
        1, (GFX_RECT) { m_B10J.still.left, m_B10J.still.top, 128, 160 },
        m_B10J.background, 128);
    Video_SelectDrawingPage(0);
}

static void M_Erase(const B10J_ASSETS *const assets, const PTN *const miko)
{
    const VIDEO_IMAGE bg = { .pixels = m_B10J.background,
                             .width = 128,
                             .height = 160,
                             .stride = 128,
                             .key = -1 };
    Video_Blit(
        &bg, (GFX_RECT) { 0, 0, 128, 160 }, m_B10J.still.left,
        m_B10J.still.top);
}

static void M_EraseBoth(const B10J_ASSETS *const assets, const PTN *const miko)
{
    Video_SelectDrawingPage(1);
    M_Erase(assets, miko);
    Video_SelectDrawingPage(0);
    M_Erase(assets, miko);
}

static void M_PutStill(const B10J_ASSETS *const assets, const PTN *const miko)
{
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        BossEntity_Put(
            &m_B10J.still, m_B10J.still.left, m_B10J.still.top,
            m_B10J.still.image);
    }
}

static void M_AnimationBoth(
    const B10J_ASSETS *const assets, const PTN *const miko, const int16_t image)
{
    m_B10J.animation.left = m_B10J.still.left;
    m_B10J.animation.top = Math16_Wrap(m_B10J.still.top + 48);
    m_B10J.animation.image = image;
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        BossEntity_Put(
            &m_B10J.animation, m_B10J.animation.left, m_B10J.animation.top,
            image);
    }
}

static void M_Cast(const B10J_ASSETS *const assets, const PTN *const miko)
{
    m_B10J.meteor_active = false;
    M_AnimationBoth(assets, miko, 0);
}

static void M_Activate(const B10J_ASSETS *const assets, const PTN *const miko)
{
    if (!m_B10J.meteor_active) {
        m_B10J.meteor_active = true;
        M_AnimationBoth(assets, miko, 1);
    }
}

static void M_Meteor(const B10J_ASSETS *const assets, const PTN *const miko)
{
    if (m_B10J.meteor_active && m_B10J.status.frame % 8 == 0) {
        m_B10J.animation.left = m_B10J.still.left;
        m_B10J.animation.top = Math16_Wrap(m_B10J.still.top + 48);
        m_B10J.animation.image = (int16_t)(1 + (m_B10J.status.frame / 8) % 4);
        BossEntity_RestorePut(
            &m_B10J.animation, m_B10J.animation.left, m_B10J.animation.top,
            m_B10J.animation.image);
    }
}

static void M_Spread(
    const B10J_ASSETS *const assets, const PTN *const miko, const int16_t left,
    const int16_t top)
{
    if (m_B10J.status.frame < 10) {
        return;
    }
    if (m_B10J.status.frame == 10) {
        m_B10J.still.left = left;
        m_B10J.still.top = top;
        M_Snap(assets, miko);
        return;
    }
    if (m_B10J.status.frame % m_B10J.spread_interval != 0) {
        return;
    }
    const int16_t bottom = Math16_Wrap(
        (m_B10J.spread_speed / m_B10J.spread_interval)
        * (m_B10J.status.frame - 10));
    const int16_t start = Math16_Wrap(80 - bottom);
    if (start < 0) {
        m_B10J.status.frame = 0;
        M_PutStill(assets, miko);
        return;
    }
    for (int16_t row = 0; row < bottom; row++) {
        BossEntity_RestorePutLine(
            &m_B10J.still, left, Math16_Wrap(top + start + row),
            m_B10J.still.image, row);
        BossEntity_RestorePutLine(
            &m_B10J.still, left, Math16_Wrap(top + 160 - start - row),
            m_B10J.still.image, (int16_t)(159 - row));
    }
}

static void M_BrokenTransition(
    const B10J_ASSETS *const assets, const PTN *const miko)
{
    if (m_B10J.status.frame < 10 || m_B10J.status.frame % 4 != 0) {
        return;
    }
    const int16_t half = Math16_Wrap((m_B10J.status.frame - 10) * 2);
    if (half >= 80) {
        m_B10J.status.frame = 0;
        return;
    }
    Sprite_Restore16(
        m_B10J.still.left, Math16_Wrap(m_B10J.still.top + half),
        m_B10J.still.sprites.width_bytes, 8);
    Sprite_Restore16(
        m_B10J.still.left, Math16_Wrap(m_B10J.still.top + 152 - half),
        m_B10J.still.sprites.width_bytes, 8);
}

static void M_Corners(
    int16_t x[4], int16_t y[4], const int16_t radius, const uint8_t angle)
{
    for (int32_t i = 0; i < 4; i++) {
        x[i] = Math16_PolarX(
            Math16_Wrap(m_B10J.still.left + 64), radius, angle + i * 64);
        y[i] = Math16_PolarY(
            Math16_Wrap(m_B10J.still.top + 80), radius, angle + i * 64);
    }
}

static void M_Loop(
    const B10J_ASSETS *const assets, const PTN *const miko, const int16_t x[4],
    const int16_t y[4], const bool restore)
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
    const B10J_ASSETS *const assets, const PTN *const miko, const int16_t x,
    const int16_t y, const uint8_t angle, const int16_t speed, const bool chase)
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

static void M_AimedStatic(
    const B10J_ASSETS *const assets, const PTN *const miko)
{
    B10J_SQUARE *const sq = &m_B10J.squares[0];
    if (m_B10J.status.frame < 100) {
        return;
    }
    if (m_B10J.status.frame == 100) {
        M_SquareInit(sq);
        m_B10J.pattern_state = Boss_Rank(64, 72, 80, 88);
        Sound_Effect(SOUND_EFFECT_TELEGRAPH);
    }
    int16_t x[4];
    int16_t y[4];
    if (m_B10J.status.frame % 8 == 0) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(assets, miko, x, y, true);
        sq->angle =
            (uint8_t)(sq->angle + (m_B10J.status.frame < 260 ? 12 : -12));
        if (sq->radius < 112) {
            sq->radius += 8;
        } else {
            for (int32_t i = 0; i < 4; i++) {
                const uint8_t angle = m_B10J.status.frame > 280
                    ? Math16_Angle(
                          Math16_Wrap(y[i] - m_B10J.still.top - 80),
                          Math16_Wrap(x[i] - m_B10J.still.left - 64))
                    : Math16_Angle(
                          Math16_Wrap(384 - y[i]),
                          Math16_Wrap(g_Player->left + 12 - x[i]));
                M_Add(
                    assets, miko, x[i], y[i], angle,
                    m_B10J.status.frame > 280
                        ? m_B10J.pattern_state
                        : (int16_t)(m_B10J.pattern_state / 2),
                    false);
                Sound_Effect(SOUND_EFFECT_PELLET);
            }
        }
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(assets, miko, x, y, false);
    }
    if (m_B10J.status.frame > 360) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(assets, miko, x, y, true);
        m_B10J.status.frame = 0;
    }
}

static void M_AimedMissiles(
    const B10J_ASSETS *const assets, const PTN *const miko)
{
    B10J_SQUARE *const sq = &m_B10J.squares[1];
    if (m_B10J.status.frame < 100) {
        return;
    }
    if (m_B10J.status.frame == 100) {
        M_SquareInit(sq);
        m_B10J.pattern_state = Boss_Rank(96, 104, 112, 118);
        Sound_Effect(SOUND_EFFECT_TELEGRAPH);
    }
    int16_t x[4];
    int16_t y[4];
    if (m_B10J.status.frame % 8 == 0) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(assets, miko, x, y, true);
        sq->angle -= 12;
        if (sq->radius < 80) {
            sq->radius += 8;
        } else if (m_B10J.status.frame == 224) {
            m_B10J.target_left = g_Player->left;
        } else if (m_B10J.status.frame > 240) {
            const MATH16_VECTOR velocity = Math16_Aim(
                (MATH16_VECTOR) { Math16_Wrap(m_B10J.still.left + 64),
                                  Math16_Wrap(m_B10J.still.top + 80) },
                (MATH16_VECTOR) { Math16_Wrap(m_B10J.target_left + 8), 384 },
                m_B10J.pattern_state, 0);
            for (int32_t i = 0; i < 4; i++) {
                Missiles_Add(
                    x[i], y[i], Math16_ToPixel(velocity.x),
                    Math16_ToPixel(velocity.y), 0);
            }
            Sound_Effect(SOUND_EFFECT_LASER);
        }
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(assets, miko, x, y, false);
    }
    if (m_B10J.status.frame > 320) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(assets, miko, x, y, true);
        m_B10J.status.frame = 0;
    }
}

static void M_TwoSquares(const B10J_ASSETS *const assets, const PTN *const miko)
{
    B10J_SQUARE *const sq = &m_B10J.squares[2];
    if (m_B10J.status.frame == 50) {
        M_Cast(assets, miko);
    }
    if (m_B10J.status.frame < 100) {
        return;
    }
    if (m_B10J.status.frame == 100) {
        M_SquareInit(sq);
        m_B10J.pattern_state = Boss_Rank(64, 72, 80, 88);
        Sound_Effect(SOUND_EFFECT_TELEGRAPH);
    }
    int16_t x[4];
    int16_t y[4];
    int16_t x2[4];
    int16_t y2[4];
    if (m_B10J.status.frame % 8 == 0) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Corners(x2, y2, sq->radius, (uint8_t)-sq->angle);
        M_Loop(assets, miko, x, y, true);
        M_Loop(assets, miko, x2, y2, true);
        sq->angle -= 6;
        Pellets_SpawnWithCloud(true);
        if (sq->radius < 112) {
            sq->radius += 8;
        } else {
            for (int32_t i = 0; i < 4; i++) {
                M_Add(
                    assets, miko, x[i], y[i],
                    Math16_Angle(
                        Math16_Wrap(y[i] - m_B10J.still.top - 80),
                        Math16_Wrap(x[i] - m_B10J.still.left - 64)),
                    m_B10J.pattern_state, false);
                M_Add(
                    assets, miko, x2[i], y2[i],
                    Math16_Angle(
                        Math16_Wrap(y2[i] - m_B10J.still.top - 80),
                        Math16_Wrap(x2[i] - m_B10J.still.left - 64)),
                    m_B10J.pattern_state, false);
                Sound_Effect(SOUND_EFFECT_PELLET);
            }
        }
        M_Corners(x, y, sq->radius, sq->angle);
        M_Corners(x2, y2, sq->radius, (uint8_t)-sq->angle);
        M_Loop(assets, miko, x, y, false);
        M_Loop(assets, miko, x2, y2, false);
        Pellets_SpawnWithCloud(false);
    }
    if (m_B10J.status.frame > 320) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Corners(x2, y2, sq->radius, (uint8_t)-sq->angle);
        M_Loop(assets, miko, x, y, true);
        M_Loop(assets, miko, x2, y2, true);
        m_B10J.status.frame = 0;
        M_Activate(assets, miko);
    }
}

static void M_InitHop(const B10J_ASSETS *const assets, const PTN *const miko)
{
    m_B10J.hop = 255;
    m_B10J.spread_interval = 4;
    m_B10J.direction = (uint8_t)(MasterRng_Next(&g_Rng) % 2);
    m_B10J.spread_speed = 16;
}

static void M_Hop(const B10J_ASSETS *const assets, const PTN *const miko)
{
    if (m_B10J.hop == 255) {
        M_EraseBoth(assets, miko);
        m_B10J.meteor_active = false;
        m_B10J.hop = 0;
        m_B10J.status.frame = 1;
    }
    if (m_B10J.hop == 0) {
        m_B10J.still.orb_inactive = true;
        M_BrokenTransition(assets, miko);
    } else {
        if (m_B10J.status.frame == 4) {
            m_B10J.still.left = m_B10J.direction == 0
                ? (int16_t)((m_B10J.hop - 1) * 128)
                : (int16_t)(640 - m_B10J.hop * 128);
            m_B10J.still.top = 96;
            M_Snap(assets, miko);
        }
        m_B10J.still.orb_inactive = m_B10J.status.frame < 12;
        M_Spread(assets, miko, m_B10J.still.left, m_B10J.still.top);
    }
    if (m_B10J.status.frame != 0) {
        return;
    }
    if (m_B10J.hop != 0) {
        if (m_B10J.hop != 4) {
            M_EraseBoth(assets, miko);
        }
        m_B10J.pattern_state = Boss_Rank(30, 35, 40, 45);
        const int16_t count = Boss_Rank(5, 8, 10, 12);
        uint8_t angle = 0;
        for (int16_t i = 0; i < count;
             i++, angle = (uint8_t)(angle + 256 / count)) {
            M_Add(
                assets, miko, Math16_Wrap(m_B10J.still.left + 64),
                Math16_Wrap(m_B10J.still.top + 80), angle, m_B10J.pattern_state,
                true);
        }
    }
    if (m_B10J.hop >= 4) {
        M_PutStill(assets, miko);
        Palette_SetGlobal(g_StagePalette);
        m_B10J.hop = 255;
        m_B10J.direction = (uint8_t)(1 - m_B10J.direction);
        m_B10J.meteor_active = true;
        m_B10J.status.frame = 0;
    } else {
        for (int32_t i = 0; i < 16; i++) {
            const int8_t *const color = g_Palette->saved.colors[i];
            Palette_SetGlobalColor(i, color[0] - 4, color[1] - 4, color[2] - 4);
        }
        m_B10J.hop++;
        m_B10J.status.frame = 1;
    }
}

static void M_Pillar(
    const B10J_ASSETS *const assets, const PTN *const miko, const int16_t left,
    const int16_t bottom, const int16_t height)
{
    const int32_t segments = height > 8 ? height - 8 : 0;
    uint16_t offset = (uint16_t)(bottom * 80 + left / 8 - (left % 8 < 0));
    for (int32_t row = 0; row < segments + 8; row++) {
        const int32_t part = row < segments ? 0 : 1;
        const int32_t index = 7 - ((row < segments ? row : row - segments) % 8);
        const uint8_t *const mask = assets->pillars.masks[part][index];
        for (int32_t byte = 0; byte < 4; byte++) {
            const uint32_t address = (offset + (uint32_t)byte) & 0x7fffu;
            if (address >= 32000) {
                continue;
            }
            for (int32_t bit = 0; bit < 8; bit++) {
                if ((mask[byte] & (0x80u >> bit)) != 0) {
                    g_Video->pages[g_Video->drawing_page][address * 8 + bit] =
                        10;
                }
            }
        }
        offset = (uint16_t)(offset - 80);
    }
}

static void M_Pillars(const B10J_ASSETS *const assets, const PTN *const miko)
{
    if (m_B10J.status.frame == 50) {
        M_Cast(assets, miko);
    }
    if (m_B10J.status.frame < 100) {
        return;
    }
    if (m_B10J.status.frame == 100) {
        for (int32_t i = 0; i < 8; i++) {
            m_B10J.pillar_time[i] = 128;
            m_B10J.pillar_bottom[i] = 400;
        }
        m_B10J.pillar_x[0] = (int16_t)(16 + MasterRng_Next(&g_Rng) % 48);
        m_B10J.pillar_x[1] = (int16_t)(624 - MasterRng_Next(&g_Rng) % 48);
        m_B10J.pillar_x[2] = (int16_t)(64 + MasterRng_Next(&g_Rng) % 224);
        m_B10J.pillar_x[3] = (int16_t)(336 + MasterRng_Next(&g_Rng) % 224);
        m_B10J.pillar_x[4] = (int16_t)(128 + MasterRng_Next(&g_Rng) % 160);
        m_B10J.pillar_x[5] = (int16_t)(336 + MasterRng_Next(&g_Rng) % 160);
        m_B10J.pillar_x[6] = (int16_t)(64 + MasterRng_Next(&g_Rng) % 224);
        m_B10J.pillar_x[7] = (int16_t)(336 + MasterRng_Next(&g_Rng) % 224);
        m_B10J.pattern_state = Boss_Rank(40, 45, 50, 55);
        Sound_Effect(SOUND_EFFECT_TELEGRAPH);
    }
    for (int32_t i = 0; i < 8; i++) {
        if (m_B10J.status.frame % 8 == i && m_B10J.status.frame > 100 + i * 20
            && m_B10J.pillar_time[i] > 32) {
            Shape_EllipseArcErase(
                m_B10J.pillar_x[i], m_B10J.pillar_bottom[i],
                m_B10J.pillar_time[i], m_B10J.pillar_time[i], 4, 0, 255);
            m_B10J.pillar_time[i] -= 8;
        }
    }
    if (m_B10J.status.frame <= 240 && (m_B10J.status.frame - 100) % 20 == 0) {
        Sound_Effect(SOUND_EFFECT_SPAWN);
    }
    if (m_B10J.status.frame == 240) {
        M_Activate(assets, miko);
    }
    for (int32_t i = 0; i < 8; i++) {
        if (m_B10J.status.frame % 8 != i
            || m_B10J.status.frame <= 100 + i * 20) {
            continue;
        }
        const int16_t x = m_B10J.pillar_x[i];
        const int16_t bottom = m_B10J.pillar_bottom[i];
        if (m_B10J.pillar_time[i] > 32) {
            Shape_EllipseArcPut(
                x, bottom, m_B10J.pillar_time[i], m_B10J.pillar_time[i], 7, 4,
                0, 255);
            Video_Line(320, 200, x, bottom, 1);
        } else if (m_B10J.pillar_time[i] > 16) {
            if (m_B10J.pillar_time[i] == 32) {
                Sound_Effect(SOUND_EFFECT_PELLET);
                Video_LineErase(320, 200, x, bottom);
            }
            M_Pillar(
                assets, miko, Math16_Wrap(x - 16), bottom,
                Math16_Wrap(16 * (34 - m_B10J.pillar_time[i])));
            if (!g_Player->invincible && g_Player->left > Math16_Wrap(x - 40)
                && g_Player->left < Math16_Wrap(x + 8)) {
                Player_Hit();
            }
            m_B10J.pillar_time[i] -= 8;
        } else if (m_B10J.pillar_time[i] != -999) {
            Pellets_AddGroup(
                g_Session.settings.difficulty,
                Math16_Wrap(m_B10J.still.left + 64),
                Math16_Wrap(m_B10J.still.top + 80),
                PELLET_GROUP_THREE_WIDE_AIMED, 36);
            Sprite_Restore16(Math16_Wrap(x - 16), 176, 32, 224);
            m_B10J.pillar_time[i] = -999;
        }
    }
    bool done = true;
    for (int32_t i = 0; i < 8; i++) {
        done = done && m_B10J.pillar_time[i] == -999;
    }
    if (done) {
        m_B10J.status.frame = 0;
    }
}

static void M_Halfcircle(const B10J_ASSETS *const assets, const PTN *const miko)
{
    B10J_SQUARE *const sq = &m_B10J.squares[3];
    if (m_B10J.status.frame < 100) {
        return;
    }
    if (m_B10J.status.frame == 100) {
        M_SquareInit(sq);
        m_B10J.missile_angle = 0;
        m_B10J.pattern_state = Boss_Rank(40, 45, 50, 55);
        Sound_Effect(SOUND_EFFECT_TELEGRAPH);
    }
    int16_t x[4];
    int16_t y[4];
    if (m_B10J.status.frame % 8 == 0) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(assets, miko, x, y, true);
        sq->angle -= 12;
        if (sq->radius < 80) {
            sq->radius += 8;
        } else if (m_B10J.status.frame > 180 && m_B10J.status.frame % 16 == 8) {
            const MATH16_VECTOR velocity = Math16_Vector(
                (int16_t)(m_B10J.pattern_state / 10), m_B10J.missile_angle);
            for (int32_t i = 0; i < 4; i++) {
                Missiles_Add(x[i], y[i], velocity.x, velocity.y, 0);
            }
            Sound_Effect(SOUND_EFFECT_LASER);
            m_B10J.missile_angle += 13;
        }
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(assets, miko, x, y, false);
    }
    if (m_B10J.status.frame > 340) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(assets, miko, x, y, true);
        m_B10J.status.frame = 0;
    }
}

static void M_Spray(const B10J_ASSETS *const assets, const PTN *const miko)
{
    B10J_SQUARE *const sq = &m_B10J.squares[4];
    if (m_B10J.status.frame < 100) {
        return;
    }
    if (m_B10J.status.frame == 100) {
        M_SquareInit(sq);
        m_B10J.pellet_angle = 128;
        m_B10J.pattern_state = Boss_Rank(32, 40, 48, 56);
        Sound_Effect(SOUND_EFFECT_TELEGRAPH);
    }
    int16_t x[4];
    int16_t y[4];
    if (m_B10J.status.frame % 8 == 0) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(assets, miko, x, y, true);
        sq->angle =
            (uint8_t)(sq->angle + (m_B10J.status.frame > 270 ? 12 : -12));
        if (sq->radius < 80) {
            sq->radius += 8;
        } else if (m_B10J.status.frame > 180) {
            m_B10J.pellet_angle =
                (uint8_t)(m_B10J.pellet_angle
                          + (m_B10J.status.frame > 270 ? 12 : -12));
            for (int32_t i = 0; i < 4; i++) {
                M_Add(
                    assets, miko, x[i], y[i], m_B10J.pellet_angle,
                    m_B10J.pattern_state, false);
            }
            Sound_Effect(SOUND_EFFECT_LASER);
        }
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(assets, miko, x, y, false);
    }
    if (m_B10J.status.frame > 370) {
        M_Corners(x, y, sq->radius, sq->angle);
        M_Loop(assets, miko, x, y, true);
        m_B10J.status.frame = 0;
    }
}

static void M_Lasers(const B10J_ASSETS *const assets, const PTN *const miko)
{
    B10J_SQUARE *const sq = &m_B10J.squares[5];
    if (m_B10J.status.frame == 50) {
        M_Cast(assets, miko);
    }
    if (m_B10J.status.frame < 100) {
        return;
    }
    if (m_B10J.status.frame == 100) {
        M_SquareInit(sq);
        m_B10J.pattern_state = Boss_Rank(50, 54, 58, 62);
        Sound_Effect(SOUND_EFFECT_TELEGRAPH);
    }
    if (m_B10J.status.frame % 8 == 0) {
        M_Corners(m_B10J.laser_x, m_B10J.laser_y, sq->radius, sq->angle);
        M_Loop(assets, miko, m_B10J.laser_x, m_B10J.laser_y, true);
        sq->angle += 3;
        if (sq->radius < 80) {
            sq->radius += 8;
        }
        M_Corners(m_B10J.laser_x, m_B10J.laser_y, sq->radius, sq->angle);
        M_Loop(assets, miko, m_B10J.laser_x, m_B10J.laser_y, false);
    }
    if (m_B10J.status.frame > 180 && m_B10J.status.frame < 300) {
        const int32_t i = m_B10J.status.frame % 4;
        ShootoutLasers_Spawn(
            i, m_B10J.laser_x[i], m_B10J.laser_y[i],
            Math16_Wrap(g_Player->left + 14), 400, m_B10J.pattern_state, 7, 20,
            4);
    }
    if (m_B10J.status.frame > 300) {
        M_Corners(m_B10J.laser_x, m_B10J.laser_y, sq->radius, sq->angle);
        M_Loop(assets, miko, m_B10J.laser_x, m_B10J.laser_y, true);
        m_B10J.status.frame = 0;
    }
}

static void M_Hit(const B10J_ASSETS *const assets, const PTN *const miko)
{
    const uint8_t flash[] = { 3, 9 };
    BossHit_UpdateAndRender(
        &m_B10J.hit, &m_B10J.status.hp, &m_B10J.boss_palette, flash, 2, 5000,
        BossEntity_HitTestOrb(&m_B10J.still),
        (BOSS_SHOT_HITBOX) { Math16_Wrap(m_B10J.still.left + 16),
                             Math16_Wrap(m_B10J.still.top + 48), 96, 48 });
}

static bool M_Entrance(const B10J_ASSETS *const assets, const PTN *const miko)
{
    for (;;) {
        if (m_B10J.entrance_waiting) {
            if (!VSyncIrq_DelayReady(2)) {
                return false;
            }
            m_B10J.entrance_waiting = false;
        }
        m_B10J.status.frame = Math16_Wrap(m_B10J.status.frame + 1);
        M_Spread(assets, miko, 256, 120);
        if (m_B10J.status.frame == 0) {
            m_B10J.status.phase = B10J_PHASE_FIRST;
            m_B10J.pattern = 0;
            m_B10J.hp_bar_filled = false;
            Palette_SetStage(&g_Palette->saved);
            m_B10J.boss_palette = g_Palette->saved;
            M_InitHop(assets, miko);
            m_B10J.entrance_active = false;
            return true;
        }
        m_B10J.entrance_waiting = true;
    }
}

static bool M_Death(const B10J_ASSETS *const assets, const PTN *const miko)
{
    if (!BossDefeat_Resume(
            &m_B10J.defeat, &m_B10J.post_defeat, miko, &g_Orb->top)) {
        return false;
    }
    Scene_Activate(&assets->next_scene);
    m_B10J.defeating = false;
    m_B10J.finished = true;
    return true;
}

static bool M_Update(const void *const data, const BOSS_CONTEXT *const ctx)
{
    const B10J_ASSETS *const assets = data;
    const PTN *const miko = ctx->miko;
    ASSERT(!m_B10J.setup_active);
    if (m_B10J.finished) {
        return true;
    }
    if (m_B10J.entrance_active) {
        return M_Entrance(assets, miko);
    }
    if (m_B10J.defeating) {
        return M_Death(assets, miko);
    }
    Missiles_Update((uint16_t)g_Session.frame_rand, &assets->missiles);
    Particles_Update(PARTICLES_TOP_RIGHT, 7);
    if (m_B10J.status.phase == B10J_PHASE_START) {
        Video_SelectDrawingPage(0);
        m_B10J.status.frame = 0;
        m_B10J.still.orb_inactive = false;
        m_B10J.spread_interval = 1;
        m_B10J.spread_speed = 2;
        m_B10J.meteor_active = true;
        m_B10J.entrance_active = true;
        return M_Entrance(assets, miko);
    }
    if (m_B10J.status.phase == B10J_PHASE_FIRST) {
        if (!m_B10J.hp_bar_filled) {
            m_B10J.hp_bar_filled =
                HudHp_Increment(m_B10J.status.hp, m_B10J.status.frame);
        }
        m_B10J.status.frame = Math16_Wrap(m_B10J.status.frame + 1);
        m_B10J.hit.frame = Math16_Wrap(m_B10J.hit.frame + 1);
        M_Meteor(assets, miko);
        switch (m_B10J.pattern) {
        case 0:
            M_AimedStatic(assets, miko);
            break;
        case 1:
            M_AimedMissiles(assets, miko);
            break;
        case 2:
            M_TwoSquares(assets, miko);
            break;
        case 3:
            M_Hop(assets, miko);
            break;
        default:
            break;
        }
        if (m_B10J.status.frame == 0) {
            m_B10J.pattern =
                m_B10J.pattern == 3 ? 0 : Math16_Wrap(m_B10J.pattern + 1);
            if (m_B10J.status.hp <= 6) {
                m_B10J.pattern = 99;
            }
        }
        M_Hit(assets, miko);
        if (m_B10J.pattern == 99 && !m_B10J.hit.invincible) {
            M_EraseBoth(assets, miko);
            m_B10J.spread_interval = 4;
            m_B10J.spread_speed = 8;
            m_B10J.still.left = 256;
            m_B10J.still.top = 120;
            M_Snap(assets, miko);
            m_B10J.status.phase = B10J_PHASE_SPREAD;
            m_B10J.status.frame = 0;
            m_B10J.hit.frame = 0;
            m_B10J.pattern = 0;
        }
    } else if (m_B10J.status.phase == B10J_PHASE_SPREAD) {
        m_B10J.status.frame = Math16_Wrap(m_B10J.status.frame + 1);
        m_B10J.hit.frame = Math16_Wrap(m_B10J.hit.frame + 1);
        M_Spread(assets, miko, 256, 120);
        if (m_B10J.status.frame == 0) {
            m_B10J.status.phase = B10J_PHASE_SECOND;
            m_B10J.status.frame = 0;
            m_B10J.hit.frame = 0;
            m_B10J.pattern = 0;
        }
    } else if (m_B10J.status.phase == B10J_PHASE_SECOND) {
        m_B10J.status.frame = Math16_Wrap(m_B10J.status.frame + 1);
        m_B10J.hit.frame = Math16_Wrap(m_B10J.hit.frame + 1);
        M_Meteor(assets, miko);
        switch (m_B10J.pattern) {
        case 0:
            M_Pillars(assets, miko);
            break;
        case 1:
            M_Halfcircle(assets, miko);
            break;
        case 2:
            M_Spray(assets, miko);
            break;
        case 3:
            M_Lasers(assets, miko);
            break;
        default:
            break;
        }
        if (m_B10J.status.frame == 0) {
            m_B10J.pattern =
                m_B10J.pattern == 3 ? 0 : Math16_Wrap(m_B10J.pattern + 1);
        }
        M_Hit(assets, miko);
        if (m_B10J.status.hp <= 0) {
            Video_SelectDrawingPage(1);
            M_Erase(assets, miko);
            Video_SelectDrawingPage(0);
            Sound_Fade();
            Pellets_EraseAndResetMoving();
            Missiles_ResetStage();
            ShootoutLasers_EraseAndReset(5);
            BossDefeat_Begin(&m_B10J.defeat, m_Defeat);
            m_B10J.defeating = true;
            return M_Death(assets, miko);
        }
    }
    return true;
}

static const PI_IMAGE *M_StageBackground(const void *const data)
{
    const B10J_ASSETS *const assets = data;
    return &assets->background;
}

const BOSS_STRATEGY g_BossMima = {
    .assets_size = sizeof(B10J_ASSETS),
    .load_assets = M_LoadAssets,
    .init = M_Init,
    .setup = M_Setup,
    .resume_setup = M_ResumeSetup,
    .update = M_Update,
    .background = M_StageBackground,
    .update_on_life_begin = true,
    .song = 8,
    .defeat = m_Defeat,
};
