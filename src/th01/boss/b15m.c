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
#include "th01/wait.h"
#include "th01/wave.h"

#include "core/base/assert.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/pi.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

typedef struct {
    BOS girl;
    BOS attack;
    BOS bat;
    GRC spheres;
    PTN missiles;
    PI_IMAGE background;
    PALETTE palette;
    SCENE_DATA next_scene;
    const uint8_t *star;
} B15M_ASSETS;

typedef struct {
    int16_t x[5];
    int16_t y[5];
} B15M_RIFTS;

typedef struct {
    uint8_t angle;
    int16_t frame;
} B15M_CIRCLE;

typedef enum {
    B15M_PHASE_ENTRANCE,
    B15M_PHASE_PATTERNS_1,
    B15M_PHASE_CENTER,
    B15M_PHASE_PATTERNS_2,
    B15M_PHASE_TRANSFORM,
    B15M_PHASE_FORMS,
} B15M_PHASE;

typedef enum {
    B15M_FORM_GIRL,
    B15M_FORM_BAT,
} B15M_FORM;

typedef struct {
    BOSS_ENTITY girl;
    BOSS_ENTITY attack;
    BOSS_ENTITY bat;
    BOSS_HIT hit;
    PALETTE boss_palette;
    PALETTE post_defeat;
    BOSS_DEFEAT defeat;
    uint8_t background[128 * 96];
    B15M_RIFTS stars;
    B15M_RIFTS missile_rifts;
    B15M_RIFTS random_rifts;
    B15M_RIFTS girl_rifts;
    B15M_RIFTS bat_rifts;
    int16_t sphere_x[10];
    int16_t sphere_y[10];
    B15M_CIRCLE circle;
    B15M_CIRCLE star_circle;
    B15M_CIRCLE safety_circle;
    B15M_CIRCLE aimed_circle;
    BOSS_STATE status;
    int16_t pattern;
    int16_t pattern_state;
    int16_t form;
    int16_t circle_radius;
    int16_t direction;
    int16_t phase3_pattern;
    int16_t bat_target_x;
    int16_t bat_target_y;
    int16_t bat_target_frames;
    int16_t bat_velocity_x;
    int16_t bat_velocity_y;
    int16_t safety_left;
    int16_t girl_pattern;
    int16_t girl_subphase;
    int16_t phase5_subphase;
    int16_t bat_pattern;
    int16_t trail_x[2][3];
    int16_t trail_y[2][3];
    int16_t entrance_tick;
    int16_t entrance_delay;
    WAIT wait;
    uint8_t entrance_stage;
    uint8_t entrance_angle;
    uint8_t transform_stage;
    int8_t call_phase;
    bool teleport_done;
    bool hp_bar_filled;
    bool entrance_active;
    bool entrance_waiting;
    bool trails_offscreen;
    bool transform_bat;
    bool defeating;
    bool finished;
} B15M;

typedef enum {
    B15M_TRANSFORM_RIFTS = 1,
    B15M_TRANSFORM_SWAPPED = 2,
} B15M_TRANSFORM;

static B15M m_B15M;

static const BOSS_DEFEAT_PARAMS m_Defeat = {
    .shift = 16,
    .step = 2,
    .whiteout_period = 7,
    .whitein_period = 5,
    .clears_stage = true,
};

