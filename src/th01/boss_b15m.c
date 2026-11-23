#include "th01/boss_b15m.h"

#include "th01/boss.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/assert.h"
#include "th01/shape.h"
#include "th01/sprite.h"
#include "th01/sprite_sheet.h"
#include "th01/wave.h"

#include <string.h>

B15M g_B15M;

typedef enum {
    B15M_TRANSFORM_RIFTS = 1,
    B15M_TRANSFORM_SWAPPED = 2,
} B15M_TRANSFORM;

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

RESULT B15M_ValidateAssets(const B15M_ASSETS *const a)
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

RESULT B15M_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const a, B15M_ASSETS *const out)
{
    *out = (B15M_ASSETS) {};
    B15M_ASSETS loaded = {};
    MUST(Boss_LoadBos(vfs, "BOSS5.BOS", a, &loaded.girl));
    MUST(Boss_LoadBos(vfs, "BOSS5_2.BOS", a, &loaded.attack));
    MUST(Boss_LoadBos(vfs, "BOSS5_3.BOS", a, &loaded.bat));
    MUST(M_LoadGrc(vfs, a, &loaded.spheres));
    MUST(M_LoadMissiles(vfs, a, &loaded.missiles));
    MUST(Boss_LoadBackground(vfs, "BOSS5.GRP", a, &loaded.background));
    MUST(Scene_Load(vfs, 5, a, &loaded.next_scene));
    MUST(M_LoadStar(vfs, a, &loaded.star));
    MUST(B15M_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B15M_Init(void)
{
    memset(&g_B15M, 0, sizeof(g_B15M));
}

void B15M_Setup(const B15M_CONTEXT *const c)
{
    ASSERT(
        !g_B15M.entrance_active && !g_B15M.defeating
        && g_B15M.pending == B15M_PENDING_NONE);
    ASSERT(Result_Ignore(B15M_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(BossEntity_Bind(&g_B15M.girl, 0, &c->assets->girl)));
    ASSERT(
        Result_Ignore(BossEntity_Bind(&g_B15M.attack, 1, &c->assets->attack)));
    ASSERT(Result_Ignore(BossEntity_Bind(&g_B15M.bat, 2, &c->assets->bat)));
    const BOSS_BOUNDS bounds = { 0, 736, 64, 304 };
    BossEntity_SetPosition(&g_B15M.girl, 256, 96, bounds);
    BossEntity_SetPosition(&g_B15M.attack, 256, 96, bounds);
    BossEntity_SetPosition(&g_B15M.bat, 256, 96, bounds);
    BossEntity_SetOrbHitbox(&g_B15M.girl, (BOSS_BOUNDS) { 16, 112, -4, 80 });
    BossEntity_SetOrbHitbox(&g_B15M.bat, (BOSS_BOUNDS) { -8, 56, -8, 40 });
    g_B15M.boss_palette = c->hit->palette->saved;
    g_B15M.post_defeat = (PALETTE) {};
    g_B15M.phase = B15M_PHASE_ENTRANCE;
    g_B15M.frame = 0;
    g_B15M.hp = 14;
    g_B15M.finished = false;
    c->hit->health->first_white = 10;
    c->hit->health->first_redwhite = 6;
    *c->rng = c->hit->frame_rand;
    c->pellets->interlace = true;
    Particles_Init(c->particles);
}

static uint16_t M_Random(const B15M_CONTEXT *const c)
{
    return MasterRng_Next(c->rng);
}
static int16_t M_CenterX(const int16_t form)
{
    return Math16_Wrap(form ? g_B15M.bat.left + 24 : g_B15M.girl.left + 64);
}
static int16_t M_CenterY(const int16_t form)
{
    return Math16_Wrap(form ? g_B15M.bat.top + 16 : g_B15M.girl.top + 48);
}
static void M_Sync(void)
{
    g_B15M.attack.left = g_B15M.girl.left;
    g_B15M.attack.top = g_B15M.girl.top;
}
static int16_t M_RandomLeft(const B15M_CONTEXT *const c)
{
    return M_Random(c) % 512;
}
static int16_t M_RandomTop(const B15M_CONTEXT *const c)
{
    return 64 + M_Random(c) % 136;
}
static void M_Snap(const B15M_CONTEXT *const c)
{
    M_Sync();
    Video_Read(
        1, (GFX_RECT) { g_B15M.girl.left / 8 * 8, g_B15M.girl.top, 128, 96 },
        g_B15M.background, 128);
    Video_SelectDrawingPage(0);
}
static void M_Background(const B15M_CONTEXT *const c)
{
    M_Sync();
    const VIDEO_IMAGE image = { .pixels = g_B15M.background,
                                .width = 128,
                                .height = 96,
                                .stride = 128,
                                .key = -1 };
    Video_Blit(
        &image, (GFX_RECT) { 0, 0, 128, 96 }, g_B15M.girl.left / 8 * 8,
        g_B15M.girl.top);
}
static void M_Unlock(
    BOSS_ENTITY *const e, const B15M_CONTEXT *const c, const int16_t image,
    const bool restore)
{
    e->lock_frame = 0;
    e->image = image;
    BossEntity_LockedMovePut(e, 0, 0, 3, restore);
}
static void M_Both(
    const B15M_CONTEXT *const c, BOSS_ENTITY *const e, const int16_t image,
    const bool erase_page0)
{
    Video_SelectDrawingPage(1);
    M_Background(c);
    M_Unlock(e, c, image, false);
    Video_SelectDrawingPage(0);
    if (erase_page0) {
        M_Background(c);
    }
    M_Unlock(e, c, image, false);
}
static void M_PutBoth(
    BOSS_ENTITY *const e, const B15M_CONTEXT *const c, const int16_t image)
{
    Video_SelectDrawingPage(1);
    M_Unlock(e, c, image, false);
    Video_SelectDrawingPage(0);
    M_Unlock(e, c, image, false);
}
static void M_Attack(const B15M_CONTEXT *const c)
{
    if (g_B15M.frame % 8 == 0) {
        M_Both(c, &g_B15M.attack, 1, true);
    } else if (g_B15M.frame % 8 == 4) {
        M_PutBoth(&g_B15M.attack, c, 2);
    }
}
static void M_Grc(
    const B15M_CONTEXT *const c, const int16_t left, const int16_t top,
    const int16_t image, const uint8_t color)
{
    if (left < -32 || left >= 640 || top < -32 || top >= 400) {
        return;
    }
    int16_t row = Math16_Wrap(top * 80 + left / 8 - (left % 8 < 0));
    const uint8_t *const pixels = c->assets->spheres.coverage + image * 1024;
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
                            g_Video.pages[g_Video.drawing_page]
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
    const B15M_CONTEXT *const c, const int16_t left, const int16_t top)
{
    if (left < 0 || left >= 640 || top < 0 || top >= 400) {
        return;
    }
    for (int32_t y = 0; y < 8; y++) {
        for (int32_t x = 0; x < 8; x++) {
            const int32_t offset = (top + y) * 640 + left + x;
            if (offset < VIDEO_PAGE_SIZE
                && (c->assets->star[y] & (0x80u >> x)) != 0) {
                g_Video.pages[g_Video.drawing_page][offset] = 2;
            }
        }
    }
}
static void M_GrcErase(
    const B15M_CONTEXT *const c, const int16_t x, const int16_t y)
{
    Sprite_Restore16(x, y, 48, 32);
}
static void M_Circle(
    const B15M_CONTEXT *const c, const int16_t x, const int16_t y,
    const int16_t radius, const uint8_t color, const uint8_t step,
    const uint8_t start, const uint8_t end)
{
    Shape_EllipseArcPut(x, y, radius, radius, color, step, start, end);
}
static void M_EraseCircle(
    const B15M_CONTEXT *const c, const int16_t x, const int16_t y,
    const int16_t radius, const uint8_t step)
{
    Shape_EllipseArcErase(x, y, radius, radius, step, 0, 255);
}
static void M_BigCircle(const B15M_CONTEXT *const c, const uint8_t color)
{
    M_Circle(
        c, M_CenterX(B15M_FORM_GIRL), M_CenterY(B15M_FORM_GIRL), 128, color, 1,
        0, 255);
}
static void M_EraseBigCircle(const B15M_CONTEXT *const c)
{
    M_EraseCircle(
        c, M_CenterX(B15M_FORM_GIRL), M_CenterY(B15M_FORM_GIRL), 128, 1);
}
static void M_SummonDots(
    const B15M_CONTEXT *const c, B15M_CIRCLE *const circle, const int16_t x,
    const int16_t y, const int16_t end)
{
    const uint8_t a = circle->angle;
    M_Circle(c, x, y, 128, 7, 1, a, (uint8_t)(a + end));
    M_Circle(c, x, y, 128, 7, 1, (uint8_t)(a + 64), (uint8_t)(a + 66));
    M_Circle(c, x, y, 128, 7, 1, (uint8_t)(a + 128), (uint8_t)(a + 130));
    M_Circle(c, x, y, 128, 7, 1, (uint8_t)(a + 192), (uint8_t)(a + 194));
    circle->angle = (uint8_t)(a + 2);
}
static bool M_SummonFlash(
    const B15M_CONTEXT *const c, B15M_CIRCLE *const circle, const int16_t start,
    const int16_t end)
{
    if (g_B15M.frame >= start && g_B15M.frame % 2 == 0 && circle->frame == 0) {
        if (g_B15M.frame == start) {
            Boss_Effect(c->hit, 8);
        }
        M_SummonDots(
            c, circle, M_CenterX(B15M_FORM_GIRL), M_CenterY(B15M_FORM_GIRL),
            end);
        if (circle->angle >= 64) {
            M_EraseBigCircle(c);
            M_BigCircle(c, 7);
            circle->frame = 1;
            M_Both(c, &g_B15M.girl, 0, true);
        }
    } else if (circle->frame != 0 && circle->frame < 40) {
        circle->frame = Math16_Wrap(circle->frame + 1);
        if (circle->frame % 8 == 0) {
            M_EraseBigCircle(c);
            M_BigCircle(c, 4);
        } else if (circle->frame % 8 == 4) {
            M_EraseBigCircle(c);
            M_BigCircle(c, 7);
        }
    } else if (circle->frame != 0) {
        return true;
    }
    return false;
}
static void M_Group(
    const B15M_CONTEXT *const c, const int16_t x, const int16_t y,
    const PELLET_GROUP group, const int16_t speed)
{
    Pellets_AddGroup(
        c->pellets, c->pellet_shared, Boss_Player(c->hit), c->rng,
        c->hit->display->rank, x, y, group, speed);
}
static void M_FormGroup(
    const B15M_CONTEXT *const c, const int16_t form, const PELLET_GROUP group,
    const int16_t speed)
{
    M_Group(
        c, Math16_Wrap(M_CenterX(form) - 4), Math16_Wrap(M_CenterY(form) - 4),
        group, speed);
}
static void M_Single(
    const B15M_CONTEXT *const c, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed, const PELLET_MOTION motion,
    const int16_t ms)
{
    Pellets_AddSingle(
        c->pellets, c->pellet_shared, c->hit->display->rank, x, y, angle, speed,
        motion, ms, 0, 0);
}
static void M_Laser(
    const B15M_CONTEXT *const c, const int16_t slot, const int16_t x,
    const int16_t y, const int16_t tx, const int16_t ty, const int16_t speed,
    const int16_t age, const int16_t width)
{
    ShootoutLasers_Spawn(
        &c->lasers->slots[slot % 10], x, y, tx, ty, speed, 7, age, width);
}
static void M_MissileAction(void *const user, const MISSILES_ACTION action)
{
    const B15M_CONTEXT *const c = user;
    if (action.kind == MISSILES_ACTION_EFFECT) {
        Boss_Effect(c->hit, action.effect);
    } else {
        Missiles_RenderAction(&c->assets->missiles, action);
    }
}
static void M_Hit(const B15M_CONTEXT *const c, const int16_t form)
{
    const BOSS_ENTITY *const e =
        g_B15M.form == B15M_FORM_GIRL ? &g_B15M.girl : &g_B15M.bat;
    const uint8_t colors[] = { 3, 6, 8, 2 };
    const BOSS_SHOT_HITBOX box = form == B15M_FORM_GIRL
        ? (BOSS_SHOT_HITBOX) { Math16_Wrap(g_B15M.girl.left + 32),
                               Math16_Wrap(g_B15M.girl.top + 32), 64, 48 }
        : (BOSS_SHOT_HITBOX) { g_B15M.bat.left, g_B15M.bat.left, 32, 32 };
    BossHit_UpdateAndRender(
        &g_B15M.hit, c->hit, &g_B15M.hp, &g_B15M.boss_palette, colors, 4, 7000,
        BossEntity_HitTestOrb(e, c->hit->orb), box);
}
static void M_Next(const int8_t phase, const int16_t pattern)
{
    g_B15M.phase = phase;
    g_B15M.frame = 0;
    g_B15M.hit.frame = 0;
    g_B15M.pattern = pattern;
    g_B15M.teleport_done = false;
}
static void M_Common(void)
{
    g_B15M.frame = Math16_Wrap(g_B15M.frame + 1);
    g_B15M.hit.frame = Math16_Wrap(g_B15M.hit.frame + 1);
    if (g_B15M.phase != B15M_PHASE_PATTERNS_1) {
        M_Sync();
    }
}
static int16_t M_SurroundX(
    const B15M_CONTEXT *const c, const BOSS_ENTITY *const e, const bool bat)
{
    return Math16_Wrap(e->left + M_Random(c) % 192 - (bat ? 72 : 32));
}
static int16_t M_SurroundY(
    const B15M_CONTEXT *const c, const BOSS_ENTITY *const e, const bool bat)
{
    return Math16_Wrap(e->top + M_Random(c) % 128 - (bat ? 48 : 32));
}
static void M_Rifts(
    const B15M_CONTEXT *const c, B15M_RIFTS *const r, const bool bat,
    const int16_t start, const int16_t end, const bool random_color)
{
    if (g_B15M.frame < start || g_B15M.frame > end || g_B15M.frame % 4 != 0) {
        return;
    }
    BOSS_ENTITY *const e = bat ? &g_B15M.bat : &g_B15M.girl;
    for (int32_t i = 0; i < 5; i++) {
        if (g_B15M.frame > start) {
            M_GrcErase(c, r->x[i], r->y[i]);
        }
        if (g_B15M.frame % 16 == (i * 4) % 16 || g_B15M.frame == start) {
            r->x[i] = M_SurroundX(c, e, bat);
            r->y[i] = M_SurroundY(c, e, bat);
        }
        if (g_B15M.frame < end) {
            const int16_t image = 4 + M_Random(c) % 2;
            const uint8_t color =
                random_color ? (uint8_t)(M_Random(c) % 16) : 4;
            M_Grc(c, r->x[i], r->y[i], image, color);
        }
    }
    Boss_Effect(c->hit, 7);
}
static bool M_Teleport(
    const B15M_CONTEXT *const c, const int16_t x, const int16_t y)
{
    M_Sync();
    switch (g_B15M.frame) {
    case 20:
        Video_SelectDrawingPage(1);
        M_Background(c);
        Video_SelectDrawingPage(0);
        M_Unlock(&g_B15M.girl, c, 2, false);
        g_B15M.girl.orb_inactive = true;
        break;
    case 28:
        M_Background(c);
        M_Unlock(&g_B15M.girl, c, 3, false);
        g_B15M.girl.orb_inactive = true;
        break;
    case 36:
        g_B15M.girl.orb_inactive = true;
        M_Background(c);
        M_Unlock(&g_B15M.girl, c, 4, false);
        break;
    case 44:
        g_B15M.girl.orb_inactive = true;
        Sprite_Restore16(g_B15M.girl.left, g_B15M.girl.top, 128, 96);
        break;
    case 52:
        g_B15M.girl.orb_inactive = true;
        g_B15M.girl.left = x;
        g_B15M.girl.top = y;
        M_Snap(c);
        M_Background(c);
        M_Unlock(&g_B15M.girl, c, 4, false);
        break;
    case 60:
        g_B15M.girl.orb_inactive = true;
        M_Background(c);
        M_Unlock(&g_B15M.girl, c, 3, false);
        break;
    case 68:
        g_B15M.girl.orb_inactive = true;
        M_Background(c);
        M_Unlock(&g_B15M.girl, c, 2, false);
        break;
    case 76:
        g_B15M.girl.orb_inactive = false;
        M_PutBoth(&g_B15M.girl, c, 0);
        break;
    default:
        if (g_B15M.frame > 80) {
            g_B15M.frame = 0;
            return true;
        }
        break;
    }
    if (g_B15M.frame % 4 != 0) {
        return false;
    }
    for (int32_t i = 0; i < 5; i++) {
        if (g_B15M.frame > 4) {
            Sprite_Restore16(g_B15M.stars.x[i], g_B15M.stars.y[i], 16, 8);
        }
        const int16_t tx = M_SurroundX(c, &g_B15M.girl, false),
                      ty = M_SurroundY(c, &g_B15M.girl, false);
        if (g_B15M.frame < 40 || g_B15M.frame > 52) {
            g_B15M.stars.x[i] = tx;
            g_B15M.stars.y[i] = ty;
        } else {
            g_B15M.stars.x[i] = Math16_Wrap(
                g_B15M.stars.x[i] + Math16_Wrap(tx - g_B15M.stars.x[i]) / 3);
            g_B15M.stars.y[i] = Math16_Wrap(
                g_B15M.stars.y[i] + Math16_Wrap(ty - g_B15M.stars.y[i]) / 3);
        }
        if (g_B15M.frame < 68) {
            M_Star8(c, g_B15M.stars.x[i], g_B15M.stars.y[i]);
        }
    }
    return false;
}
static void M_TrailCopy(const int32_t i, const int16_t x, const int16_t y)
{
    for (int32_t t = 2; t > 0; t--) {
        g_B15M.trail_x[i][t] = g_B15M.trail_x[i][t - 1];
        g_B15M.trail_y[i][t] = g_B15M.trail_y[i][t - 1];
    }
    g_B15M.trail_x[i][0] = x;
    g_B15M.trail_y[i][0] = y;
}
static void M_Trail(
    const B15M_CONTEXT *const c, const int32_t i, const int16_t x,
    const int16_t y, const int16_t valid)
{
    if (valid > 1) {
        M_GrcErase(c, g_B15M.trail_x[i][0], g_B15M.trail_y[i][0]);
    }
    M_Grc(c, x, y, 3, 8);
    if (valid > 1) {
        if (valid > 2) {
            M_GrcErase(c, g_B15M.trail_x[i][1], g_B15M.trail_y[i][1]);
        }
        M_Grc(c, g_B15M.trail_x[i][0], g_B15M.trail_y[i][0], 2, 8);
    }
    if (valid > 2) {
        if (valid > 3) {
            M_GrcErase(c, g_B15M.trail_x[i][2], g_B15M.trail_y[i][2]);
        }
        M_Grc(c, g_B15M.trail_x[i][1], g_B15M.trail_y[i][1], 1, 8);
    }
}
static bool M_Entrance(const B15M_CONTEXT *const c)
{
    if (!g_B15M.entrance_active) {
        g_B15M.entrance_active = true;
        g_B15M.entrance_stage = 0;
        g_B15M.entrance_angle = 0;
        g_B15M.hit.frame = 0;
        g_B15M.boss_palette = c->hit->palette->saved;
        M_Snap(c);
    }
    if (g_B15M.entrance_waiting) {
        if (!Frame_DelayReady((uint16_t)g_B15M.pending_scroll)) {
            return false;
        }
        g_B15M.entrance_waiting = false;
    }
    for (;;) {
        g_B15M.hit.frame = Math16_Wrap(g_B15M.hit.frame + 1);
        if (g_B15M.entrance_stage == 0) {
            for (int32_t i = 0; i < 2; i++) {
                const int16_t x =
                    Math16_PolarX(304, 96, g_B15M.entrance_angle + i * 128);
                const int16_t y =
                    Math16_PolarY(114, 96, g_B15M.entrance_angle + i * 128);
                M_Trail(c, i, x, y, g_B15M.hit.frame);
                M_TrailCopy(i, x, y);
            }
            if (g_B15M.hit.frame >= 120) {
                g_B15M.entrance_stage = 1;
                g_B15M.hit.frame = 0;
                g_B15M.trails_offscreen = false;
                continue;
            }
            g_B15M.entrance_angle = (uint8_t)(g_B15M.entrance_angle + 24);
            g_B15M.pending_scroll = (120 - g_B15M.hit.frame) / 12 + 1;
        } else {
            const int16_t f = g_B15M.hit.frame;
            if (!g_B15M.trails_offscreen && f % 4 == 0) {
                for (int32_t i = 0; i < 2; i++) {
                    const int16_t x = Math16_Wrap(
                                      g_B15M.trail_x[i][0] + 64 - i * 128),
                                  y = g_B15M.trail_y[i][0];
                    M_Trail(c, i, x, y, 4);
                    if (g_B15M.trail_x[0][2] > 640
                        && g_B15M.trail_x[1][2] < -32) {
                        g_B15M.trails_offscreen = true;
                    }
                    M_TrailCopy(i, x, y);
                }
            }
            if (f > 20 && f < 100) {
                if (f % 4 == 0) {
                    if (f != 24) {
                        Wave_Restore(
                            g_B15M.girl.left, g_B15M.girl.top, f - 2, 104 - f,
                            (f - 4) * 6, 144, 96);
                    }
                    BossEntity_WavePut(
                        &g_B15M.girl, g_B15M.girl.left, g_B15M.girl.top, 0,
                        f + 2, 100 - f, f * 6);
                }
            } else if (f == 100) {
                Wave_Restore(
                    g_B15M.girl.left, g_B15M.girl.top, 102, 0, 100, 144, 96);
                M_Unlock(&g_B15M.girl, c, 0, true);
            } else if (f == 128) {
                M_Background(c);
                BossEntity_WavePut(
                    &g_B15M.girl, g_B15M.girl.left, g_B15M.girl.top, 0, 3, 8,
                    64);
            } else if (f == 130) {
                Wave_Restore(
                    g_B15M.girl.left, g_B15M.girl.top, 3, 8, 64, 144, 96);
                M_Unlock(&g_B15M.girl, c, 0, true);
            } else if (f > 140) {
                g_B15M.frame = 0;
                g_B15M.hit.frame = 0;
                g_B15M.phase = B15M_PHASE_PATTERNS_1;
                g_B15M.girl.orb_inactive = false;
                M_PutBoth(&g_B15M.girl, c, g_B15M.girl.image);
                g_B15M.teleport_done = false;
                g_B15M.pattern = 1;
                g_B15M.hp_bar_filled = false;
                g_B15M.entrance_active = false;
                return true;
            }
            g_B15M.pending_scroll = 1;
        }
        if (!Frame_DelayReady((uint16_t)g_B15M.pending_scroll)) {
            g_B15M.entrance_waiting = true;
            return false;
        }
    }
}

static int16_t M_LasersAcross(const B15M_CONTEXT *const c)
{
    const int16_t x = M_CenterX(B15M_FORM_GIRL),
                  y = Math16_Wrap(g_B15M.girl.top + 32);
    if (g_B15M.frame == 50) {
        g_B15M.direction = M_Random(c) % 2;
        M_Both(c, &g_B15M.girl, 1, false);
        g_B15M.pattern_state = Boss_Rank(50, 55, 60, 65);
    } else if (g_B15M.frame == 60) {
        M_Circle(c, x, y, 32, 4, 2, 0, 255);
        g_B15M.circle_radius = 32;
    } else if (
        g_B15M.frame > 60 && g_B15M.frame < 120 && g_B15M.frame % 2 == 0) {
        M_EraseCircle(c, x, y, g_B15M.circle_radius, 2);
        g_B15M.circle_radius = Math16_Wrap(g_B15M.circle_radius + 8);
        M_Circle(
            c, x, y, g_B15M.circle_radius, 4,
            g_B15M.frame < 100       ? 2
                : g_B15M.frame < 110 ? 8
                                     : 32,
            0, 255);
    } else if (g_B15M.frame == 120) {
        M_EraseCircle(c, x, y, g_B15M.circle_radius, 32);
    } else if (g_B15M.frame == 150) {
        M_Both(c, &g_B15M.girl, 0, false);
    }
    if (g_B15M.frame >= 70 && g_B15M.frame % 10 == 0) {
        const int16_t tx = g_B15M.direction == 0
            ? Math16_Wrap((g_B15M.frame - 70) / 10 * 64)
            : Math16_Wrap(640 - (g_B15M.frame - 70) / 10 * 64);
        M_Laser(
            c, g_B15M.frame / 10, Math16_Wrap(g_B15M.girl.left + 60),
            Math16_Wrap(g_B15M.girl.top + 28), tx, 400, g_B15M.pattern_state,
            25, 4);
        Boss_Effect(c->hit, 6);
        if ((g_B15M.direction == 0 && tx >= 640)
            || (g_B15M.direction == 1 && tx <= 0)) {
            g_B15M.frame = 0;
            return 0;
        }
    }
    return 1;
}
static int16_t M_RandomMissiles(const B15M_CONTEXT *const c)
{
    if (g_B15M.frame == 50) {
        M_Both(c, &g_B15M.girl, 1, false);
        g_B15M.pattern_state = Boss_Rank(15, 21, 25, 29);
    }
    if (g_B15M.frame > 60 && g_B15M.frame % 3 == 0) {
        const int32_t i = M_Random(c) % 5;
        const uint8_t angle = (uint8_t)(M_Random(c) % g_B15M.pattern_state
                                        - (g_B15M.pattern_state - 1) / 2 + 64);
        const MATH16_VECTOR v = Math16_Vector(7, angle);
        Missiles_Add(
            c->missiles, g_B15M.missile_rifts.x[i], g_B15M.missile_rifts.y[i],
            v.x, v.y, 0);
    }
    M_Rifts(c, &g_B15M.missile_rifts, false, 60, 160, false);
    if (g_B15M.frame > 170) {
        g_B15M.frame = 0;
        return 0;
    }
    return 2;
}
static int16_t M_CirclePellets(const B15M_CONTEXT *const c)
{
    if (g_B15M.frame < 10) {
        g_B15M.circle.frame = 0;
    }
    if (g_B15M.frame == 50) {
        M_Both(c, &g_B15M.girl, 1, false);
        g_B15M.circle.angle = 0;
        g_B15M.pattern_state = Boss_Rank(
            PELLET_GROUP_SINGLE_AIMED, PELLET_GROUP_RANDOM_NARROW_AIMED,
            PELLET_GROUP_THREE_WIDE_AIMED, PELLET_GROUP_FIVE_NARROW_AIMED);
    }
    if (M_SummonFlash(c, &g_B15M.circle, 60, 0)) {
        M_EraseBigCircle(c);
        g_B15M.circle.angle = 0;
        for (int32_t i = 0; i < 32; i++) {
            const int16_t x = Math16_PolarX(
                Math16_Wrap(g_B15M.girl.left + 60), 128, g_B15M.circle.angle);
            const int16_t y = Math16_PolarY(
                M_CenterY(B15M_FORM_GIRL), 128, g_B15M.circle.angle);
            M_Group(c, x, y, (PELLET_GROUP)g_B15M.pattern_state, 4);
            g_B15M.circle.angle = (uint8_t)(g_B15M.circle.angle + 8);
        }
        g_B15M.frame = 0;
        g_B15M.circle.frame = 0;
        return 0;
    }
    return 3;
}
static int16_t M_Phase1(const B15M_CONTEXT *const c, const int16_t id)
{
    switch (id) {
    case 0:
        return M_Random(c) % 4;
    case 1:
        return M_LasersAcross(c);
    case 2:
        return M_RandomMissiles(c);
    case 3:
        return M_CirclePellets(c);
    default:
        return 0;
    }
}
static void M_StarLines(
    const B15M_CONTEXT *const c, const bool erase, const uint8_t color)
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
static int16_t M_Star(const B15M_CONTEXT *const c)
{
    B15M_CIRCLE *const circle = &g_B15M.star_circle;
    if (g_B15M.frame < 5) {
        circle->frame = 0;
        circle->angle = 0;
    }
    if (g_B15M.frame < 10) {
        return 0;
    }
    if (g_B15M.frame == 10) {
        M_Both(c, &g_B15M.attack, 0, true);
        circle->angle = 0;
        circle->frame = 0;
    }
    if (g_B15M.frame >= 10 && g_B15M.frame % 2 == 0 && circle->frame == 0) {
        if (g_B15M.frame == 10) {
            Boss_Effect(c->hit, 8);
        }
        M_SummonDots(
            c, circle, M_CenterX(B15M_FORM_GIRL), M_CenterY(B15M_FORM_GIRL), 2);
        if (circle->angle >= 64) {
            circle->frame = 1;
            M_EraseBigCircle(c);
            M_BigCircle(c, 7);
        }
    } else if (circle->angle >= 64) {
        circle->frame = Math16_Wrap(circle->frame + 1);
        if (circle->frame == 10) {
            M_StarLines(c, false, 7);
        }
        if (circle->frame > 20 && circle->frame % 4 == 0) {
            M_StarLines(c, false, 4);
            M_BigCircle(c, 4);
        } else if (circle->frame > 20 && circle->frame % 4 == 2) {
            M_StarLines(c, false, 7);
            M_BigCircle(c, 7);
        }
        if (circle->frame > 60) {
            M_StarLines(c, true, 0);
            M_EraseBigCircle(c);
            g_B15M.frame = 0;
            circle->angle = 0;
            circle->frame = 0;
            return 1;
        }
    }
    return 0;
}
static int16_t M_CurvedRings(const B15M_CONTEXT *const c)
{
    M_Attack(c);
    int16_t offset = -1, speed = 0;
    if (g_B15M.frame == 10) {
        g_B15M.pattern_state = Boss_Rank(14, 16, 18, 20);
        offset = 0;
        speed = 48;
    } else if (g_B15M.frame == 16) {
        offset = 2;
        speed = 54;
    } else if (g_B15M.frame == 24) {
        offset = 4;
        speed = 64;
    } else if (g_B15M.frame == 32) {
        offset = 6;
        speed = 72;
    } else if (g_B15M.frame == 40) {
        offset = 8;
        speed = 80;
    } else if (g_B15M.frame > 60) {
        g_B15M.frame = 0;
        Video_SelectDrawingPage(1);
        M_Background(c);
        return 0;
    }
    if (offset >= 0) {
        for (int32_t i = 0; i < g_B15M.pattern_state; i++) {
            M_Single(
                c, Math16_Wrap(M_CenterX(B15M_FORM_GIRL) - 4),
                Math16_Wrap(M_CenterY(B15M_FORM_GIRL) - 4),
                (uint8_t)((256 / g_B15M.pattern_state) * i + offset), speed,
                PELLET_REGULAR, 0);
        }
    }
    return 1;
}
static int16_t M_SphereClusters(const B15M_CONTEXT *const c)
{
    M_Attack(c);
    if (g_B15M.frame <= 100 && g_B15M.frame % 10 == 0) {
        const int32_t n = g_B15M.frame / 10;
        const int16_t column =
            Math16_Wrap((g_B15M.girl.left < 256 ? 608 : 0) - g_B15M.girl.left)
            / 10;
        g_B15M.sphere_x[n - 1] =
            Math16_Wrap(n * column + M_CenterX(B15M_FORM_GIRL) - 16);
        const int16_t row =
            Math16_Wrap(M_Random(c) % 200 + 200 - g_B15M.girl.top) / 10;
        g_B15M.sphere_y[n - 1] =
            Math16_Wrap(n * row + M_CenterY(B15M_FORM_GIRL) - 16);
        for (int32_t i = 0; i < n; i++) {
            M_Grc(c, g_B15M.sphere_x[i], g_B15M.sphere_y[i], 3, 4);
        }
    } else if (
        g_B15M.frame > 100 && g_B15M.frame <= 230 && g_B15M.frame % 10 == 0) {
        if (g_B15M.frame <= 200) {
            const int32_t n = (g_B15M.frame - 100) / 10;
            Boss_Effect(c->hit, 7);
            M_Grc(c, g_B15M.sphere_x[n - 1], g_B15M.sphere_y[n - 1], 4, 4);
            for (int32_t i = n; i < 10; i++) {
                M_Grc(c, g_B15M.sphere_x[i], g_B15M.sphere_y[i], 3, 4);
            }
            g_B15M.pattern_state = Boss_Rank(5, 7, 9, 13);
            for (int32_t i = 0; i < g_B15M.pattern_state; i++) {
                const uint8_t angle = M_Random(c) & 127;
                const int16_t speed = (M_Random(c) + 24) & 63;
                M_Single(
                    c, g_B15M.sphere_x[n - 1], g_B15M.sphere_y[n - 1], angle,
                    speed, PELLET_REGULAR, 0);
            }
        }
        if (g_B15M.frame > 130) {
            const int32_t i = (g_B15M.frame - 130) / 10 - 1;
            M_GrcErase(c, g_B15M.sphere_x[i], g_B15M.sphere_y[i]);
        }
    } else if (g_B15M.frame > 250) {
        g_B15M.frame = 0;
        return 0;
    }
    return 2;
}
static int16_t M_RandomRifts(const B15M_CONTEXT *const c)
{
    M_Attack(c);
    if (g_B15M.frame == 30) {
        g_B15M.pattern_state = Boss_Rank(6, 3, 2, 2);
    }
    if (g_B15M.frame > 60 && g_B15M.frame < 120
        && g_B15M.frame % g_B15M.pattern_state == 0) {
        const int32_t i = M_Random(c) % 5;
        const uint8_t angle = (uint8_t)(M_Random(c) % 21 + 54);
        M_Single(
            c, g_B15M.random_rifts.x[i], g_B15M.random_rifts.y[i], angle, 96,
            PELLET_REGULAR, 0);
    } else if (g_B15M.frame >= 120 && g_B15M.frame < 200) {
        const int32_t i = M_Random(c) % 5;
        uint8_t angle = (uint8_t)(M_Random(c) % 21 + 54);
        const uint8_t offset =
            (uint8_t)((M_Random(c) & 1) ? g_B15M.frame - 120
                                        : -(g_B15M.frame - 120));
        angle = (uint8_t)(angle * (offset / 2));
        M_Single(
            c, g_B15M.random_rifts.x[i], g_B15M.random_rifts.y[i], angle, 96,
            PELLET_REGULAR, 0);
    }
    M_Rifts(c, &g_B15M.random_rifts, false, 60, 200, false);
    if (g_B15M.frame > 220) {
        g_B15M.frame = 0;
        return 0;
    }
    return 3;
}
static int16_t M_Phase3(const B15M_CONTEXT *const c, const int16_t id)
{
    if (id == 99) {
        g_B15M.phase3_pattern = 0;
        return 0;
    }
    if (id == 0) {
        return M_Random(c) % 4;
    }
    if (id < 1 || id > 3) {
        return 0;
    }
    if (g_B15M.phase3_pattern == 0) {
        g_B15M.phase3_pattern = M_Star(c);
        return id;
    }
    g_B15M.phase3_pattern = id == 1 ? M_CurvedRings(c)
        : id == 2                   ? M_SphereClusters(c)
                                    : M_RandomRifts(c);
    return g_B15M.phase3_pattern;
}

static bool M_ScrollWait(const int16_t top)
{
    g_B15M.pending = B15M_PENDING_SCROLL;
    g_B15M.pending_scroll = top;
    Frame_BeginWait(&g_B15M.wait);
    return false;
}
static void M_TransformResult(void)
{
    if (g_B15M.call_phase == B15M_PHASE_TRANSFORM) {
        g_B15M.pattern = g_B15M.transform_bat ? 1 : 0;
    } else {
        g_B15M.form = g_B15M.transform_bat ? B15M_FORM_BAT : B15M_FORM_GIRL;
    }
}
static bool M_TransformFinish(const B15M_CONTEXT *const c)
{
    if (g_B15M.frame <= 100) {
        if (g_B15M.call_phase == B15M_PHASE_TRANSFORM) {
            g_B15M.pattern = g_B15M.transform_bat ? 0 : 1;
        } else {
            g_B15M.form = g_B15M.transform_bat ? B15M_FORM_GIRL : B15M_FORM_BAT;
        }
        return true;
    }
    if (g_B15M.transform_bat) {
        g_B15M.bat.left = Math16_Wrap(g_B15M.girl.left + 40);
        g_B15M.bat.top = Math16_Wrap(g_B15M.girl.top + 32);
        g_B15M.bat.image = 0;
        Video_SelectDrawingPage(1);
        M_Background(c);
        Video_SelectDrawingPage(0);
        M_Background(c);
    } else {
        Sprite_Restore16(g_B15M.bat.left, g_B15M.bat.top, 48, 32);
        int16_t x = Math16_Wrap(g_B15M.bat.left - 40),
                y = Math16_Wrap(g_B15M.bat.top - 32);
        if (x < 0) {
            x = 0;
        } else if (x > 512) {
            x = 512;
        }
        if (y < 64) {
            y = 64;
        }
        g_B15M.girl.left = x;
        g_B15M.girl.top = y;
        M_Snap(c);
        M_Both(c, &g_B15M.attack, 1, true);
    }
    g_B15M.transform_stage = B15M_TRANSFORM_SWAPPED;
    return M_ScrollWait(400);
}
static bool M_Transform(const B15M_CONTEXT *const c, const bool bat)
{
    g_B15M.transform_bat = bat;
    g_B15M.transform_stage = B15M_TRANSFORM_RIFTS;
    if (bat && g_B15M.frame == 20) {
        M_Both(c, &g_B15M.attack, 0, true);
    }
    M_Rifts(
        c, bat ? &g_B15M.girl_rifts : &g_B15M.bat_rifts, !bat, 20, 100, true);
    if (g_B15M.frame >= 20 && g_B15M.frame % 2 == 0) {
        return M_ScrollWait(Math16_Wrap(400 - g_B15M.frame % 4 * 2));
    }
    return M_TransformFinish(c);
}
static int16_t M_Abs(const int16_t value)
{
    return value < 0 ? Math16_Wrap(-(int32_t)value) : value;
}
static int16_t M_BatFly(const B15M_CONTEXT *const c)
{
    if (g_B15M.frame < 3) {
        g_B15M.bat_velocity_x = 0;
        g_B15M.bat_velocity_y = 0;
        return 0;
    }
    if (g_B15M.frame == 3) {
        do {
            g_B15M.bat_target_x = M_RandomLeft(c);
            g_B15M.bat_target_y = Math16_Wrap(M_RandomTop(c) + 32);
        } while (M_Abs(Math16_Wrap(g_B15M.bat.left - g_B15M.bat_target_x)) < 128
                 && M_Abs(Math16_Wrap(g_B15M.bat.top - g_B15M.bat_target_y))
                     < 64);
        const MATH16_VECTOR v = Math16_Aim(
            (MATH16_VECTOR) { g_B15M.bat.left, g_B15M.bat.top },
            (MATH16_VECTOR) { g_B15M.bat_target_x, g_B15M.bat_target_y }, 2, 0);
        g_B15M.bat_velocity_x = v.x;
        g_B15M.bat_velocity_y = v.y;
        g_B15M.bat_target_frames =
            M_Abs(Math16_Wrap(g_B15M.bat_target_x - g_B15M.bat.left)) / 2;
    }
    g_B15M.bat_velocity_y = Math16_Wrap(
        g_B15M.bat_velocity_y + ((int32_t)M_Random(c) % 5 - 2) / 10);
    if (g_B15M.frame / 3 >= g_B15M.bat_target_frames) {
        g_B15M.bat_velocity_x = 0;
        g_B15M.bat_velocity_y = 0;
        g_B15M.frame = 0;
        return 1;
    }
    return 0;
}
static void M_BatPattern(const B15M_CONTEXT *const c)
{
    switch (g_B15M.bat_pattern) {
    case 1:
        if (g_B15M.frame % 16 == 0) {
            M_FormGroup(
                c, B15M_FORM_BAT,
                (PELLET_GROUP)Boss_Rank(
                    PELLET_GROUP_THREE_WIDE_AIMED, PELLET_GROUP_FIVE_WIDE_AIMED,
                    PELLET_GROUP_FIVE_NARROW_AIMED, PELLET_GROUP_FIVE_NARROW),
                56);
        }
        break;
    case 2:
        if (g_B15M.frame % 4 == 0) {
            const PELLET_GROUP group = (M_Random(c) & 1)
                ? PELLET_GROUP_TWO_NARROW_AIMED
                : PELLET_GROUP_TWO_WIDE_AIMED;
            M_FormGroup(c, B15M_FORM_BAT, group, 88);
        }
        break;
    case 3:
        if (g_B15M.frame % 4 == 0) {
            const uint8_t angle = (uint8_t)M_Random(c);
            M_Single(
                c, Math16_Wrap(M_CenterX(B15M_FORM_BAT) - 4),
                Math16_Wrap(M_CenterY(B15M_FORM_BAT) - 4), angle, 2,
                PELLET_GRAVITY, 1);
        }
        break;
    case 4:
        if (g_B15M.frame % 50 < 10 && g_B15M.frame % 4 == 0) {
            M_FormGroup(c, 1, PELLET_GROUP_TWO_NARROW_AIMED, 85);
        }
        if (g_B15M.frame % 50 >= 25 && g_B15M.frame % 50 < 35
            && g_B15M.frame % 4 == 0) {
            M_FormGroup(c, 1, PELLET_GROUP_THREE_NARROW_AIMED, 85);
        }
        break;
    default:
        break;
    }
}
static void M_SymmetricFire(
    const B15M_CONTEXT *const c, const uint8_t angle, const int16_t speed)
{
    const int16_t y = Math16_Wrap(M_CenterY(B15M_FORM_GIRL) - 8);
    M_Single(
        c, Math16_Wrap(M_CenterX(B15M_FORM_GIRL) - 8), y,
        (uint8_t)(128 - angle), speed, PELLET_REGULAR, 0);
    M_Single(c, M_CenterX(B15M_FORM_GIRL), y, angle, speed, PELLET_REGULAR, 0);
}
static int16_t M_SymmetricArc(const B15M_CONTEXT *const c)
{
    M_Attack(c);
    if (g_B15M.frame == 50) {
        g_B15M.pattern_state = Boss_Rank(2, 3, 3, 4);
        for (int32_t i = 0; i < 4; i++) {
            M_SymmetricFire(
                c, 64, Math16_Wrap((i + g_B15M.pattern_state) * 16));
        }
    } else if (g_B15M.frame == 60 || g_B15M.frame == 70) {
        const uint8_t angle = g_B15M.frame == 60 ? 48 : 24;
        for (int32_t i = 0; i < 4; i++) {
            M_SymmetricFire(
                c, angle, Math16_Wrap((i + g_B15M.pattern_state) * 16));
        }
        Boss_Effect(c->hit, 7);
    } else if (g_B15M.frame == 80) {
        for (int32_t i = 0; i < 10; i++) {
            M_SymmetricFire(
                c, (uint8_t)(i * 6),
                Math16_Wrap((2 + g_B15M.pattern_state) * 16));
        }
        Boss_Effect(c->hit, 7);
    } else if (g_B15M.frame >= 120) {
        g_B15M.frame = 0;
        return 0;
    }
    return 1;
}
static void M_SafetyAfterHit(const B15M_CONTEXT *const c)
{
    if (Boss_Player(c->hit)->is_hit) {
        g_B15M.safety_circle.frame = 200;
    }
    if (g_B15M.frame % g_B15M.pattern_state == 0) {
        const int16_t x = M_Random(c) % 632;
        M_Group(
            c, x, 64,
            c->hit->display->rank == 3 ? PELLET_GROUP_RANDOM_WIDE
                                       : PELLET_GROUP_SINGLE,
            72);
    }
    if (g_B15M.safety_circle.frame > 180
        && g_B15M.safety_circle.frame % 4 == 0) {
        M_FormGroup(c, 0, PELLET_GROUP_SINGLE_AIMED, 72);
    }
}
static int16_t M_Safety(const B15M_CONTEXT *const c, const uint64_t host_tick)
{
    B15M_CIRCLE *const circle = &g_B15M.safety_circle;
    if (g_B15M.frame < 10) {
        circle->frame = 0;
    }
    M_Attack(c);
    if (g_B15M.frame < 60) {
        return 1;
    }
    if (g_B15M.frame == 60) {
        g_B15M.pattern_state = Boss_Rank(4, 2, 2, 2);
        circle->angle = 0;
    }
    if (g_B15M.frame >= 60 && g_B15M.frame % 2 == 0 && circle->frame == 0) {
        if (g_B15M.frame == 60) {
            Boss_Effect(c->hit, 8);
            g_B15M.safety_left = Boss_Player(c->hit)->left;
        }
        M_SummonDots(c, circle, Math16_Wrap(g_B15M.safety_left + 16), 384, 2);
        if (circle->angle >= 64) {
            M_EraseCircle(c, Math16_Wrap(g_B15M.safety_left + 16), 384, 128, 1);
            M_Circle(
                c, Math16_Wrap(g_B15M.safety_left + 16), 384, 128, 7, 1, 0,
                255);
            circle->frame = 1;
            M_Both(c, &g_B15M.girl, 0, true);
        }
    } else if (circle->frame != 0 && circle->frame < 200) {
        circle->frame = Math16_Wrap(circle->frame + 1);
        if (circle->frame % 4 == 0) {
            M_EraseCircle(c, Math16_Wrap(g_B15M.safety_left + 16), 384, 128, 1);
            M_Circle(
                c, Math16_Wrap(g_B15M.safety_left + 16), 384, 128, 7, 1, 0,
                255);
        }
        if (!Boss_Player(c->hit)->invincible
            && (Math16_Wrap(g_B15M.safety_left + 104)
                    <= Boss_Player(c->hit)->left
                || Math16_Wrap(g_B15M.safety_left - 128)
                    >= Boss_Player(c->hit)->left)) {
            g_B15M.pending = B15M_PENDING_SAFETY;
            g_B15M.pending_tick = host_tick;
            return -1;
        }
        M_SafetyAfterHit(c);
    } else if (circle->frame < 200) {
        return 1;
    } else {
        if (circle->frame == 200) {
            M_EraseCircle(c, Math16_Wrap(g_B15M.safety_left + 16), 384, 128, 1);
        }
        circle->frame = Math16_Wrap(circle->frame + 1);
        if (circle->frame % 8 == 0) {
            M_Group(
                c, Math16_Wrap(g_B15M.bat.left + 60),
                Math16_Wrap(g_B15M.bat.top + 44), PELLET_GROUP_SINGLE_AIMED,
                72);
        }
        if (circle->frame > 260) {
            g_B15M.frame = 0;
            circle->frame = 0;
            circle->angle = 0;
            return 0;
        }
    }
    return 1;
}
static int16_t M_AimedRing(const B15M_CONTEXT *const c)
{
    M_Attack(c);
    if (g_B15M.frame < 50) {
        return 1;
    }
    if (g_B15M.frame == 50) {
        M_Both(c, &g_B15M.girl, 1, false);
        g_B15M.aimed_circle.angle = 0;
        g_B15M.pattern_state = Boss_Rank(40, 30, 20, 15);
    }
    if (M_SummonFlash(c, &g_B15M.aimed_circle, 60, 2)) {
        M_EraseBigCircle(c);
        g_B15M.aimed_circle.angle = 0;
        for (int32_t i = 0; i < 32; i++) {
            const int16_t x = Math16_PolarX(
                M_CenterX(B15M_FORM_GIRL), 128, g_B15M.aimed_circle.angle);
            const int16_t y = Math16_PolarY(
                Math16_Wrap(g_B15M.girl.top + 28), 128,
                g_B15M.aimed_circle.angle);
            M_Group(c, x, y, PELLET_GROUP_SINGLE, 64);
            g_B15M.aimed_circle.angle =
                (uint8_t)(g_B15M.aimed_circle.angle + 8);
        }
        g_B15M.frame = 0;
        g_B15M.aimed_circle.frame = 0;
        return 0;
    }
    if (g_B15M.frame % g_B15M.pattern_state == 0) {
        M_Group(
            c, Math16_Wrap(M_CenterX(B15M_FORM_GIRL) - 42),
            Math16_Wrap(M_CenterY(B15M_FORM_GIRL) - 8),
            PELLET_GROUP_FIVE_WIDE_AIMED, 64);
        M_Group(
            c, Math16_Wrap(M_CenterX(B15M_FORM_GIRL) + 36),
            Math16_Wrap(M_CenterY(B15M_FORM_GIRL) - 8),
            PELLET_GROUP_FIVE_WIDE_AIMED, 64);
    }
    if (g_B15M.frame % (g_B15M.pattern_state * 2) == 0) {
        M_Laser(
            c, g_B15M.frame / 10, Math16_Wrap(M_CenterX(B15M_FORM_GIRL) - 4),
            Math16_Wrap(g_B15M.girl.top + 28), Boss_Player(c->hit)->left, 400,
            50, 20, 8);
        Boss_Effect(c->hit, 6);
    }
    return 1;
}
static int16_t M_GirlPhase5(
    const B15M_CONTEXT *const c, const uint64_t host_tick)
{
    if (g_B15M.girl_pattern == 0) {
        g_B15M.girl_pattern = M_Random(c) % 3 + 1;
        return 0;
    }
    if (g_B15M.girl_subphase == 0) {
        g_B15M.girl_subphase = M_Star(c);
        return 0;
    }
    int16_t result = 1;
    switch (g_B15M.girl_pattern) {
    case 1:
        result = M_SymmetricArc(c);
        break;
    case 2:
        result = M_Safety(c, host_tick);
        break;
    case 3:
        result = M_AimedRing(c);
        break;
    default:
        break;
    }
    if (result < 0) {
        return -1;
    }
    g_B15M.girl_subphase = result;
    if (result == 0) {
        g_B15M.girl_pattern = 0;
        return 1;
    }
    return 0;
}
static void M_ResetPhase5(void)
{
    g_B15M.phase5_subphase = 0;
    g_B15M.bat_pattern = 0;
    g_B15M.girl_pattern = 0;
    g_B15M.girl_subphase = 0;
}
static void M_AfterTransform(void)
{
    if (g_B15M.call_phase == B15M_PHASE_TRANSFORM) {
        if (g_B15M.pattern == 1) {
            M_Next(B15M_PHASE_FORMS, 0);
            g_B15M.form = B15M_FORM_BAT;
            M_ResetPhase5();
        }
    } else if (g_B15M.phase5_subphase == 1) {
        if (g_B15M.transform_bat) {
            if (g_B15M.form == B15M_FORM_BAT) {
                g_B15M.phase5_subphase = 0;
            }
        } else if (g_B15M.form == B15M_FORM_GIRL) {
            g_B15M.bat_pattern = 0;
            g_B15M.phase5_subphase = 0;
        }
    }
}
static bool M_Phase5(const B15M_CONTEXT *const c, const uint64_t host_tick)
{
    if (g_B15M.form == B15M_FORM_BAT) {
        if (g_B15M.phase5_subphase == 0) {
            g_B15M.phase5_subphase = M_BatFly(c);
            if (g_B15M.bat_pattern == 0) {
                g_B15M.bat_pattern = M_Random(c) % 4 + 1;
            }
            M_BatPattern(c);
        } else if (g_B15M.phase5_subphase == 1) {
            if (!M_Transform(c, false)) {
                return false;
            }
            M_AfterTransform();
        }
    } else {
        if (g_B15M.phase5_subphase == 0) {
            const int16_t result = M_GirlPhase5(c, host_tick);
            if (result < 0) {
                return false;
            }
            g_B15M.phase5_subphase = result;
        } else {
            if (!M_Transform(c, true)) {
                return false;
            }
            M_AfterTransform();
        }
    }
    return true;
}
static bool M_Death(const B15M_CONTEXT *const c)
{
    const BOSS_DEFEAT_CONTEXT context = { .palette = c->hit->palette,
                                          .post_defeat = &g_B15M.post_defeat,
                                          .miko = c->miko,
                                          .player = Boss_Player(c->hit),
                                          .origin_top = &c->hit->orb->top,
                                          .stage_cleared = c->stage_cleared };
    if (!BossDefeat_Resume(&g_B15M.defeat, &context)) {
        return false;
    }
    Scene_Activate(c->scene, &c->assets->next_scene);
    g_B15M.defeating = false;
    g_B15M.finished = true;
    return true;
}
static bool M_Tail(const B15M_CONTEXT *const c)
{
    if (g_B15M.call_phase == B15M_PHASE_FORMS) {
        M_Hit(c, g_B15M.form);
        if (g_B15M.hp <= 0) {
            c->fade_music(c->hit->user);
            Pellets_EraseAndResetMoving(c->pellets);
            M_Background(c);
            Missiles_Reset(c->missiles);
            ShootoutLasers_EraseAndReset(c->lasers, 10);
            BossDefeat_Begin(&g_B15M.defeat, BOSS_DEFEAT_REGULAR);
            g_B15M.defeating = true;
            return M_Death(c);
        }
    }
    return true;
}
bool B15M_UpdateAndRender(const B15M_CONTEXT *const c, const uint64_t host_tick)
{
    if (g_B15M.finished) {
        return true;
    }
    if (g_B15M.defeating) {
        return M_Death(c);
    }
    if (g_B15M.pending == B15M_PENDING_SCROLL) {
        if (!Frame_WaitReady(&g_B15M.wait)) {
            return false;
        }
        g_B15M.pending = B15M_PENDING_NONE;
        Video_SetScroll(g_B15M.pending_scroll);
        if (g_B15M.transform_stage == B15M_TRANSFORM_RIFTS) {
            if (!M_TransformFinish(c)) {
                return false;
            }
        } else {
            g_B15M.frame = 0;
            M_TransformResult();
        }
        M_AfterTransform();
        return M_Tail(c);
    }
    if (g_B15M.pending == B15M_PENDING_SAFETY) {
        ASSERT(host_tick >= g_B15M.pending_tick);
        if (host_tick - g_B15M.pending_tick < 6) {
            return false;
        }
        g_B15M.pending = B15M_PENDING_NONE;
        Boss_Player(c->hit)->is_hit = true;
        M_SafetyAfterHit(c);
        g_B15M.girl_subphase = 1;
        g_B15M.phase5_subphase = 0;
        return M_Tail(c);
    }
    if (g_B15M.entrance_active) {
        return M_Entrance(c);
    }
    g_B15M.call_phase = g_B15M.phase;
    Missiles_UpdateAndRender(
        c->missiles, Boss_Player(c->hit), (uint16_t)c->hit->frame_rand,
        (MISSILES_OUTPUT) { M_MissileAction, (void *)c });
    if (g_B15M.phase == B15M_PHASE_ENTRANCE) {
        return M_Entrance(c);
    }
    if (g_B15M.phase == B15M_PHASE_PATTERNS_1
        || g_B15M.phase == B15M_PHASE_PATTERNS_2) {
        if (g_B15M.phase == B15M_PHASE_PATTERNS_1 && !g_B15M.hp_bar_filled) {
            g_B15M.hp_bar_filled =
                HudHp_Increment(c->hit->health, g_B15M.hp, g_B15M.frame);
        }
        M_Common();
        if (g_B15M.pattern != 0) {
            g_B15M.pattern = g_B15M.phase == B15M_PHASE_PATTERNS_1
                ? M_Phase1(c, g_B15M.pattern)
                : M_Phase3(c, g_B15M.pattern);
        } else if (!g_B15M.teleport_done) {
            const int16_t top = M_RandomTop(c);
            const int16_t left = M_RandomLeft(c);
            g_B15M.teleport_done = M_Teleport(c, left, top);
        } else {
            g_B15M.pattern = g_B15M.phase == B15M_PHASE_PATTERNS_1
                ? M_Phase1(c, 0)
                : M_Phase3(c, 0);
            g_B15M.teleport_done = false;
        }
        M_Hit(c, B15M_FORM_GIRL);
        if (!g_B15M.hit.invincible
            && g_B15M.hp <= (g_B15M.phase == B15M_PHASE_PATTERNS_1 ? 10 : 6)
            && g_B15M.teleport_done) {
            if (g_B15M.phase == B15M_PHASE_PATTERNS_1) {
                M_Next(B15M_PHASE_CENTER, 0);
            } else {
                M_Next(B15M_PHASE_TRANSFORM, 0);
                g_B15M.form = B15M_FORM_GIRL;
            }
        }
    } else if (g_B15M.phase == B15M_PHASE_CENTER) {
        g_B15M.frame = Math16_Wrap(g_B15M.frame + 1);
        g_B15M.teleport_done = M_Teleport(c, 256, 96);
        if (g_B15M.teleport_done) {
            g_B15M.phase3_pattern = 0;
            M_Next(B15M_PHASE_PATTERNS_2, 1);
        }
    } else if (g_B15M.phase == B15M_PHASE_TRANSFORM) {
        g_B15M.frame = Math16_Wrap(g_B15M.frame + 1);
        if (!g_B15M.teleport_done) {
            g_B15M.teleport_done = M_Teleport(c, 256, 96);
        } else {
            if (!M_Transform(c, true)) {
                return false;
            }
        }
        M_AfterTransform();
    } else if (g_B15M.phase == B15M_PHASE_FORMS) {
        M_Common();
        if (g_B15M.form != B15M_FORM_GIRL) {
            BossEntity_LockedMovePut(
                &g_B15M.bat, g_B15M.bat_velocity_x, g_B15M.bat_velocity_y, 2,
                true);
            if (g_B15M.frame % 9 == 0) {
                g_B15M.bat.image = 0;
            } else if (g_B15M.frame % 9 == 3) {
                g_B15M.bat.image = 1;
            } else if (g_B15M.frame % 9 == 6) {
                g_B15M.bat.image = 2;
            }
        }
        if (!M_Phase5(c, host_tick)) {
            return false;
        }
    }
    return M_Tail(c);
}