static RESULT M_LoadGrc(
    VFS *const vfs, const ALLOCATOR *const a, GRC *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS5_GR.GRC", VFS_IN_MEMORY, &reader));
    const RESULT result = Grc_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadMissiles(
    VFS *const vfs, const ALLOCATOR *const a, PTN *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS3_M.PTN", VFS_IN_MEMORY, &reader));
    const RESULT result = Ptn_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadStar(
    VFS *const vfs, const ALLOCATOR *const a, const uint8_t **const out)
{
    uint8_t *shapes;
    size_t size;
    MUST(SpriteSheet_Load(vfs, "shape8x8.bmp", 8, 8, a, &shapes, &size));
    FAIL_IF(size != 7 * 8, "Invalid shape8x8.bmp size");
    *out = shapes + 8;
    return OK;
}

static RESULT M_ValidateAssets(const B15M_ASSETS *const a)
{
    FAIL_IF(
        a == nullptr || a->girl.pixels == nullptr || a->girl.coverage == nullptr
            || a->girl.width != 128 || a->girl.height != 96
            || a->girl.count != 6 || a->attack.pixels == nullptr
            || a->attack.coverage == nullptr || a->attack.width != 128
            || a->attack.height != 96 || a->attack.count != 3
            || a->bat.pixels == nullptr || a->bat.coverage == nullptr
            || a->bat.width != 48 || a->bat.height != 32 || a->bat.count != 3,
        "Invalid Elis sprite banks");
    FAIL_IF(
        a->spheres.coverage == nullptr || a->spheres.width != 32
            || a->spheres.height != 32 || a->spheres.count != 6
            || a->star == nullptr || a->background.pixels == nullptr
            || a->background.width != 640 || a->background.height != 400
            || !a->background.has_palette,
        "Invalid Elis effects or background");
    MUST(Missiles_ValidateSprites(&a->missiles));
    FAIL_IF(
        a->next_scene.id != 5 || a->next_scene.layouts.stages == nullptr,
        "Missing Elis next scene");
    return OK;
}

static RESULT M_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const a, void *const data)
{
    B15M_ASSETS *const out = data;
    *out = (B15M_ASSETS) {};
    B15M_ASSETS loaded = {};
    MUST(BossAssets_LoadBos(vfs, "BOSS5.BOS", a, &loaded.girl));
    MUST(BossAssets_LoadBos(vfs, "BOSS5_2.BOS", a, &loaded.attack));
    MUST(BossAssets_LoadBos(vfs, "BOSS5_3.BOS", a, &loaded.bat));
    MUST(M_LoadGrc(vfs, a, &loaded.spheres));
    MUST(M_LoadMissiles(vfs, a, &loaded.missiles));
    MUST(BossAssets_LoadBackground(vfs, "BOSS5.GRP", a, &loaded.background));
    MUST(Scene_Load(vfs, 5, a, &loaded.next_scene));
    MUST(M_LoadStar(vfs, a, &loaded.star));
    MUST(M_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

static void M_Init(void)
{
    memset(&m_B15M, 0, sizeof(m_B15M));
}

static void M_Setup(const void *const data, const BOSS_CONTEXT *const ctx)
{
    const B15M_ASSETS *const assets = data;
    Boss_Select(&m_B15M.status);
    ASSERT(
        !m_B15M.entrance_active && !m_B15M.defeating
        && Wait_Idle(&m_B15M.wait));
    ASSERT(Result_Ignore(M_ValidateAssets(assets)));
    ASSERT(Result_Ignore(BossEntity_Bind(&m_B15M.girl, 0, &assets->girl)));
    ASSERT(Result_Ignore(BossEntity_Bind(&m_B15M.attack, 1, &assets->attack)));
    ASSERT(Result_Ignore(BossEntity_Bind(&m_B15M.bat, 2, &assets->bat)));
    const BOSS_BOUNDS bounds = { 0, 736, 64, 304 };
    BossEntity_SetPosition(&m_B15M.girl, 256, 96, bounds);
    BossEntity_SetPosition(&m_B15M.attack, 256, 96, bounds);
    BossEntity_SetPosition(&m_B15M.bat, 256, 96, bounds);
    BossEntity_SetOrbHitbox(&m_B15M.girl, (BOSS_BOUNDS) { 16, 112, -4, 80 });
    BossEntity_SetOrbHitbox(&m_B15M.bat, (BOSS_BOUNDS) { -8, 56, -8, 40 });
    m_B15M.boss_palette = g_Palette->saved;
    m_B15M.post_defeat = (PALETTE) {};
    m_B15M.status.phase = B15M_PHASE_ENTRANCE;
    m_B15M.status.frame = 0;
    m_B15M.status.hp = 14;
    m_B15M.finished = false;
    HudHp_Begin(6, 10);
    Rng_Seed(g_Session->frame_rand);
    Pellets_Interlace();
    Particles_ResetStage();
}

static uint16_t M_Random(const B15M_ASSETS *const assets, const PTN *const miko)
{
    return MasterRng_Next(&g_Rng);
}
static int16_t M_CenterX(const int16_t form)
{
    return Math16_Wrap(form ? m_B15M.bat.left + 24 : m_B15M.girl.left + 64);
}
static int16_t M_CenterY(const int16_t form)
{
    return Math16_Wrap(form ? m_B15M.bat.top + 16 : m_B15M.girl.top + 48);
}
static void M_Sync(void)
{
    m_B15M.attack.left = m_B15M.girl.left;
    m_B15M.attack.top = m_B15M.girl.top;
}
static int16_t M_RandomLeft(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    return M_Random(assets, miko) % 512;
}
static int16_t M_RandomTop(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    return 64 + M_Random(assets, miko) % 136;
}
static void M_Snap(const B15M_ASSETS *const assets, const PTN *const miko)
{
    M_Sync();
    Video_Read(
        1, (GFX_RECT) { m_B15M.girl.left / 8 * 8, m_B15M.girl.top, 128, 96 },
        m_B15M.background, 128);
    Video_SelectDrawingPage(0);
}
static void M_Background(const B15M_ASSETS *const assets, const PTN *const miko)
{
    M_Sync();
    const VIDEO_IMAGE image = { .pixels = m_B15M.background,
                                .width = 128,
                                .height = 96,
                                .stride = 128,
                                .key = -1 };
    Video_Blit(
        &image, (GFX_RECT) { 0, 0, 128, 96 }, m_B15M.girl.left / 8 * 8,
        m_B15M.girl.top);
}
static void M_Unlock(
    BOSS_ENTITY *const e, const B15M_ASSETS *const assets,
    const PTN *const miko, const int16_t image, const bool restore)
{
    e->lock_frame = 0;
    e->image = image;
    BossEntity_LockedMovePut(e, 0, 0, 3, restore);
}
static void M_Both(
    const B15M_ASSETS *const assets, const PTN *const miko,
    BOSS_ENTITY *const e, const int16_t image, const bool erase_page0)
{
    Video_SelectDrawingPage(1);
    M_Background(assets, miko);
    M_Unlock(e, assets, miko, image, false);
    Video_SelectDrawingPage(0);
    if (erase_page0) {
        M_Background(assets, miko);
    }
    M_Unlock(e, assets, miko, image, false);
}
static void M_PutBoth(
    BOSS_ENTITY *const e, const B15M_ASSETS *const assets,
    const PTN *const miko, const int16_t image)
{
    Video_SelectDrawingPage(1);
    M_Unlock(e, assets, miko, image, false);
    Video_SelectDrawingPage(0);
    M_Unlock(e, assets, miko, image, false);
}
static void M_Attack(const B15M_ASSETS *const assets, const PTN *const miko)
{
    if (m_B15M.status.frame % 8 == 0) {
        M_Both(assets, miko, &m_B15M.attack, 1, true);
    } else if (m_B15M.status.frame % 8 == 4) {
        M_PutBoth(&m_B15M.attack, assets, miko, 2);
    }
}
static void M_Grc(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t left,
    const int16_t top, const int16_t image, const uint8_t color)
{
    if (left < -32 || left >= 640 || top < -32 || top >= 400) {
        return;
    }
    int16_t row = Math16_Wrap(top * 80 + left / 8 - (left % 8 < 0));
    const uint8_t *const pixels = assets->spheres.coverage + image * 1024;
    for (int32_t y = 0; y < 32; y++) {
        if (row >= 0) {
            const int32_t vy = row / 80;
            for (int32_t byte = 0; byte < 4; byte++) {
                const int16_t address = Math16_Wrap(row + byte);
                if ((left >= 0 && address / 80 == vy)
                    || (left < 0 && address / 80 != vy)) {
                    for (int32_t bit = 0; bit < 8; bit++) {
                        if (address >= 0 && address < 32000
                            && pixels[y * 32 + byte * 8 + bit] != 0) {
                            g_Video->pages[g_Video->drawing_page]
                                          [address * 8 + bit] = color & 15;
                        }
                    }
                }
            }
        }
        row = Math16_Wrap(row + 80);
        if (row >= 32000) {
            break;
        }
    }
}
static void M_Star8(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t left,
    const int16_t top)
{
    if (left < 0 || left >= 640 || top < 0 || top >= 400) {
        return;
    }
    for (int32_t y = 0; y < 8; y++) {
        for (int32_t x = 0; x < 8; x++) {
            const int32_t offset = (top + y) * 640 + left + x;
            if (offset < VIDEO_PAGE_SIZE
                && (assets->star[y] & (0x80u >> x)) != 0) {
                g_Video->pages[g_Video->drawing_page][offset] = 2;
            }
        }
    }
}
static void M_GrcErase(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t x,
    const int16_t y)
{
    Sprite_Restore16(x, y, 48, 32);
}
static void M_Circle(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t x,
    const int16_t y, const int16_t radius, const uint8_t color,
    const uint8_t step, const uint8_t start, const uint8_t end)
{
    Shape_EllipseArcPut(x, y, radius, radius, color, step, start, end);
}
static void M_EraseCircle(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t x,
    const int16_t y, const int16_t radius, const uint8_t step)
{
    Shape_EllipseArcErase(x, y, radius, radius, step, 0, 255);
}
static void M_BigCircle(
    const B15M_ASSETS *const assets, const PTN *const miko, const uint8_t color)
{
    M_Circle(
        assets, miko, M_CenterX(B15M_FORM_GIRL), M_CenterY(B15M_FORM_GIRL), 128,
        color, 1, 0, 255);
}
static void M_EraseBigCircle(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    M_EraseCircle(
        assets, miko, M_CenterX(B15M_FORM_GIRL), M_CenterY(B15M_FORM_GIRL), 128,
        1);
}
static void M_SummonDots(
    const B15M_ASSETS *const assets, const PTN *const miko,
    B15M_CIRCLE *const circle, const int16_t x, const int16_t y,
    const int16_t end)
{
    const uint8_t a = circle->angle;
    M_Circle(assets, miko, x, y, 128, 7, 1, a, (uint8_t)(a + end));
    M_Circle(
        assets, miko, x, y, 128, 7, 1, (uint8_t)(a + 64), (uint8_t)(a + 66));
    M_Circle(
        assets, miko, x, y, 128, 7, 1, (uint8_t)(a + 128), (uint8_t)(a + 130));
    M_Circle(
        assets, miko, x, y, 128, 7, 1, (uint8_t)(a + 192), (uint8_t)(a + 194));
    circle->angle = (uint8_t)(a + 2);
}
static bool M_SummonFlash(
    const B15M_ASSETS *const assets, const PTN *const miko,
    B15M_CIRCLE *const circle, const int16_t start, const int16_t end)
{
    if (m_B15M.status.frame >= start && m_B15M.status.frame % 2 == 0
        && circle->frame == 0) {
        if (m_B15M.status.frame == start) {
            Sound_Effect(SOUND_EFFECT_TELEGRAPH);
        }
        M_SummonDots(
            assets, miko, circle, M_CenterX(B15M_FORM_GIRL),
            M_CenterY(B15M_FORM_GIRL), end);
        if (circle->angle >= 64) {
            M_EraseBigCircle(assets, miko);
            M_BigCircle(assets, miko, 7);
            circle->frame = 1;
            M_Both(assets, miko, &m_B15M.girl, 0, true);
        }
    } else if (circle->frame != 0 && circle->frame < 40) {
        circle->frame = Math16_Wrap(circle->frame + 1);
        if (circle->frame % 8 == 0) {
            M_EraseBigCircle(assets, miko);
            M_BigCircle(assets, miko, 4);
        } else if (circle->frame % 8 == 4) {
            M_EraseBigCircle(assets, miko);
            M_BigCircle(assets, miko, 7);
        }
    } else if (circle->frame != 0) {
        return true;
    }
    return false;
}
static void M_Group(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t x,
    const int16_t y, const PELLET_GROUP group, const int16_t speed)
{
    Pellets_AddGroup(g_Session->settings.difficulty, x, y, group, speed);
}
static void M_FormGroup(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t form,
    const PELLET_GROUP group, const int16_t speed)
{
    M_Group(
        assets, miko, Math16_Wrap(M_CenterX(form) - 4),
        Math16_Wrap(M_CenterY(form) - 4), group, speed);
}
static void M_Single(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t x,
    const int16_t y, const uint8_t angle, const int16_t speed,
    const PELLET_MOTION motion, const int16_t ms)
{
    Pellets_AddSingle(
        g_Session->settings.difficulty, x, y, angle, speed, motion, ms, 0, 0);
}
static void M_Laser(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t slot,
    const int16_t x, const int16_t y, const int16_t tx, const int16_t ty,
    const int16_t speed, const int16_t age, const int16_t width)
{
    ShootoutLasers_Spawn(slot % 10, x, y, tx, ty, speed, 7, age, width);
}
static void M_Hit(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t form)
{
    const BOSS_ENTITY *const e =
        m_B15M.form == B15M_FORM_GIRL ? &m_B15M.girl : &m_B15M.bat;
    const uint8_t colors[] = { 3, 6, 8, 2 };
    const BOSS_SHOT_HITBOX box = form == B15M_FORM_GIRL
        ? (BOSS_SHOT_HITBOX) { Math16_Wrap(m_B15M.girl.left + 32),
                               Math16_Wrap(m_B15M.girl.top + 32), 64, 48 }
        : (BOSS_SHOT_HITBOX) { m_B15M.bat.left, m_B15M.bat.left, 32, 32 };
    BossHit_UpdateAndRender(
        &m_B15M.hit, &m_B15M.status.hp, &m_B15M.boss_palette, colors, 4, 7000,
        BossEntity_HitTestOrb(e), box);
}
static void M_Next(const int8_t phase, const int16_t pattern)
{
    m_B15M.status.phase = phase;
    m_B15M.status.frame = 0;
    m_B15M.hit.frame = 0;
    m_B15M.pattern = pattern;
    m_B15M.teleport_done = false;
}
static void M_Common(void)
{
    m_B15M.status.frame = Math16_Wrap(m_B15M.status.frame + 1);
    m_B15M.hit.frame = Math16_Wrap(m_B15M.hit.frame + 1);
    if (m_B15M.status.phase != B15M_PHASE_PATTERNS_1) {
        M_Sync();
    }
}
static int16_t M_SurroundX(
    const B15M_ASSETS *const assets, const PTN *const miko,
    const BOSS_ENTITY *const e, const bool bat)
{
    return Math16_Wrap(
        e->left + M_Random(assets, miko) % 192 - (bat ? 72 : 32));
}
static int16_t M_SurroundY(
    const B15M_ASSETS *const assets, const PTN *const miko,
    const BOSS_ENTITY *const e, const bool bat)
{
    return Math16_Wrap(e->top + M_Random(assets, miko) % 128 - (bat ? 48 : 32));
}
static void M_Rifts(
    const B15M_ASSETS *const assets, const PTN *const miko, B15M_RIFTS *const r,
    const bool bat, const int16_t start, const int16_t end,
    const bool random_color)
{
    if (m_B15M.status.frame < start || m_B15M.status.frame > end
        || m_B15M.status.frame % 4 != 0) {
        return;
    }
    BOSS_ENTITY *const e = bat ? &m_B15M.bat : &m_B15M.girl;
    for (int32_t i = 0; i < 5; i++) {
        if (m_B15M.status.frame > start) {
            M_GrcErase(assets, miko, r->x[i], r->y[i]);
        }
        if (m_B15M.status.frame % 16 == (i * 4) % 16
            || m_B15M.status.frame == start) {
            r->x[i] = M_SurroundX(assets, miko, e, bat);
            r->y[i] = M_SurroundY(assets, miko, e, bat);
        }
        if (m_B15M.status.frame < end) {
            const int16_t image = 4 + M_Random(assets, miko) % 2;
            const uint8_t color =
                random_color ? (uint8_t)(M_Random(assets, miko) % 16) : 4;
            M_Grc(assets, miko, r->x[i], r->y[i], image, color);
        }
    }
    Sound_Effect(SOUND_EFFECT_PELLET);
}
static bool M_Teleport(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t x,
    const int16_t y)
{
    M_Sync();
    switch (m_B15M.status.frame) {
    case 20:
        Video_SelectDrawingPage(1);
        M_Background(assets, miko);
        Video_SelectDrawingPage(0);
        M_Unlock(&m_B15M.girl, assets, miko, 2, false);
        m_B15M.girl.orb_inactive = true;
        break;
    case 28:
        M_Background(assets, miko);
        M_Unlock(&m_B15M.girl, assets, miko, 3, false);
        m_B15M.girl.orb_inactive = true;
        break;
    case 36:
        m_B15M.girl.orb_inactive = true;
        M_Background(assets, miko);
        M_Unlock(&m_B15M.girl, assets, miko, 4, false);
        break;
    case 44:
        m_B15M.girl.orb_inactive = true;
        Sprite_Restore16(m_B15M.girl.left, m_B15M.girl.top, 128, 96);
        break;
    case 52:
        m_B15M.girl.orb_inactive = true;
        m_B15M.girl.left = x;
        m_B15M.girl.top = y;
        M_Snap(assets, miko);
        M_Background(assets, miko);
        M_Unlock(&m_B15M.girl, assets, miko, 4, false);
        break;
    case 60:
        m_B15M.girl.orb_inactive = true;
        M_Background(assets, miko);
        M_Unlock(&m_B15M.girl, assets, miko, 3, false);
        break;
    case 68:
        m_B15M.girl.orb_inactive = true;
        M_Background(assets, miko);
        M_Unlock(&m_B15M.girl, assets, miko, 2, false);
        break;
    case 76:
        m_B15M.girl.orb_inactive = false;
        M_PutBoth(&m_B15M.girl, assets, miko, 0);
        break;
    default:
        if (m_B15M.status.frame > 80) {
            m_B15M.status.frame = 0;
            return true;
        }
        break;
    }
    if (m_B15M.status.frame % 4 != 0) {
        return false;
    }
    for (int32_t i = 0; i < 5; i++) {
        if (m_B15M.status.frame > 4) {
            Sprite_Restore16(m_B15M.stars.x[i], m_B15M.stars.y[i], 16, 8);
        }
        const int16_t tx = M_SurroundX(assets, miko, &m_B15M.girl, false),
                      ty = M_SurroundY(assets, miko, &m_B15M.girl, false);
        if (m_B15M.status.frame < 40 || m_B15M.status.frame > 52) {
            m_B15M.stars.x[i] = tx;
            m_B15M.stars.y[i] = ty;
        } else {
            m_B15M.stars.x[i] = Math16_Wrap(
                m_B15M.stars.x[i] + Math16_Wrap(tx - m_B15M.stars.x[i]) / 3);
            m_B15M.stars.y[i] = Math16_Wrap(
                m_B15M.stars.y[i] + Math16_Wrap(ty - m_B15M.stars.y[i]) / 3);
        }
        if (m_B15M.status.frame < 68) {
            M_Star8(assets, miko, m_B15M.stars.x[i], m_B15M.stars.y[i]);
        }
    }
    return false;
}
static void M_TrailCopy(const int32_t i, const int16_t x, const int16_t y)
{
    for (int32_t t = 2; t > 0; t--) {
        m_B15M.trail_x[i][t] = m_B15M.trail_x[i][t - 1];
        m_B15M.trail_y[i][t] = m_B15M.trail_y[i][t - 1];
    }
    m_B15M.trail_x[i][0] = x;
    m_B15M.trail_y[i][0] = y;
}
static void M_Trail(
    const B15M_ASSETS *const assets, const PTN *const miko, const int32_t i,
    const int16_t x, const int16_t y, const int16_t valid)
{
    if (valid > 1) {
        M_GrcErase(assets, miko, m_B15M.trail_x[i][0], m_B15M.trail_y[i][0]);
    }
    M_Grc(assets, miko, x, y, 3, 8);
    if (valid > 1) {
        if (valid > 2) {
            M_GrcErase(
                assets, miko, m_B15M.trail_x[i][1], m_B15M.trail_y[i][1]);
        }
        M_Grc(assets, miko, m_B15M.trail_x[i][0], m_B15M.trail_y[i][0], 2, 8);
    }
    if (valid > 2) {
        if (valid > 3) {
            M_GrcErase(
                assets, miko, m_B15M.trail_x[i][2], m_B15M.trail_y[i][2]);
        }
        M_Grc(assets, miko, m_B15M.trail_x[i][1], m_B15M.trail_y[i][1], 1, 8);
    }
}
static bool M_Entrance(const B15M_ASSETS *const assets, const PTN *const miko)
{
    if (!m_B15M.entrance_active) {
        m_B15M.entrance_active = true;
        m_B15M.entrance_stage = 0;
        m_B15M.entrance_angle = 0;
        m_B15M.hit.frame = 0;
        m_B15M.boss_palette = g_Palette->saved;
        M_Snap(assets, miko);
    }
    if (m_B15M.entrance_waiting) {
        if (!VSyncIrq_DelayReady((uint16_t)m_B15M.entrance_delay)) {
            return false;
        }
        m_B15M.entrance_waiting = false;
    }
    for (;;) {
        m_B15M.hit.frame = Math16_Wrap(m_B15M.hit.frame + 1);
        if (m_B15M.entrance_stage == 0) {
            for (int32_t i = 0; i < 2; i++) {
                const int16_t x =
                    Math16_PolarX(304, 96, m_B15M.entrance_angle + i * 128);
                const int16_t y =
                    Math16_PolarY(114, 96, m_B15M.entrance_angle + i * 128);
                M_Trail(assets, miko, i, x, y, m_B15M.hit.frame);
                M_TrailCopy(i, x, y);
            }
            if (m_B15M.hit.frame >= 120) {
                m_B15M.entrance_stage = 1;
                m_B15M.hit.frame = 0;
                m_B15M.trails_offscreen = false;
                continue;
            }
            m_B15M.entrance_angle = (uint8_t)(m_B15M.entrance_angle + 24);
            m_B15M.entrance_delay = (120 - m_B15M.hit.frame) / 12 + 1;
        } else {
            const int16_t f = m_B15M.hit.frame;
            if (!m_B15M.trails_offscreen && f % 4 == 0) {
                for (int32_t i = 0; i < 2; i++) {
                    const int16_t x = Math16_Wrap(
                                      m_B15M.trail_x[i][0] + 64 - i * 128),
                                  y = m_B15M.trail_y[i][0];
                    M_Trail(assets, miko, i, x, y, 4);
                    if (m_B15M.trail_x[0][2] > 640
                        && m_B15M.trail_x[1][2] < -32) {
                        m_B15M.trails_offscreen = true;
                    }
                    M_TrailCopy(i, x, y);
                }
            }
            if (f > 20 && f < 100) {
                if (f % 4 == 0) {
                    if (f != 24) {
                        Wave_Restore(
                            m_B15M.girl.left, m_B15M.girl.top, f - 2, 104 - f,
                            (f - 4) * 6, 144, 96);
                    }
                    BossEntity_WavePut(
                        &m_B15M.girl, m_B15M.girl.left, m_B15M.girl.top, 0,
                        f + 2, 100 - f, f * 6);
                }
            } else if (f == 100) {
                Wave_Restore(
                    m_B15M.girl.left, m_B15M.girl.top, 102, 0, 100, 144, 96);
                M_Unlock(&m_B15M.girl, assets, miko, 0, true);
            } else if (f == 128) {
                M_Background(assets, miko);
                BossEntity_WavePut(
                    &m_B15M.girl, m_B15M.girl.left, m_B15M.girl.top, 0, 3, 8,
                    64);
            } else if (f == 130) {
                Wave_Restore(
                    m_B15M.girl.left, m_B15M.girl.top, 3, 8, 64, 144, 96);
                M_Unlock(&m_B15M.girl, assets, miko, 0, true);
            } else if (f > 140) {
                m_B15M.status.frame = 0;
                m_B15M.hit.frame = 0;
                m_B15M.status.phase = B15M_PHASE_PATTERNS_1;
                m_B15M.girl.orb_inactive = false;
                M_PutBoth(&m_B15M.girl, assets, miko, m_B15M.girl.image);
                m_B15M.teleport_done = false;
                m_B15M.pattern = 1;
                m_B15M.hp_bar_filled = false;
                m_B15M.entrance_active = false;
                return true;
            }
            m_B15M.entrance_delay = 1;
        }
        if (!VSyncIrq_DelayReady((uint16_t)m_B15M.entrance_delay)) {
            m_B15M.entrance_waiting = true;
            return false;
        }
    }
}

static int16_t M_LasersAcross(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    const int16_t x = M_CenterX(B15M_FORM_GIRL),
                  y = Math16_Wrap(m_B15M.girl.top + 32);
    if (m_B15M.status.frame == 50) {
        m_B15M.direction = M_Random(assets, miko) % 2;
        M_Both(assets, miko, &m_B15M.girl, 1, false);
        m_B15M.pattern_state = Boss_Rank(50, 55, 60, 65);
    } else if (m_B15M.status.frame == 60) {
        M_Circle(assets, miko, x, y, 32, 4, 2, 0, 255);
        m_B15M.circle_radius = 32;
    } else if (
        m_B15M.status.frame > 60 && m_B15M.status.frame < 120
        && m_B15M.status.frame % 2 == 0) {
        M_EraseCircle(assets, miko, x, y, m_B15M.circle_radius, 2);
        m_B15M.circle_radius = Math16_Wrap(m_B15M.circle_radius + 8);
        M_Circle(
            assets, miko, x, y, m_B15M.circle_radius, 4,
            m_B15M.status.frame < 100       ? 2
                : m_B15M.status.frame < 110 ? 8
                                            : 32,
            0, 255);
    } else if (m_B15M.status.frame == 120) {
        M_EraseCircle(assets, miko, x, y, m_B15M.circle_radius, 32);
    } else if (m_B15M.status.frame == 150) {
        M_Both(assets, miko, &m_B15M.girl, 0, false);
    }
    if (m_B15M.status.frame >= 70 && m_B15M.status.frame % 10 == 0) {
        const int16_t tx = m_B15M.direction == 0
            ? Math16_Wrap((m_B15M.status.frame - 70) / 10 * 64)
            : Math16_Wrap(640 - (m_B15M.status.frame - 70) / 10 * 64);
        M_Laser(
            assets, miko, m_B15M.status.frame / 10,
            Math16_Wrap(m_B15M.girl.left + 60),
            Math16_Wrap(m_B15M.girl.top + 28), tx, 400, m_B15M.pattern_state,
            25, 4);
        Sound_Effect(SOUND_EFFECT_LASER);
        if ((m_B15M.direction == 0 && tx >= 640)
            || (m_B15M.direction == 1 && tx <= 0)) {
            m_B15M.status.frame = 0;
            return 0;
        }
    }
    return 1;
}
static int16_t M_RandomMissiles(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    if (m_B15M.status.frame == 50) {
        M_Both(assets, miko, &m_B15M.girl, 1, false);
        m_B15M.pattern_state = Boss_Rank(15, 21, 25, 29);
    }
    if (m_B15M.status.frame > 60 && m_B15M.status.frame % 3 == 0) {
        const int32_t i = M_Random(assets, miko) % 5;
        const uint8_t angle =
            (uint8_t)(M_Random(assets, miko) % m_B15M.pattern_state
                      - (m_B15M.pattern_state - 1) / 2 + 64);
        const MATH16_VECTOR v = Math16_Vector(7, angle);
        Missiles_Add(
            m_B15M.missile_rifts.x[i], m_B15M.missile_rifts.y[i], v.x, v.y, 0);
    }
    M_Rifts(assets, miko, &m_B15M.missile_rifts, false, 60, 160, false);
    if (m_B15M.status.frame > 170) {
        m_B15M.status.frame = 0;
        return 0;
    }
    return 2;
}
static int16_t M_CirclePellets(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    if (m_B15M.status.frame < 10) {
        m_B15M.circle.frame = 0;
    }
    if (m_B15M.status.frame == 50) {
        M_Both(assets, miko, &m_B15M.girl, 1, false);
        m_B15M.circle.angle = 0;
        m_B15M.pattern_state = Boss_Rank(
            PELLET_GROUP_SINGLE_AIMED, PELLET_GROUP_RANDOM_NARROW_AIMED,
            PELLET_GROUP_THREE_WIDE_AIMED, PELLET_GROUP_FIVE_NARROW_AIMED);
    }
    if (M_SummonFlash(assets, miko, &m_B15M.circle, 60, 0)) {
        M_EraseBigCircle(assets, miko);
        m_B15M.circle.angle = 0;
        for (int32_t i = 0; i < 32; i++) {
            const int16_t x = Math16_PolarX(
                Math16_Wrap(m_B15M.girl.left + 60), 128, m_B15M.circle.angle);
            const int16_t y = Math16_PolarY(
                M_CenterY(B15M_FORM_GIRL), 128, m_B15M.circle.angle);
            M_Group(assets, miko, x, y, (PELLET_GROUP)m_B15M.pattern_state, 4);
            m_B15M.circle.angle = (uint8_t)(m_B15M.circle.angle + 8);
        }
        m_B15M.status.frame = 0;
        m_B15M.circle.frame = 0;
        return 0;
    }
    return 3;
}
static int16_t M_Phase1(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t id)
{
    switch (id) {
    case 0:
        return M_Random(assets, miko) % 4;
    case 1:
        return M_LasersAcross(assets, miko);
    case 2:
        return M_RandomMissiles(assets, miko);
    case 3:
        return M_CirclePellets(assets, miko);
    default:
        return 0;
    }
}
static void M_StarLines(
    const B15M_ASSETS *const assets, const PTN *const miko, const bool erase,
    const uint8_t color)
{
    const uint8_t pairs[6][2] = { { 192, 22 }, { 192, 106 }, { 22, 106 },
                                  { 64, 150 }, { 64, 234 },  { 150, 234 } };
    for (int32_t i = 0; i < 6; i++) {
        const int16_t x0 =
            Math16_PolarX(M_CenterX(B15M_FORM_GIRL), 128, pairs[i][0]);
        const int16_t y0 =
            Math16_PolarY(M_CenterY(B15M_FORM_GIRL), 128, pairs[i][0]);
        const int16_t x1 =
            Math16_PolarX(M_CenterX(B15M_FORM_GIRL), 128, pairs[i][1]);
        const int16_t y1 =
            Math16_PolarY(M_CenterY(B15M_FORM_GIRL), 128, pairs[i][1]);
        if (erase) {
            Video_LineErase(x0, y0, x1, y1);
        } else {
            Video_Line(x0, y0, x1, y1, color);
        }
    }
}
static int16_t M_Star(const B15M_ASSETS *const assets, const PTN *const miko)
{
    B15M_CIRCLE *const circle = &m_B15M.star_circle;
    if (m_B15M.status.frame < 5) {
        circle->frame = 0;
        circle->angle = 0;
    }
    if (m_B15M.status.frame < 10) {
        return 0;
    }
    if (m_B15M.status.frame == 10) {
        M_Both(assets, miko, &m_B15M.attack, 0, true);
        circle->angle = 0;
        circle->frame = 0;
    }
    if (m_B15M.status.frame >= 10 && m_B15M.status.frame % 2 == 0
        && circle->frame == 0) {
        if (m_B15M.status.frame == 10) {
            Sound_Effect(SOUND_EFFECT_TELEGRAPH);
        }
        M_SummonDots(
            assets, miko, circle, M_CenterX(B15M_FORM_GIRL),
            M_CenterY(B15M_FORM_GIRL), 2);
        if (circle->angle >= 64) {
            circle->frame = 1;
            M_EraseBigCircle(assets, miko);
            M_BigCircle(assets, miko, 7);
        }
    } else if (circle->angle >= 64) {
        circle->frame = Math16_Wrap(circle->frame + 1);
        if (circle->frame == 10) {
            M_StarLines(assets, miko, false, 7);
        }
        if (circle->frame > 20 && circle->frame % 4 == 0) {
            M_StarLines(assets, miko, false, 4);
            M_BigCircle(assets, miko, 4);
        } else if (circle->frame > 20 && circle->frame % 4 == 2) {
            M_StarLines(assets, miko, false, 7);
            M_BigCircle(assets, miko, 7);
        }
        if (circle->frame > 60) {
            M_StarLines(assets, miko, true, 0);
            M_EraseBigCircle(assets, miko);
            m_B15M.status.frame = 0;
            circle->angle = 0;
            circle->frame = 0;
            return 1;
        }
    }
    return 0;
}
static int16_t M_CurvedRings(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    M_Attack(assets, miko);
    int16_t offset = -1;
    int16_t speed = 0;
    if (m_B15M.status.frame == 10) {
        m_B15M.pattern_state = Boss_Rank(14, 16, 18, 20);
        offset = 0;
        speed = 48;
    } else if (m_B15M.status.frame == 16) {
        offset = 2;
        speed = 54;
    } else if (m_B15M.status.frame == 24) {
        offset = 4;
        speed = 64;
    } else if (m_B15M.status.frame == 32) {
        offset = 6;
        speed = 72;
    } else if (m_B15M.status.frame == 40) {
        offset = 8;
        speed = 80;
    } else if (m_B15M.status.frame > 60) {
        m_B15M.status.frame = 0;
        Video_SelectDrawingPage(1);
        M_Background(assets, miko);
        return 0;
    }
    if (offset >= 0) {
        for (int32_t i = 0; i < m_B15M.pattern_state; i++) {
            M_Single(
                assets, miko, Math16_Wrap(M_CenterX(B15M_FORM_GIRL) - 4),
                Math16_Wrap(M_CenterY(B15M_FORM_GIRL) - 4),
                (uint8_t)((256 / m_B15M.pattern_state) * i + offset), speed,
                PELLET_REGULAR, 0);
        }
    }
    return 1;
}
static int16_t M_SphereClusters(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    M_Attack(assets, miko);
    if (m_B15M.status.frame <= 100 && m_B15M.status.frame % 10 == 0) {
        const int32_t n = m_B15M.status.frame / 10;
        const int16_t column =
            Math16_Wrap((m_B15M.girl.left < 256 ? 608 : 0) - m_B15M.girl.left)
            / 10;
        m_B15M.sphere_x[n - 1] =
            Math16_Wrap(n * column + M_CenterX(B15M_FORM_GIRL) - 16);
        const int16_t row =
            Math16_Wrap(M_Random(assets, miko) % 200 + 200 - m_B15M.girl.top)
            / 10;
        m_B15M.sphere_y[n - 1] =
            Math16_Wrap(n * row + M_CenterY(B15M_FORM_GIRL) - 16);
        for (int32_t i = 0; i < n; i++) {
            M_Grc(assets, miko, m_B15M.sphere_x[i], m_B15M.sphere_y[i], 3, 4);
        }
    } else if (
        m_B15M.status.frame > 100 && m_B15M.status.frame <= 230
        && m_B15M.status.frame % 10 == 0) {
        if (m_B15M.status.frame <= 200) {
            const int32_t n = (m_B15M.status.frame - 100) / 10;
            Sound_Effect(SOUND_EFFECT_PELLET);
            M_Grc(
                assets, miko, m_B15M.sphere_x[n - 1], m_B15M.sphere_y[n - 1], 4,
                4);
            for (int32_t i = n; i < 10; i++) {
                M_Grc(
                    assets, miko, m_B15M.sphere_x[i], m_B15M.sphere_y[i], 3, 4);
            }
            m_B15M.pattern_state = Boss_Rank(5, 7, 9, 13);
            for (int32_t i = 0; i < m_B15M.pattern_state; i++) {
                const uint8_t angle = M_Random(assets, miko) & 127;
                const int16_t speed = (M_Random(assets, miko) + 24) & 63;
                M_Single(
                    assets, miko, m_B15M.sphere_x[n - 1],
                    m_B15M.sphere_y[n - 1], angle, speed, PELLET_REGULAR, 0);
            }
        }
        if (m_B15M.status.frame > 130) {
            const int32_t i = (m_B15M.status.frame - 130) / 10 - 1;
            M_GrcErase(assets, miko, m_B15M.sphere_x[i], m_B15M.sphere_y[i]);
        }
    } else if (m_B15M.status.frame > 250) {
        m_B15M.status.frame = 0;
        return 0;
    }
    return 2;
}
static int16_t M_RandomRifts(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    M_Attack(assets, miko);
    if (m_B15M.status.frame == 30) {
        m_B15M.pattern_state = Boss_Rank(6, 3, 2, 2);
    }
    if (m_B15M.status.frame > 60 && m_B15M.status.frame < 120
        && m_B15M.status.frame % m_B15M.pattern_state == 0) {
        const int32_t i = M_Random(assets, miko) % 5;
        const uint8_t angle = (uint8_t)(M_Random(assets, miko) % 21 + 54);
        M_Single(
            assets, miko, m_B15M.random_rifts.x[i], m_B15M.random_rifts.y[i],
            angle, 96, PELLET_REGULAR, 0);
    } else if (m_B15M.status.frame >= 120 && m_B15M.status.frame < 200) {
        const int32_t i = M_Random(assets, miko) % 5;
        uint8_t angle = (uint8_t)(M_Random(assets, miko) % 21 + 54);
        const uint8_t offset = (uint8_t)((M_Random(assets, miko) & 1)
                                             ? m_B15M.status.frame - 120
                                             : -(m_B15M.status.frame - 120));
        angle = (uint8_t)(angle * (offset / 2));
        M_Single(
            assets, miko, m_B15M.random_rifts.x[i], m_B15M.random_rifts.y[i],
            angle, 96, PELLET_REGULAR, 0);
    }
    M_Rifts(assets, miko, &m_B15M.random_rifts, false, 60, 200, false);
    if (m_B15M.status.frame > 220) {
        m_B15M.status.frame = 0;
        return 0;
    }
    return 3;
}
static int16_t M_Phase3(
    const B15M_ASSETS *const assets, const PTN *const miko, const int16_t id)
{
    if (id == 99) {
        m_B15M.phase3_pattern = 0;
        return 0;
    }
    if (id == 0) {
        return M_Random(assets, miko) % 4;
    }
    if (id < 1 || id > 3) {
        return 0;
    }
    if (m_B15M.phase3_pattern == 0) {
        m_B15M.phase3_pattern = M_Star(assets, miko);
        return id;
    }
    m_B15M.phase3_pattern = id == 1 ? M_CurvedRings(assets, miko)
        : id == 2                   ? M_SphereClusters(assets, miko)
                                    : M_RandomRifts(assets, miko);
    return m_B15M.phase3_pattern;
}

static void M_TransformResult(void)
{
    if (m_B15M.call_phase == B15M_PHASE_TRANSFORM) {
        m_B15M.pattern = m_B15M.transform_bat ? 1 : 0;
    } else {
        m_B15M.form = m_B15M.transform_bat ? B15M_FORM_BAT : B15M_FORM_GIRL;
    }
}
static bool M_TransformFinish(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    if (m_B15M.status.frame <= 100) {
        if (m_B15M.call_phase == B15M_PHASE_TRANSFORM) {
            m_B15M.pattern = m_B15M.transform_bat ? 0 : 1;
        } else {
            m_B15M.form = m_B15M.transform_bat ? B15M_FORM_GIRL : B15M_FORM_BAT;
        }
        return true;
    }
    if (m_B15M.transform_bat) {
        m_B15M.bat.left = Math16_Wrap(m_B15M.girl.left + 40);
        m_B15M.bat.top = Math16_Wrap(m_B15M.girl.top + 32);
        m_B15M.bat.image = 0;
        Video_SelectDrawingPage(1);
        M_Background(assets, miko);
        Video_SelectDrawingPage(0);
        M_Background(assets, miko);
    } else {
        Sprite_Restore16(m_B15M.bat.left, m_B15M.bat.top, 48, 32);
        int16_t x = Math16_Wrap(m_B15M.bat.left - 40),
                y = Math16_Wrap(m_B15M.bat.top - 32);
        if (x < 0) {
            x = 0;
        } else if (x > 512) {
            x = 512;
        }
        if (y < 64) {
            y = 64;
        }
        m_B15M.girl.left = x;
        m_B15M.girl.top = y;
        M_Snap(assets, miko);
        M_Both(assets, miko, &m_B15M.attack, 1, true);
    }
    m_B15M.transform_stage = B15M_TRANSFORM_SWAPPED;
    return Wait_Scroll(&m_B15M.wait, 400);
}
static bool M_Transform(
    const B15M_ASSETS *const assets, const PTN *const miko, const bool bat)
{
    m_B15M.transform_bat = bat;
    m_B15M.transform_stage = B15M_TRANSFORM_RIFTS;
    if (bat && m_B15M.status.frame == 20) {
        M_Both(assets, miko, &m_B15M.attack, 0, true);
    }
    M_Rifts(
        assets, miko, bat ? &m_B15M.girl_rifts : &m_B15M.bat_rifts, !bat, 20,
        100, true);
    if (m_B15M.status.frame >= 20 && m_B15M.status.frame % 2 == 0) {
        return Wait_Scroll(
            &m_B15M.wait, Math16_Wrap(400 - m_B15M.status.frame % 4 * 2));
    }
    return M_TransformFinish(assets, miko);
}
static int16_t M_Abs(const int16_t value)
{
    return value < 0 ? Math16_Wrap(-(int32_t)value) : value;
}
static int16_t M_BatFly(const B15M_ASSETS *const assets, const PTN *const miko)
{
    if (m_B15M.status.frame < 3) {
        m_B15M.bat_velocity_x = 0;
        m_B15M.bat_velocity_y = 0;
        return 0;
    }
    if (m_B15M.status.frame == 3) {
        do {
            m_B15M.bat_target_x = M_RandomLeft(assets, miko);
            m_B15M.bat_target_y = Math16_Wrap(M_RandomTop(assets, miko) + 32);
        } while (M_Abs(Math16_Wrap(m_B15M.bat.left - m_B15M.bat_target_x)) < 128
                 && M_Abs(Math16_Wrap(m_B15M.bat.top - m_B15M.bat_target_y))
                     < 64);
        const MATH16_VECTOR v = Math16_Aim(
            (MATH16_VECTOR) { m_B15M.bat.left, m_B15M.bat.top },
            (MATH16_VECTOR) { m_B15M.bat_target_x, m_B15M.bat_target_y }, 2, 0);
        m_B15M.bat_velocity_x = v.x;
        m_B15M.bat_velocity_y = v.y;
        m_B15M.bat_target_frames =
            M_Abs(Math16_Wrap(m_B15M.bat_target_x - m_B15M.bat.left)) / 2;
    }
    m_B15M.bat_velocity_y = Math16_Wrap(
        m_B15M.bat_velocity_y + ((int32_t)M_Random(assets, miko) % 5 - 2) / 10);
    if (m_B15M.status.frame / 3 >= m_B15M.bat_target_frames) {
        m_B15M.bat_velocity_x = 0;
        m_B15M.bat_velocity_y = 0;
        m_B15M.status.frame = 0;
        return 1;
    }
    return 0;
}
static void M_BatPattern(const B15M_ASSETS *const assets, const PTN *const miko)
{
    switch (m_B15M.bat_pattern) {
    case 1:
        if (m_B15M.status.frame % 16 == 0) {
            M_FormGroup(
                assets, miko, B15M_FORM_BAT,
                (PELLET_GROUP)Boss_Rank(
                    PELLET_GROUP_THREE_WIDE_AIMED, PELLET_GROUP_FIVE_WIDE_AIMED,
                    PELLET_GROUP_FIVE_NARROW_AIMED, PELLET_GROUP_FIVE_NARROW),
                56);
        }
        break;
    case 2:
        if (m_B15M.status.frame % 4 == 0) {
            const PELLET_GROUP group = (M_Random(assets, miko) & 1)
                ? PELLET_GROUP_TWO_NARROW_AIMED
                : PELLET_GROUP_TWO_WIDE_AIMED;
            M_FormGroup(assets, miko, B15M_FORM_BAT, group, 88);
        }
        break;
    case 3:
        if (m_B15M.status.frame % 4 == 0) {
            const uint8_t angle = (uint8_t)M_Random(assets, miko);
            M_Single(
                assets, miko, Math16_Wrap(M_CenterX(B15M_FORM_BAT) - 4),
                Math16_Wrap(M_CenterY(B15M_FORM_BAT) - 4), angle, 2,
                PELLET_GRAVITY, 1);
        }
        break;
    case 4:
        if (m_B15M.status.frame % 50 < 10 && m_B15M.status.frame % 4 == 0) {
            M_FormGroup(assets, miko, 1, PELLET_GROUP_TWO_NARROW_AIMED, 85);
        }
        if (m_B15M.status.frame % 50 >= 25 && m_B15M.status.frame % 50 < 35
            && m_B15M.status.frame % 4 == 0) {
            M_FormGroup(assets, miko, 1, PELLET_GROUP_THREE_NARROW_AIMED, 85);
        }
        break;
    default:
        break;
    }
}
static void M_SymmetricFire(
    const B15M_ASSETS *const assets, const PTN *const miko, const uint8_t angle,
    const int16_t speed)
{
    const int16_t y = Math16_Wrap(M_CenterY(B15M_FORM_GIRL) - 8);
    M_Single(
        assets, miko, Math16_Wrap(M_CenterX(B15M_FORM_GIRL) - 8), y,
        (uint8_t)(128 - angle), speed, PELLET_REGULAR, 0);
    M_Single(
        assets, miko, M_CenterX(B15M_FORM_GIRL), y, angle, speed,
        PELLET_REGULAR, 0);
}
static int16_t M_SymmetricArc(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    M_Attack(assets, miko);
    if (m_B15M.status.frame == 50) {
        m_B15M.pattern_state = Boss_Rank(2, 3, 3, 4);
        for (int32_t i = 0; i < 4; i++) {
            M_SymmetricFire(
                assets, miko, 64, Math16_Wrap((i + m_B15M.pattern_state) * 16));
        }
    } else if (m_B15M.status.frame == 60 || m_B15M.status.frame == 70) {
        const uint8_t angle = m_B15M.status.frame == 60 ? 48 : 24;
        for (int32_t i = 0; i < 4; i++) {
            M_SymmetricFire(
                assets, miko, angle,
                Math16_Wrap((i + m_B15M.pattern_state) * 16));
        }
        Sound_Effect(SOUND_EFFECT_PELLET);
    } else if (m_B15M.status.frame == 80) {
        for (int32_t i = 0; i < 10; i++) {
            M_SymmetricFire(
                assets, miko, (uint8_t)(i * 6),
                Math16_Wrap((2 + m_B15M.pattern_state) * 16));
        }
        Sound_Effect(SOUND_EFFECT_PELLET);
    } else if (m_B15M.status.frame >= 120) {
        m_B15M.status.frame = 0;
        return 0;
    }
    return 1;
}
static void M_SafetyAfterHit(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    if (g_Player->is_hit) {
        m_B15M.safety_circle.frame = 200;
    }
    if (m_B15M.status.frame % m_B15M.pattern_state == 0) {
        const int16_t x = M_Random(assets, miko) % 632;
        M_Group(
            assets, miko, x, 64,
            g_Session->settings.difficulty == 3 ? PELLET_GROUP_RANDOM_WIDE
                                                : PELLET_GROUP_SINGLE,
            72);
    }
    if (m_B15M.safety_circle.frame > 180
        && m_B15M.safety_circle.frame % 4 == 0) {
        M_FormGroup(assets, miko, 0, PELLET_GROUP_SINGLE_AIMED, 72);
    }
}
static int16_t M_Safety(
    const B15M_ASSETS *const assets, const PTN *const miko,
    const uint64_t host_tick)
{
    B15M_CIRCLE *const circle = &m_B15M.safety_circle;
    if (m_B15M.status.frame < 10) {
        circle->frame = 0;
    }
    M_Attack(assets, miko);
    if (m_B15M.status.frame < 60) {
        return 1;
    }
    if (m_B15M.status.frame == 60) {
        m_B15M.pattern_state = Boss_Rank(4, 2, 2, 2);
        circle->angle = 0;
    }
    if (m_B15M.status.frame >= 60 && m_B15M.status.frame % 2 == 0
        && circle->frame == 0) {
        if (m_B15M.status.frame == 60) {
            Sound_Effect(SOUND_EFFECT_TELEGRAPH);
            m_B15M.safety_left = g_Player->left;
        }
        M_SummonDots(
            assets, miko, circle, Math16_Wrap(m_B15M.safety_left + 16), 384, 2);
        if (circle->angle >= 64) {
            M_EraseCircle(
                assets, miko, Math16_Wrap(m_B15M.safety_left + 16), 384, 128,
                1);
            M_Circle(
                assets, miko, Math16_Wrap(m_B15M.safety_left + 16), 384, 128, 7,
                1, 0, 255);
            circle->frame = 1;
            M_Both(assets, miko, &m_B15M.girl, 0, true);
        }
    } else if (circle->frame != 0 && circle->frame < 200) {
        circle->frame = Math16_Wrap(circle->frame + 1);
        if (circle->frame % 4 == 0) {
            M_EraseCircle(
                assets, miko, Math16_Wrap(m_B15M.safety_left + 16), 384, 128,
                1);
            M_Circle(
                assets, miko, Math16_Wrap(m_B15M.safety_left + 16), 384, 128, 7,
                1, 0, 255);
        }
        if (!g_Player->invincible
            && (Math16_Wrap(m_B15M.safety_left + 104) <= g_Player->left
                || Math16_Wrap(m_B15M.safety_left - 128) >= g_Player->left)) {
            Wait_Ticks(&m_B15M.wait, host_tick, 6);
            return -1;
        }
        M_SafetyAfterHit(assets, miko);
    } else if (circle->frame < 200) {
        return 1;
    } else {
        if (circle->frame == 200) {
            M_EraseCircle(
                assets, miko, Math16_Wrap(m_B15M.safety_left + 16), 384, 128,
                1);
        }
        circle->frame = Math16_Wrap(circle->frame + 1);
        if (circle->frame % 8 == 0) {
            M_Group(
                assets, miko, Math16_Wrap(m_B15M.bat.left + 60),
                Math16_Wrap(m_B15M.bat.top + 44), PELLET_GROUP_SINGLE_AIMED,
                72);
        }
        if (circle->frame > 260) {
            m_B15M.status.frame = 0;
            circle->frame = 0;
            circle->angle = 0;
            return 0;
        }
    }
    return 1;
}
static int16_t M_AimedRing(
    const B15M_ASSETS *const assets, const PTN *const miko)
{
    M_Attack(assets, miko);
    if (m_B15M.status.frame < 50) {
        return 1;
    }
    if (m_B15M.status.frame == 50) {
        M_Both(assets, miko, &m_B15M.girl, 1, false);
        m_B15M.aimed_circle.angle = 0;
        m_B15M.pattern_state = Boss_Rank(40, 30, 20, 15);
    }
    if (M_SummonFlash(assets, miko, &m_B15M.aimed_circle, 60, 2)) {
        M_EraseBigCircle(assets, miko);
        m_B15M.aimed_circle.angle = 0;
        for (int32_t i = 0; i < 32; i++) {
            const int16_t x = Math16_PolarX(
                M_CenterX(B15M_FORM_GIRL), 128, m_B15M.aimed_circle.angle);
            const int16_t y = Math16_PolarY(
                Math16_Wrap(m_B15M.girl.top + 28), 128,
                m_B15M.aimed_circle.angle);
            M_Group(assets, miko, x, y, PELLET_GROUP_SINGLE, 64);
            m_B15M.aimed_circle.angle =
                (uint8_t)(m_B15M.aimed_circle.angle + 8);
        }
        m_B15M.status.frame = 0;
        m_B15M.aimed_circle.frame = 0;
        return 0;
    }
    if (m_B15M.status.frame % m_B15M.pattern_state == 0) {
        M_Group(
            assets, miko, Math16_Wrap(M_CenterX(B15M_FORM_GIRL) - 42),
            Math16_Wrap(M_CenterY(B15M_FORM_GIRL) - 8),
            PELLET_GROUP_FIVE_WIDE_AIMED, 64);
        M_Group(
            assets, miko, Math16_Wrap(M_CenterX(B15M_FORM_GIRL) + 36),
            Math16_Wrap(M_CenterY(B15M_FORM_GIRL) - 8),
            PELLET_GROUP_FIVE_WIDE_AIMED, 64);
    }
    if (m_B15M.status.frame % (m_B15M.pattern_state * 2) == 0) {
        M_Laser(
            assets, miko, m_B15M.status.frame / 10,
            Math16_Wrap(M_CenterX(B15M_FORM_GIRL) - 4),
            Math16_Wrap(m_B15M.girl.top + 28), g_Player->left, 400, 50, 20, 8);
        Sound_Effect(SOUND_EFFECT_LASER);
    }
    return 1;
}
static int16_t M_GirlPhase5(
    const B15M_ASSETS *const assets, const PTN *const miko,
    const uint64_t host_tick)
{
    if (m_B15M.girl_pattern == 0) {
        m_B15M.girl_pattern = M_Random(assets, miko) % 3 + 1;
        return 0;
    }
    if (m_B15M.girl_subphase == 0) {
        m_B15M.girl_subphase = M_Star(assets, miko);
        return 0;
    }
    int16_t result = 1;
    switch (m_B15M.girl_pattern) {
    case 1:
        result = M_SymmetricArc(assets, miko);
        break;
    case 2:
        result = M_Safety(assets, miko, host_tick);
        break;
    case 3:
        result = M_AimedRing(assets, miko);
        break;
    default:
        break;
    }
    if (result < 0) {
        return -1;
    }
    m_B15M.girl_subphase = result;
    if (result == 0) {
        m_B15M.girl_pattern = 0;
        return 1;
    }
    return 0;
}
static void M_ResetPhase5(void)
{
    m_B15M.phase5_subphase = 0;
    m_B15M.bat_pattern = 0;
    m_B15M.girl_pattern = 0;
    m_B15M.girl_subphase = 0;
}
static void M_AfterTransform(void)
{
    if (m_B15M.call_phase == B15M_PHASE_TRANSFORM) {
        if (m_B15M.pattern == 1) {
            M_Next(B15M_PHASE_FORMS, 0);
            m_B15M.form = B15M_FORM_BAT;
            M_ResetPhase5();
        }
    } else if (m_B15M.phase5_subphase == 1) {
        if (m_B15M.transform_bat) {
            if (m_B15M.form == B15M_FORM_BAT) {
                m_B15M.phase5_subphase = 0;
            }
        } else if (m_B15M.form == B15M_FORM_GIRL) {
            m_B15M.bat_pattern = 0;
            m_B15M.phase5_subphase = 0;
        }
    }
}
static bool M_Phase5(
    const B15M_ASSETS *const assets, const PTN *const miko,
    const uint64_t host_tick)
{
    if (m_B15M.form == B15M_FORM_BAT) {
        if (m_B15M.phase5_subphase == 0) {
            m_B15M.phase5_subphase = M_BatFly(assets, miko);
            if (m_B15M.bat_pattern == 0) {
                m_B15M.bat_pattern = M_Random(assets, miko) % 4 + 1;
            }
            M_BatPattern(assets, miko);
        } else if (m_B15M.phase5_subphase == 1) {
            if (!M_Transform(assets, miko, false)) {
                return false;
            }
            M_AfterTransform();
        }
    } else {
        if (m_B15M.phase5_subphase == 0) {
            const int16_t result = M_GirlPhase5(assets, miko, host_tick);
            if (result < 0) {
                return false;
            }
            m_B15M.phase5_subphase = result;
        } else {
            if (!M_Transform(assets, miko, true)) {
                return false;
            }
            M_AfterTransform();
        }
    }
    return true;
}
static bool M_Death(const B15M_ASSETS *const assets, const PTN *const miko)
{
    if (!BossDefeat_Resume(
            &m_B15M.defeat, &m_B15M.post_defeat, miko, &g_Orb->top)) {
        return false;
    }
    Scene_Activate(&assets->next_scene);
    m_B15M.defeating = false;
    m_B15M.finished = true;
    return true;
}
static bool M_Tail(const B15M_ASSETS *const assets, const PTN *const miko)
{
    if (m_B15M.call_phase == B15M_PHASE_FORMS) {
        M_Hit(assets, miko, m_B15M.form);
        if (m_B15M.status.hp <= 0) {
            Sound_Fade();
            Pellets_RemoveAll();
            M_Background(assets, miko);
            Missiles_ResetStage();
            ShootoutLasers_EraseAndReset(10);
            BossDefeat_Begin(&m_B15M.defeat, m_Defeat);
            m_B15M.defeating = true;
            return M_Death(assets, miko);
        }
    }
    return true;
}
static bool M_Update(const void *const data, const BOSS_CONTEXT *const ctx)
{
    const B15M_ASSETS *const assets = data;
    const PTN *const miko = ctx->miko;
    const uint64_t host_tick = ctx->host_tick;
    if (m_B15M.finished) {
        return true;
    }
    if (m_B15M.defeating) {
        return M_Death(assets, miko);
    }
    if (!Wait_Idle(&m_B15M.wait)) {
        const WAIT_KIND kind = m_B15M.wait.kind;
        if (!Wait_Ready(&m_B15M.wait, host_tick)) {
            return false;
        }
        if (kind == WAIT_SCROLL) {
            if (m_B15M.transform_stage == B15M_TRANSFORM_RIFTS) {
                if (!M_TransformFinish(assets, miko)) {
                    return false;
                }
            } else {
                m_B15M.status.frame = 0;
                M_TransformResult();
            }
            M_AfterTransform();
            return M_Tail(assets, miko);
        }
        Player_Hit();
        M_SafetyAfterHit(assets, miko);
        m_B15M.girl_subphase = 1;
        m_B15M.phase5_subphase = 0;
        return M_Tail(assets, miko);
    }
    if (m_B15M.entrance_active) {
        return M_Entrance(assets, miko);
    }
    m_B15M.call_phase = m_B15M.status.phase;
    Missiles_Update((uint16_t)g_Session->frame_rand, &assets->missiles);
    if (m_B15M.status.phase == B15M_PHASE_ENTRANCE) {
        return M_Entrance(assets, miko);
    }
    if (m_B15M.status.phase == B15M_PHASE_PATTERNS_1
        || m_B15M.status.phase == B15M_PHASE_PATTERNS_2) {
        if (m_B15M.status.phase == B15M_PHASE_PATTERNS_1
            && !m_B15M.hp_bar_filled) {
            m_B15M.hp_bar_filled =
                HudHp_Increment(m_B15M.status.hp, m_B15M.status.frame);
        }
        M_Common();
        if (m_B15M.pattern != 0) {
            m_B15M.pattern = m_B15M.status.phase == B15M_PHASE_PATTERNS_1
                ? M_Phase1(assets, miko, m_B15M.pattern)
                : M_Phase3(assets, miko, m_B15M.pattern);
        } else if (!m_B15M.teleport_done) {
            const int16_t top = M_RandomTop(assets, miko);
            const int16_t left = M_RandomLeft(assets, miko);
            m_B15M.teleport_done = M_Teleport(assets, miko, left, top);
        } else {
            m_B15M.pattern = m_B15M.status.phase == B15M_PHASE_PATTERNS_1
                ? M_Phase1(assets, miko, 0)
                : M_Phase3(assets, miko, 0);
            m_B15M.teleport_done = false;
        }
        M_Hit(assets, miko, B15M_FORM_GIRL);
        if (!m_B15M.hit.invincible
            && m_B15M.status.hp
                <= (m_B15M.status.phase == B15M_PHASE_PATTERNS_1 ? 10 : 6)
            && m_B15M.teleport_done) {
            if (m_B15M.status.phase == B15M_PHASE_PATTERNS_1) {
                M_Next(B15M_PHASE_CENTER, 0);
            } else {
                M_Next(B15M_PHASE_TRANSFORM, 0);
                m_B15M.form = B15M_FORM_GIRL;
            }
        }
    } else if (m_B15M.status.phase == B15M_PHASE_CENTER) {
        m_B15M.status.frame = Math16_Wrap(m_B15M.status.frame + 1);
        m_B15M.teleport_done = M_Teleport(assets, miko, 256, 96);
        if (m_B15M.teleport_done) {
            m_B15M.phase3_pattern = 0;
            M_Next(B15M_PHASE_PATTERNS_2, 1);
        }
    } else if (m_B15M.status.phase == B15M_PHASE_TRANSFORM) {
        m_B15M.status.frame = Math16_Wrap(m_B15M.status.frame + 1);
        if (!m_B15M.teleport_done) {
            m_B15M.teleport_done = M_Teleport(assets, miko, 256, 96);
        } else {
            if (!M_Transform(assets, miko, true)) {
                return false;
            }
        }
        M_AfterTransform();
    } else if (m_B15M.status.phase == B15M_PHASE_FORMS) {
        M_Common();
        if (m_B15M.form != B15M_FORM_GIRL) {
            BossEntity_LockedMovePut(
                &m_B15M.bat, m_B15M.bat_velocity_x, m_B15M.bat_velocity_y, 2,
                true);
            if (m_B15M.status.frame % 9 == 0) {
                m_B15M.bat.image = 0;
            } else if (m_B15M.status.frame % 9 == 3) {
                m_B15M.bat.image = 1;
            } else if (m_B15M.status.frame % 9 == 6) {
                m_B15M.bat.image = 2;
            }
        }
        if (!M_Phase5(assets, miko, host_tick)) {
            return false;
        }
    }
    return M_Tail(assets, miko);
}

static const PI_IMAGE *M_StageBackground(const void *const data)
{
    const B15M_ASSETS *const assets = data;
    return &assets->background;
}

const BOSS_STRATEGY g_BossElis = {
    .assets_size = sizeof(B15M_ASSETS),
    .load_assets = M_LoadAssets,
    .init = M_Init,
    .setup = M_Setup,
    .update = M_Update,
    .background = M_StageBackground,
    .song = 10,
    .defeat = m_Defeat,
};
