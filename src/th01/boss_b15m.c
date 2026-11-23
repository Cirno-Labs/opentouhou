#include "th01/boss_b15m.h"

#include "th01/boss.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/assert.h"
#include "th01/shape.h"
#include "th01/sprite.h"
#include "th01/wave.h"

#include <string.h>

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
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, const uint8_t **const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "REIIDEN.EXE", VFS_IN_MEMORY, &reader));
    READER image;
    RESULT result = ReiidenImage_Open(&reader, scratch, scratch_size, &image);
    uint8_t mask[8];
    if (IS_OK(result)) {
        Reader_Seek(&image, 0x39252);
        Reader_ReadBytes(&image, mask, sizeof(mask));
        result = Reader_Result(&image);
    }
    Reader_Close(&reader);
    MUST(result);
    uint8_t *const allocated = Allocator_Alloc(a, sizeof(mask));
    memcpy(allocated, mask, sizeof(mask));
    *out = allocated;
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
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, B15M_ASSETS *const out)
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
    MUST(M_LoadStar(vfs, scratch, scratch_size, a, &loaded.star));
    MUST(B15M_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B15M_Init(B15M *const b, const ALLOCATOR *const a)
{
    *b = (B15M) { .background = Allocator_Alloc(a, 128 * 96) };
}

void B15M_Setup(B15M *const b, const B15M_CONTEXT *const c)
{
    ASSERT(
        !b->entrance_active && !b->defeating
        && b->pending == B15M_PENDING_NONE);
    ASSERT(b->background != nullptr);
    ASSERT(Result_Ignore(B15M_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->girl, c->sprites, 0, &c->assets->girl)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->attack, c->sprites, 1, &c->assets->attack)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->bat, c->sprites, 2, &c->assets->bat)));
    const BOSS_BOUNDS bounds = { 0, 736, 64, 304 };
    BossEntity_SetPosition(&b->girl, 256, 96, bounds);
    BossEntity_SetPosition(&b->attack, 256, 96, bounds);
    BossEntity_SetPosition(&b->bat, 256, 96, bounds);
    BossEntity_SetOrbHitbox(&b->girl, (BOSS_BOUNDS) { 16, 112, -4, 80 });
    BossEntity_SetOrbHitbox(&b->bat, (BOSS_BOUNDS) { -8, 56, -8, 40 });
    b->boss_palette = c->hit->palette->saved;
    b->post_defeat = (PALETTE) {};
    b->phase = B15M_PHASE_ENTRANCE;
    b->frame = 0;
    b->hp = 14;
    b->finished = false;
    c->hit->health->first_white = 10;
    c->hit->health->first_redwhite = 6;
    *c->rng = c->hit->frame_rand;
    c->pellets->interlace = true;
    Particles_Init(c->particles);
}

static int16_t M_W(const int32_t x)
{
    return Math16_Wrap(x);
}
static uint16_t M_Random(const B15M_CONTEXT *const c)
{
    return MasterRng_Next(c->rng);
}
static int16_t M_CenterX(const B15M *const b, const int16_t form)
{
    return M_W(form ? b->bat.left + 24 : b->girl.left + 64);
}
static int16_t M_CenterY(const B15M *const b, const int16_t form)
{
    return M_W(form ? b->bat.top + 16 : b->girl.top + 48);
}
static void M_Sync(B15M *const b)
{
    b->attack.left = b->girl.left;
    b->attack.top = b->girl.top;
}
static int16_t M_RandomLeft(const B15M_CONTEXT *const c)
{
    return M_Random(c) % 512;
}
static int16_t M_RandomTop(const B15M_CONTEXT *const c)
{
    return 64 + M_Random(c) % 136;
}
static void M_Snap(B15M *const b, const B15M_CONTEXT *const c)
{
    M_Sync(b);
    Video_Read(
        1, (GFX_RECT) { b->girl.left / 8 * 8, b->girl.top, 128, 96 },
        b->background, 128);
    Video_SelectDrawingPage(0);
}
static void M_Background(B15M *const b, const B15M_CONTEXT *const c)
{
    M_Sync(b);
    const VIDEO_IMAGE image = { .pixels = b->background,
                                .width = 128,
                                .height = 96,
                                .stride = 128,
                                .key = -1 };
    Video_Blit(
        &image, (GFX_RECT) { 0, 0, 128, 96 }, b->girl.left / 8 * 8,
        b->girl.top);
}
static void M_Unlock(
    BOSS_ENTITY *const e, const B15M_CONTEXT *const c, const int16_t image,
    const bool restore)
{
    e->lock_frame = 0;
    e->image = image;
    BossEntity_LockedMovePut(e, c->sprites, 0, 0, 3, restore);
}
static void M_Both(
    B15M *const b, const B15M_CONTEXT *const c, BOSS_ENTITY *const e,
    const int16_t image, const bool erase_page0)
{
    Video_SelectDrawingPage(1);
    M_Background(b, c);
    M_Unlock(e, c, image, false);
    Video_SelectDrawingPage(0);
    if (erase_page0) {
        M_Background(b, c);
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
static void M_Attack(B15M *const b, const B15M_CONTEXT *const c)
{
    if (b->frame % 8 == 0) {
        M_Both(b, c, &b->attack, 1, true);
    } else if (b->frame % 8 == 4) {
        M_PutBoth(&b->attack, c, 2);
    }
}
static void M_Grc(
    const B15M_CONTEXT *const c, const int16_t left, const int16_t top,
    const int16_t image, const uint8_t color)
{
    if (left < -32 || left >= 640 || top < -32 || top >= 400) {
        return;
    }
    int16_t row = M_W(top * 80 + left / 8 - (left % 8 < 0));
    const uint8_t *const pixels = c->assets->spheres.coverage + image * 1024;
    for (int32_t y = 0; y < 32; y++) {
        if (row >= 0) {
            const int32_t vy = row / 80;
            for (int32_t byte = 0; byte < 4; byte++) {
                const int16_t address = M_W(row + byte);
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
        row = M_W(row + 80);
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
static void M_BigCircle(
    B15M *const b, const B15M_CONTEXT *const c, const uint8_t color)
{
    M_Circle(
        c, M_CenterX(b, B15M_FORM_GIRL), M_CenterY(b, B15M_FORM_GIRL), 128,
        color, 1, 0, 255);
}
static void M_EraseBigCircle(B15M *const b, const B15M_CONTEXT *const c)
{
    M_EraseCircle(
        c, M_CenterX(b, B15M_FORM_GIRL), M_CenterY(b, B15M_FORM_GIRL), 128, 1);
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
    B15M *const b, const B15M_CONTEXT *const c, B15M_CIRCLE *const circle,
    const int16_t start, const int16_t end)
{
    if (b->frame >= start && b->frame % 2 == 0 && circle->frame == 0) {
        if (b->frame == start) {
            Boss_Effect(c->hit, 8);
        }
        M_SummonDots(
            c, circle, M_CenterX(b, B15M_FORM_GIRL),
            M_CenterY(b, B15M_FORM_GIRL), end);
        if (circle->angle >= 64) {
            M_EraseBigCircle(b, c);
            M_BigCircle(b, c, 7);
            circle->frame = 1;
            M_Both(b, c, &b->girl, 0, true);
        }
    } else if (circle->frame != 0 && circle->frame < 40) {
        circle->frame = M_W(circle->frame + 1);
        if (circle->frame % 8 == 0) {
            M_EraseBigCircle(b, c);
            M_BigCircle(b, c, 4);
        } else if (circle->frame % 8 == 4) {
            M_EraseBigCircle(b, c);
            M_BigCircle(b, c, 7);
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
    B15M *const b, const B15M_CONTEXT *const c, const int16_t form,
    const PELLET_GROUP group, const int16_t speed)
{
    M_Group(
        c, M_W(M_CenterX(b, form) - 4), M_W(M_CenterY(b, form) - 4), group,
        speed);
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
static void M_Hit(
    B15M *const b, const B15M_CONTEXT *const c, const int16_t form)
{
    const BOSS_ENTITY *const e = b->form == B15M_FORM_GIRL ? &b->girl : &b->bat;
    const uint8_t colors[] = { 3, 6, 8, 2 };
    const BOSS_SHOT_HITBOX box = form == B15M_FORM_GIRL
        ? (BOSS_SHOT_HITBOX) { M_W(b->girl.left + 32), M_W(b->girl.top + 32),
                               64, 48 }
        : (BOSS_SHOT_HITBOX) { b->bat.left, b->bat.left, 32, 32 };
    BossHit_UpdateAndRender(
        &b->hit, c->hit, &b->hp, &b->boss_palette, colors, 4, 7000,
        BossEntity_HitTestOrb(e, c->hit->orb), box);
}
static void M_Next(B15M *const b, const int8_t phase, const int16_t pattern)
{
    b->phase = phase;
    b->frame = 0;
    b->hit.frame = 0;
    b->pattern = pattern;
    b->teleport_done = false;
}
static void M_Common(B15M *const b)
{
    b->frame = M_W(b->frame + 1);
    b->hit.frame = M_W(b->hit.frame + 1);
    if (b->phase != B15M_PHASE_PATTERNS_1) {
        M_Sync(b);
    }
}
static int16_t M_SurroundX(
    const B15M_CONTEXT *const c, const BOSS_ENTITY *const e, const bool bat)
{
    return M_W(e->left + M_Random(c) % 192 - (bat ? 72 : 32));
}
static int16_t M_SurroundY(
    const B15M_CONTEXT *const c, const BOSS_ENTITY *const e, const bool bat)
{
    return M_W(e->top + M_Random(c) % 128 - (bat ? 48 : 32));
}
static void M_Rifts(
    B15M *const b, const B15M_CONTEXT *const c, B15M_RIFTS *const r,
    const bool bat, const int16_t start, const int16_t end,
    const bool random_color)
{
    if (b->frame < start || b->frame > end || b->frame % 4 != 0) {
        return;
    }
    BOSS_ENTITY *const e = bat ? &b->bat : &b->girl;
    for (int32_t i = 0; i < 5; i++) {
        if (b->frame > start) {
            M_GrcErase(c, r->x[i], r->y[i]);
        }
        if (b->frame % 16 == (i * 4) % 16 || b->frame == start) {
            r->x[i] = M_SurroundX(c, e, bat);
            r->y[i] = M_SurroundY(c, e, bat);
        }
        if (b->frame < end) {
            const int16_t image = 4 + M_Random(c) % 2;
            const uint8_t color =
                random_color ? (uint8_t)(M_Random(c) % 16) : 4;
            M_Grc(c, r->x[i], r->y[i], image, color);
        }
    }
    Boss_Effect(c->hit, 7);
}
static bool M_Teleport(
    B15M *const b, const B15M_CONTEXT *const c, const int16_t x,
    const int16_t y)
{
    M_Sync(b);
    switch (b->frame) {
    case 20:
        Video_SelectDrawingPage(1);
        M_Background(b, c);
        Video_SelectDrawingPage(0);
        M_Unlock(&b->girl, c, 2, false);
        b->girl.orb_inactive = true;
        break;
    case 28:
        M_Background(b, c);
        M_Unlock(&b->girl, c, 3, false);
        b->girl.orb_inactive = true;
        break;
    case 36:
        b->girl.orb_inactive = true;
        M_Background(b, c);
        M_Unlock(&b->girl, c, 4, false);
        break;
    case 44:
        b->girl.orb_inactive = true;
        Sprite_Restore16(b->girl.left, b->girl.top, 128, 96);
        break;
    case 52:
        b->girl.orb_inactive = true;
        b->girl.left = x;
        b->girl.top = y;
        M_Snap(b, c);
        M_Background(b, c);
        M_Unlock(&b->girl, c, 4, false);
        break;
    case 60:
        b->girl.orb_inactive = true;
        M_Background(b, c);
        M_Unlock(&b->girl, c, 3, false);
        break;
    case 68:
        b->girl.orb_inactive = true;
        M_Background(b, c);
        M_Unlock(&b->girl, c, 2, false);
        break;
    case 76:
        b->girl.orb_inactive = false;
        M_PutBoth(&b->girl, c, 0);
        break;
    default:
        if (b->frame > 80) {
            b->frame = 0;
            return true;
        }
        break;
    }
    if (b->frame % 4 != 0) {
        return false;
    }
    for (int32_t i = 0; i < 5; i++) {
        if (b->frame > 4) {
            Sprite_Restore16(b->stars.x[i], b->stars.y[i], 16, 8);
        }
        const int16_t tx = M_SurroundX(c, &b->girl, false),
                      ty = M_SurroundY(c, &b->girl, false);
        if (b->frame < 40 || b->frame > 52) {
            b->stars.x[i] = tx;
            b->stars.y[i] = ty;
        } else {
            b->stars.x[i] = M_W(b->stars.x[i] + M_W(tx - b->stars.x[i]) / 3);
            b->stars.y[i] = M_W(b->stars.y[i] + M_W(ty - b->stars.y[i]) / 3);
        }
        if (b->frame < 68) {
            M_Star8(c, b->stars.x[i], b->stars.y[i]);
        }
    }
    return false;
}
static void M_TrailCopy(
    B15M *const b, const int32_t i, const int16_t x, const int16_t y)
{
    for (int32_t t = 2; t > 0; t--) {
        b->trail_x[i][t] = b->trail_x[i][t - 1];
        b->trail_y[i][t] = b->trail_y[i][t - 1];
    }
    b->trail_x[i][0] = x;
    b->trail_y[i][0] = y;
}
static void M_Trail(
    B15M *const b, const B15M_CONTEXT *const c, const int32_t i,
    const int16_t x, const int16_t y, const int16_t valid)
{
    if (valid > 1) {
        M_GrcErase(c, b->trail_x[i][0], b->trail_y[i][0]);
    }
    M_Grc(c, x, y, 3, 8);
    if (valid > 1) {
        if (valid > 2) {
            M_GrcErase(c, b->trail_x[i][1], b->trail_y[i][1]);
        }
        M_Grc(c, b->trail_x[i][0], b->trail_y[i][0], 2, 8);
    }
    if (valid > 2) {
        if (valid > 3) {
            M_GrcErase(c, b->trail_x[i][2], b->trail_y[i][2]);
        }
        M_Grc(c, b->trail_x[i][1], b->trail_y[i][1], 1, 8);
    }
}
static bool M_Entrance(B15M *const b, const B15M_CONTEXT *const c)
{
    if (!b->entrance_active) {
        b->entrance_active = true;
        b->entrance_stage = 0;
        b->entrance_angle = 0;
        b->hit.frame = 0;
        b->boss_palette = c->hit->palette->saved;
        M_Snap(b, c);
    }
    if (b->entrance_waiting) {
        if (!Frame_DelayReady((uint16_t)b->pending_scroll)) {
            return false;
        }
        b->entrance_waiting = false;
    }
    for (;;) {
        b->hit.frame = M_W(b->hit.frame + 1);
        if (b->entrance_stage == 0) {
            for (int32_t i = 0; i < 2; i++) {
                const int16_t x =
                    Math16_PolarX(304, 96, b->entrance_angle + i * 128);
                const int16_t y =
                    Math16_PolarY(114, 96, b->entrance_angle + i * 128);
                M_Trail(b, c, i, x, y, b->hit.frame);
                M_TrailCopy(b, i, x, y);
            }
            if (b->hit.frame >= 120) {
                b->entrance_stage = 1;
                b->hit.frame = 0;
                b->trails_offscreen = false;
                continue;
            }
            b->entrance_angle = (uint8_t)(b->entrance_angle + 24);
            b->pending_scroll = (120 - b->hit.frame) / 12 + 1;
        } else {
            const int16_t f = b->hit.frame;
            if (!b->trails_offscreen && f % 4 == 0) {
                for (int32_t i = 0; i < 2; i++) {
                    const int16_t x = M_W(b->trail_x[i][0] + 64 - i * 128),
                                  y = b->trail_y[i][0];
                    M_Trail(b, c, i, x, y, 4);
                    if (b->trail_x[0][2] > 640 && b->trail_x[1][2] < -32) {
                        b->trails_offscreen = true;
                    }
                    M_TrailCopy(b, i, x, y);
                }
            }
            if (f > 20 && f < 100) {
                if (f % 4 == 0) {
                    if (f != 24) {
                        Wave_Restore(
                            b->girl.left, b->girl.top, f - 2, 104 - f,
                            (f - 4) * 6, 144, 96);
                    }
                    BossEntity_WavePut(
                        &b->girl, c->sprites, b->girl.left, b->girl.top, 0,
                        f + 2, 100 - f, f * 6);
                }
            } else if (f == 100) {
                Wave_Restore(b->girl.left, b->girl.top, 102, 0, 100, 144, 96);
                M_Unlock(&b->girl, c, 0, true);
            } else if (f == 128) {
                M_Background(b, c);
                BossEntity_WavePut(
                    &b->girl, c->sprites, b->girl.left, b->girl.top, 0, 3, 8,
                    64);
            } else if (f == 130) {
                Wave_Restore(b->girl.left, b->girl.top, 3, 8, 64, 144, 96);
                M_Unlock(&b->girl, c, 0, true);
            } else if (f > 140) {
                b->frame = 0;
                b->hit.frame = 0;
                b->phase = B15M_PHASE_PATTERNS_1;
                b->girl.orb_inactive = false;
                M_PutBoth(&b->girl, c, b->girl.image);
                b->teleport_done = false;
                b->pattern = 1;
                b->hp_bar_filled = false;
                b->entrance_active = false;
                return true;
            }
            b->pending_scroll = 1;
        }
        if (!Frame_DelayReady((uint16_t)b->pending_scroll)) {
            b->entrance_waiting = true;
            return false;
        }
    }
}

static int16_t M_LasersAcross(B15M *const b, const B15M_CONTEXT *const c)
{
    const int16_t x = M_CenterX(b, B15M_FORM_GIRL), y = M_W(b->girl.top + 32);
    if (b->frame == 50) {
        b->direction = M_Random(c) % 2;
        M_Both(b, c, &b->girl, 1, false);
        b->pattern_state = Boss_Rank(50, 55, 60, 65);
    } else if (b->frame == 60) {
        M_Circle(c, x, y, 32, 4, 2, 0, 255);
        b->circle_radius = 32;
    } else if (b->frame > 60 && b->frame < 120 && b->frame % 2 == 0) {
        M_EraseCircle(c, x, y, b->circle_radius, 2);
        b->circle_radius = M_W(b->circle_radius + 8);
        M_Circle(
            c, x, y, b->circle_radius, 4,
            b->frame < 100       ? 2
                : b->frame < 110 ? 8
                                 : 32,
            0, 255);
    } else if (b->frame == 120) {
        M_EraseCircle(c, x, y, b->circle_radius, 32);
    } else if (b->frame == 150) {
        M_Both(b, c, &b->girl, 0, false);
    }
    if (b->frame >= 70 && b->frame % 10 == 0) {
        const int16_t tx = b->direction == 0
            ? M_W((b->frame - 70) / 10 * 64)
            : M_W(640 - (b->frame - 70) / 10 * 64);
        M_Laser(
            c, b->frame / 10, M_W(b->girl.left + 60), M_W(b->girl.top + 28), tx,
            400, b->pattern_state, 25, 4);
        Boss_Effect(c->hit, 6);
        if ((b->direction == 0 && tx >= 640)
            || (b->direction == 1 && tx <= 0)) {
            b->frame = 0;
            return 0;
        }
    }
    return 1;
}
static int16_t M_RandomMissiles(B15M *const b, const B15M_CONTEXT *const c)
{
    if (b->frame == 50) {
        M_Both(b, c, &b->girl, 1, false);
        b->pattern_state = Boss_Rank(15, 21, 25, 29);
    }
    if (b->frame > 60 && b->frame % 3 == 0) {
        const int32_t i = M_Random(c) % 5;
        const uint8_t angle = (uint8_t)(M_Random(c) % b->pattern_state
                                        - (b->pattern_state - 1) / 2 + 64);
        const MATH16_VECTOR v = Math16_Vector(7, angle);
        Missiles_Add(
            c->missiles, b->missile_rifts.x[i], b->missile_rifts.y[i], v.x, v.y,
            0);
    }
    M_Rifts(b, c, &b->missile_rifts, false, 60, 160, false);
    if (b->frame > 170) {
        b->frame = 0;
        return 0;
    }
    return 2;
}
static int16_t M_CirclePellets(B15M *const b, const B15M_CONTEXT *const c)
{
    if (b->frame < 10) {
        b->circle.frame = 0;
    }
    if (b->frame == 50) {
        M_Both(b, c, &b->girl, 1, false);
        b->circle.angle = 0;
        b->pattern_state = Boss_Rank(
            PELLET_GROUP_SINGLE_AIMED, PELLET_GROUP_RANDOM_NARROW_AIMED,
            PELLET_GROUP_THREE_WIDE_AIMED, PELLET_GROUP_FIVE_NARROW_AIMED);
    }
    if (M_SummonFlash(b, c, &b->circle, 60, 0)) {
        M_EraseBigCircle(b, c);
        b->circle.angle = 0;
        for (int32_t i = 0; i < 32; i++) {
            const int16_t x =
                Math16_PolarX(M_W(b->girl.left + 60), 128, b->circle.angle);
            const int16_t y = Math16_PolarY(
                M_CenterY(b, B15M_FORM_GIRL), 128, b->circle.angle);
            M_Group(c, x, y, (PELLET_GROUP)b->pattern_state, 4);
            b->circle.angle = (uint8_t)(b->circle.angle + 8);
        }
        b->frame = 0;
        b->circle.frame = 0;
        return 0;
    }
    return 3;
}
static int16_t M_Phase1(
    B15M *const b, const B15M_CONTEXT *const c, const int16_t id)
{
    switch (id) {
    case 0:
        return M_Random(c) % 4;
    case 1:
        return M_LasersAcross(b, c);
    case 2:
        return M_RandomMissiles(b, c);
    case 3:
        return M_CirclePellets(b, c);
    default:
        return 0;
    }
}
static void M_StarLines(
    B15M *const b, const B15M_CONTEXT *const c, const bool erase,
    const uint8_t color)
{
    const uint8_t pairs[6][2] = { { 192, 22 }, { 192, 106 }, { 22, 106 },
                                  { 64, 150 }, { 64, 234 },  { 150, 234 } };
    for (int32_t i = 0; i < 6; i++) {
        const int16_t x0 =
            Math16_PolarX(M_CenterX(b, B15M_FORM_GIRL), 128, pairs[i][0]);
        const int16_t y0 =
            Math16_PolarY(M_CenterY(b, B15M_FORM_GIRL), 128, pairs[i][0]);
        const int16_t x1 =
            Math16_PolarX(M_CenterX(b, B15M_FORM_GIRL), 128, pairs[i][1]);
        const int16_t y1 =
            Math16_PolarY(M_CenterY(b, B15M_FORM_GIRL), 128, pairs[i][1]);
        if (erase) {
            Video_LineErase(x0, y0, x1, y1);
        } else {
            Video_Line(x0, y0, x1, y1, color);
        }
    }
}
static int16_t M_Star(B15M *const b, const B15M_CONTEXT *const c)
{
    B15M_CIRCLE *const circle = &b->star_circle;
    if (b->frame < 5) {
        circle->frame = 0;
        circle->angle = 0;
    }
    if (b->frame < 10) {
        return 0;
    }
    if (b->frame == 10) {
        M_Both(b, c, &b->attack, 0, true);
        circle->angle = 0;
        circle->frame = 0;
    }
    if (b->frame >= 10 && b->frame % 2 == 0 && circle->frame == 0) {
        if (b->frame == 10) {
            Boss_Effect(c->hit, 8);
        }
        M_SummonDots(
            c, circle, M_CenterX(b, B15M_FORM_GIRL),
            M_CenterY(b, B15M_FORM_GIRL), 2);
        if (circle->angle >= 64) {
            circle->frame = 1;
            M_EraseBigCircle(b, c);
            M_BigCircle(b, c, 7);
        }
    } else if (circle->angle >= 64) {
        circle->frame = M_W(circle->frame + 1);
        if (circle->frame == 10) {
            M_StarLines(b, c, false, 7);
        }
        if (circle->frame > 20 && circle->frame % 4 == 0) {
            M_StarLines(b, c, false, 4);
            M_BigCircle(b, c, 4);
        } else if (circle->frame > 20 && circle->frame % 4 == 2) {
            M_StarLines(b, c, false, 7);
            M_BigCircle(b, c, 7);
        }
        if (circle->frame > 60) {
            M_StarLines(b, c, true, 0);
            M_EraseBigCircle(b, c);
            b->frame = 0;
            circle->angle = 0;
            circle->frame = 0;
            return 1;
        }
    }
    return 0;
}
static int16_t M_CurvedRings(B15M *const b, const B15M_CONTEXT *const c)
{
    M_Attack(b, c);
    int16_t offset = -1, speed = 0;
    if (b->frame == 10) {
        b->pattern_state = Boss_Rank(14, 16, 18, 20);
        offset = 0;
        speed = 48;
    } else if (b->frame == 16) {
        offset = 2;
        speed = 54;
    } else if (b->frame == 24) {
        offset = 4;
        speed = 64;
    } else if (b->frame == 32) {
        offset = 6;
        speed = 72;
    } else if (b->frame == 40) {
        offset = 8;
        speed = 80;
    } else if (b->frame > 60) {
        b->frame = 0;
        Video_SelectDrawingPage(1);
        M_Background(b, c);
        return 0;
    }
    if (offset >= 0) {
        for (int32_t i = 0; i < b->pattern_state; i++) {
            M_Single(
                c, M_W(M_CenterX(b, B15M_FORM_GIRL) - 4),
                M_W(M_CenterY(b, B15M_FORM_GIRL) - 4),
                (uint8_t)((256 / b->pattern_state) * i + offset), speed,
                PELLET_REGULAR, 0);
        }
    }
    return 1;
}
static int16_t M_SphereClusters(B15M *const b, const B15M_CONTEXT *const c)
{
    M_Attack(b, c);
    if (b->frame <= 100 && b->frame % 10 == 0) {
        const int32_t n = b->frame / 10;
        const int16_t column =
            M_W((b->girl.left < 256 ? 608 : 0) - b->girl.left) / 10;
        b->sphere_x[n - 1] =
            M_W(n * column + M_CenterX(b, B15M_FORM_GIRL) - 16);
        const int16_t row = M_W(M_Random(c) % 200 + 200 - b->girl.top) / 10;
        b->sphere_y[n - 1] = M_W(n * row + M_CenterY(b, B15M_FORM_GIRL) - 16);
        for (int32_t i = 0; i < n; i++) {
            M_Grc(c, b->sphere_x[i], b->sphere_y[i], 3, 4);
        }
    } else if (b->frame > 100 && b->frame <= 230 && b->frame % 10 == 0) {
        if (b->frame <= 200) {
            const int32_t n = (b->frame - 100) / 10;
            Boss_Effect(c->hit, 7);
            M_Grc(c, b->sphere_x[n - 1], b->sphere_y[n - 1], 4, 4);
            for (int32_t i = n; i < 10; i++) {
                M_Grc(c, b->sphere_x[i], b->sphere_y[i], 3, 4);
            }
            b->pattern_state = Boss_Rank(5, 7, 9, 13);
            for (int32_t i = 0; i < b->pattern_state; i++) {
                const uint8_t angle = M_Random(c) & 127;
                const int16_t speed = (M_Random(c) + 24) & 63;
                M_Single(
                    c, b->sphere_x[n - 1], b->sphere_y[n - 1], angle, speed,
                    PELLET_REGULAR, 0);
            }
        }
        if (b->frame > 130) {
            const int32_t i = (b->frame - 130) / 10 - 1;
            M_GrcErase(c, b->sphere_x[i], b->sphere_y[i]);
        }
    } else if (b->frame > 250) {
        b->frame = 0;
        return 0;
    }
    return 2;
}
static int16_t M_RandomRifts(B15M *const b, const B15M_CONTEXT *const c)
{
    M_Attack(b, c);
    if (b->frame == 30) {
        b->pattern_state = Boss_Rank(6, 3, 2, 2);
    }
    if (b->frame > 60 && b->frame < 120 && b->frame % b->pattern_state == 0) {
        const int32_t i = M_Random(c) % 5;
        const uint8_t angle = (uint8_t)(M_Random(c) % 21 + 54);
        M_Single(
            c, b->random_rifts.x[i], b->random_rifts.y[i], angle, 96,
            PELLET_REGULAR, 0);
    } else if (b->frame >= 120 && b->frame < 200) {
        const int32_t i = M_Random(c) % 5;
        uint8_t angle = (uint8_t)(M_Random(c) % 21 + 54);
        const uint8_t offset =
            (uint8_t)((M_Random(c) & 1) ? b->frame - 120 : -(b->frame - 120));
        angle = (uint8_t)(angle * (offset / 2));
        M_Single(
            c, b->random_rifts.x[i], b->random_rifts.y[i], angle, 96,
            PELLET_REGULAR, 0);
    }
    M_Rifts(b, c, &b->random_rifts, false, 60, 200, false);
    if (b->frame > 220) {
        b->frame = 0;
        return 0;
    }
    return 3;
}
static int16_t M_Phase3(
    B15M *const b, const B15M_CONTEXT *const c, const int16_t id)
{
    if (id == 99) {
        b->phase3_pattern = 0;
        return 0;
    }
    if (id == 0) {
        return M_Random(c) % 4;
    }
    if (id < 1 || id > 3) {
        return 0;
    }
    if (b->phase3_pattern == 0) {
        b->phase3_pattern = M_Star(b, c);
        return id;
    }
    b->phase3_pattern = id == 1 ? M_CurvedRings(b, c)
        : id == 2               ? M_SphereClusters(b, c)
                                : M_RandomRifts(b, c);
    return b->phase3_pattern;
}

static bool M_ScrollWait(B15M *const b, const int16_t top)
{
    b->pending = B15M_PENDING_SCROLL;
    b->pending_scroll = top;
    Frame_BeginWait(&b->wait);
    return false;
}
static void M_TransformResult(B15M *const b)
{
    if (b->call_phase == B15M_PHASE_TRANSFORM) {
        b->pattern = b->transform_bat ? 1 : 0;
    } else {
        b->form = b->transform_bat ? B15M_FORM_BAT : B15M_FORM_GIRL;
    }
}
static bool M_TransformFinish(B15M *const b, const B15M_CONTEXT *const c)
{
    if (b->frame <= 100) {
        if (b->call_phase == B15M_PHASE_TRANSFORM) {
            b->pattern = b->transform_bat ? 0 : 1;
        } else {
            b->form = b->transform_bat ? B15M_FORM_GIRL : B15M_FORM_BAT;
        }
        return true;
    }
    if (b->transform_bat) {
        b->bat.left = M_W(b->girl.left + 40);
        b->bat.top = M_W(b->girl.top + 32);
        b->bat.image = 0;
        Video_SelectDrawingPage(1);
        M_Background(b, c);
        Video_SelectDrawingPage(0);
        M_Background(b, c);
    } else {
        Sprite_Restore16(b->bat.left, b->bat.top, 48, 32);
        int16_t x = M_W(b->bat.left - 40), y = M_W(b->bat.top - 32);
        if (x < 0) {
            x = 0;
        } else if (x > 512) {
            x = 512;
        }
        if (y < 64) {
            y = 64;
        }
        b->girl.left = x;
        b->girl.top = y;
        M_Snap(b, c);
        M_Both(b, c, &b->attack, 1, true);
    }
    b->transform_stage = B15M_TRANSFORM_SWAPPED;
    return M_ScrollWait(b, 400);
}
static bool M_Transform(
    B15M *const b, const B15M_CONTEXT *const c, const bool bat)
{
    b->transform_bat = bat;
    b->transform_stage = B15M_TRANSFORM_RIFTS;
    if (bat && b->frame == 20) {
        M_Both(b, c, &b->attack, 0, true);
    }
    M_Rifts(b, c, bat ? &b->girl_rifts : &b->bat_rifts, !bat, 20, 100, true);
    if (b->frame >= 20 && b->frame % 2 == 0) {
        return M_ScrollWait(b, M_W(400 - b->frame % 4 * 2));
    }
    return M_TransformFinish(b, c);
}
static int16_t M_Abs(const int16_t value)
{
    return value < 0 ? M_W(-(int32_t)value) : value;
}
static int16_t M_BatFly(B15M *const b, const B15M_CONTEXT *const c)
{
    if (b->frame < 3) {
        b->bat_velocity_x = 0;
        b->bat_velocity_y = 0;
        return 0;
    }
    if (b->frame == 3) {
        do {
            b->bat_target_x = M_RandomLeft(c);
            b->bat_target_y = M_W(M_RandomTop(c) + 32);
        } while (M_Abs(M_W(b->bat.left - b->bat_target_x)) < 128
                 && M_Abs(M_W(b->bat.top - b->bat_target_y)) < 64);
        const MATH16_VECTOR v = Math16_Aim(
            (MATH16_VECTOR) { b->bat.left, b->bat.top },
            (MATH16_VECTOR) { b->bat_target_x, b->bat_target_y }, 2, 0);
        b->bat_velocity_x = v.x;
        b->bat_velocity_y = v.y;
        b->bat_target_frames = M_Abs(M_W(b->bat_target_x - b->bat.left)) / 2;
    }
    b->bat_velocity_y =
        M_W(b->bat_velocity_y + ((int32_t)M_Random(c) % 5 - 2) / 10);
    if (b->frame / 3 >= b->bat_target_frames) {
        b->bat_velocity_x = 0;
        b->bat_velocity_y = 0;
        b->frame = 0;
        return 1;
    }
    return 0;
}
static void M_BatPattern(B15M *const b, const B15M_CONTEXT *const c)
{
    switch (b->bat_pattern) {
    case 1:
        if (b->frame % 16 == 0) {
            M_FormGroup(
                b, c, B15M_FORM_BAT,
                (PELLET_GROUP)Boss_Rank(
                    PELLET_GROUP_THREE_WIDE_AIMED, PELLET_GROUP_FIVE_WIDE_AIMED,
                    PELLET_GROUP_FIVE_NARROW_AIMED, PELLET_GROUP_FIVE_NARROW),
                56);
        }
        break;
    case 2:
        if (b->frame % 4 == 0) {
            const PELLET_GROUP group = (M_Random(c) & 1)
                ? PELLET_GROUP_TWO_NARROW_AIMED
                : PELLET_GROUP_TWO_WIDE_AIMED;
            M_FormGroup(b, c, B15M_FORM_BAT, group, 88);
        }
        break;
    case 3:
        if (b->frame % 4 == 0) {
            const uint8_t angle = (uint8_t)M_Random(c);
            M_Single(
                c, M_W(M_CenterX(b, B15M_FORM_BAT) - 4),
                M_W(M_CenterY(b, B15M_FORM_BAT) - 4), angle, 2, PELLET_GRAVITY,
                1);
        }
        break;
    case 4:
        if (b->frame % 50 < 10 && b->frame % 4 == 0) {
            M_FormGroup(b, c, 1, PELLET_GROUP_TWO_NARROW_AIMED, 85);
        }
        if (b->frame % 50 >= 25 && b->frame % 50 < 35 && b->frame % 4 == 0) {
            M_FormGroup(b, c, 1, PELLET_GROUP_THREE_NARROW_AIMED, 85);
        }
        break;
    default:
        break;
    }
}
static void M_SymmetricFire(
    B15M *const b, const B15M_CONTEXT *const c, const uint8_t angle,
    const int16_t speed)
{
    const int16_t y = M_W(M_CenterY(b, B15M_FORM_GIRL) - 8);
    M_Single(
        c, M_W(M_CenterX(b, B15M_FORM_GIRL) - 8), y, (uint8_t)(128 - angle),
        speed, PELLET_REGULAR, 0);
    M_Single(
        c, M_CenterX(b, B15M_FORM_GIRL), y, angle, speed, PELLET_REGULAR, 0);
}
static int16_t M_SymmetricArc(B15M *const b, const B15M_CONTEXT *const c)
{
    M_Attack(b, c);
    if (b->frame == 50) {
        b->pattern_state = Boss_Rank(2, 3, 3, 4);
        for (int32_t i = 0; i < 4; i++) {
            M_SymmetricFire(b, c, 64, M_W((i + b->pattern_state) * 16));
        }
    } else if (b->frame == 60 || b->frame == 70) {
        const uint8_t angle = b->frame == 60 ? 48 : 24;
        for (int32_t i = 0; i < 4; i++) {
            M_SymmetricFire(b, c, angle, M_W((i + b->pattern_state) * 16));
        }
        Boss_Effect(c->hit, 7);
    } else if (b->frame == 80) {
        for (int32_t i = 0; i < 10; i++) {
            M_SymmetricFire(
                b, c, (uint8_t)(i * 6), M_W((2 + b->pattern_state) * 16));
        }
        Boss_Effect(c->hit, 7);
    } else if (b->frame >= 120) {
        b->frame = 0;
        return 0;
    }
    return 1;
}
static void M_SafetyAfterHit(B15M *const b, const B15M_CONTEXT *const c)
{
    if (Boss_Player(c->hit)->is_hit) {
        b->safety_circle.frame = 200;
    }
    if (b->frame % b->pattern_state == 0) {
        const int16_t x = M_Random(c) % 632;
        M_Group(
            c, x, 64,
            c->hit->display->rank == 3 ? PELLET_GROUP_RANDOM_WIDE
                                       : PELLET_GROUP_SINGLE,
            72);
    }
    if (b->safety_circle.frame > 180 && b->safety_circle.frame % 4 == 0) {
        M_FormGroup(b, c, 0, PELLET_GROUP_SINGLE_AIMED, 72);
    }
}
static int16_t M_Safety(
    B15M *const b, const B15M_CONTEXT *const c, const uint64_t host_tick)
{
    B15M_CIRCLE *const circle = &b->safety_circle;
    if (b->frame < 10) {
        circle->frame = 0;
    }
    M_Attack(b, c);
    if (b->frame < 60) {
        return 1;
    }
    if (b->frame == 60) {
        b->pattern_state = Boss_Rank(4, 2, 2, 2);
        circle->angle = 0;
    }
    if (b->frame >= 60 && b->frame % 2 == 0 && circle->frame == 0) {
        if (b->frame == 60) {
            Boss_Effect(c->hit, 8);
            b->safety_left = Boss_Player(c->hit)->left;
        }
        M_SummonDots(c, circle, M_W(b->safety_left + 16), 384, 2);
        if (circle->angle >= 64) {
            M_EraseCircle(c, M_W(b->safety_left + 16), 384, 128, 1);
            M_Circle(c, M_W(b->safety_left + 16), 384, 128, 7, 1, 0, 255);
            circle->frame = 1;
            M_Both(b, c, &b->girl, 0, true);
        }
    } else if (circle->frame != 0 && circle->frame < 200) {
        circle->frame = M_W(circle->frame + 1);
        if (circle->frame % 4 == 0) {
            M_EraseCircle(c, M_W(b->safety_left + 16), 384, 128, 1);
            M_Circle(c, M_W(b->safety_left + 16), 384, 128, 7, 1, 0, 255);
        }
        if (!Boss_Player(c->hit)->invincible
            && (M_W(b->safety_left + 104) <= Boss_Player(c->hit)->left
                || M_W(b->safety_left - 128) >= Boss_Player(c->hit)->left)) {
            b->pending = B15M_PENDING_SAFETY;
            b->pending_tick = host_tick;
            return -1;
        }
        M_SafetyAfterHit(b, c);
    } else if (circle->frame < 200) {
        return 1;
    } else {
        if (circle->frame == 200) {
            M_EraseCircle(c, M_W(b->safety_left + 16), 384, 128, 1);
        }
        circle->frame = M_W(circle->frame + 1);
        if (circle->frame % 8 == 0) {
            M_Group(
                c, M_W(b->bat.left + 60), M_W(b->bat.top + 44),
                PELLET_GROUP_SINGLE_AIMED, 72);
        }
        if (circle->frame > 260) {
            b->frame = 0;
            circle->frame = 0;
            circle->angle = 0;
            return 0;
        }
    }
    return 1;
}
static int16_t M_AimedRing(B15M *const b, const B15M_CONTEXT *const c)
{
    M_Attack(b, c);
    if (b->frame < 50) {
        return 1;
    }
    if (b->frame == 50) {
        M_Both(b, c, &b->girl, 1, false);
        b->aimed_circle.angle = 0;
        b->pattern_state = Boss_Rank(40, 30, 20, 15);
    }
    if (M_SummonFlash(b, c, &b->aimed_circle, 60, 2)) {
        M_EraseBigCircle(b, c);
        b->aimed_circle.angle = 0;
        for (int32_t i = 0; i < 32; i++) {
            const int16_t x = Math16_PolarX(
                M_CenterX(b, B15M_FORM_GIRL), 128, b->aimed_circle.angle);
            const int16_t y = Math16_PolarY(
                M_W(b->girl.top + 28), 128, b->aimed_circle.angle);
            M_Group(c, x, y, PELLET_GROUP_SINGLE, 64);
            b->aimed_circle.angle = (uint8_t)(b->aimed_circle.angle + 8);
        }
        b->frame = 0;
        b->aimed_circle.frame = 0;
        return 0;
    }
    if (b->frame % b->pattern_state == 0) {
        M_Group(
            c, M_W(M_CenterX(b, B15M_FORM_GIRL) - 42),
            M_W(M_CenterY(b, B15M_FORM_GIRL) - 8), PELLET_GROUP_FIVE_WIDE_AIMED,
            64);
        M_Group(
            c, M_W(M_CenterX(b, B15M_FORM_GIRL) + 36),
            M_W(M_CenterY(b, B15M_FORM_GIRL) - 8), PELLET_GROUP_FIVE_WIDE_AIMED,
            64);
    }
    if (b->frame % (b->pattern_state * 2) == 0) {
        M_Laser(
            c, b->frame / 10, M_W(M_CenterX(b, B15M_FORM_GIRL) - 4),
            M_W(b->girl.top + 28), Boss_Player(c->hit)->left, 400, 50, 20, 8);
        Boss_Effect(c->hit, 6);
    }
    return 1;
}
static int16_t M_GirlPhase5(
    B15M *const b, const B15M_CONTEXT *const c, const uint64_t host_tick)
{
    if (b->girl_pattern == 0) {
        b->girl_pattern = M_Random(c) % 3 + 1;
        return 0;
    }
    if (b->girl_subphase == 0) {
        b->girl_subphase = M_Star(b, c);
        return 0;
    }
    int16_t result = 1;
    switch (b->girl_pattern) {
    case 1:
        result = M_SymmetricArc(b, c);
        break;
    case 2:
        result = M_Safety(b, c, host_tick);
        break;
    case 3:
        result = M_AimedRing(b, c);
        break;
    default:
        break;
    }
    if (result < 0) {
        return -1;
    }
    b->girl_subphase = result;
    if (result == 0) {
        b->girl_pattern = 0;
        return 1;
    }
    return 0;
}
static void M_ResetPhase5(B15M *const b)
{
    b->phase5_subphase = 0;
    b->bat_pattern = 0;
    b->girl_pattern = 0;
    b->girl_subphase = 0;
}
static void M_AfterTransform(B15M *const b)
{
    if (b->call_phase == B15M_PHASE_TRANSFORM) {
        if (b->pattern == 1) {
            M_Next(b, B15M_PHASE_FORMS, 0);
            b->form = B15M_FORM_BAT;
            M_ResetPhase5(b);
        }
    } else if (b->phase5_subphase == 1) {
        if (b->transform_bat) {
            if (b->form == B15M_FORM_BAT) {
                b->phase5_subphase = 0;
            }
        } else if (b->form == B15M_FORM_GIRL) {
            b->bat_pattern = 0;
            b->phase5_subphase = 0;
        }
    }
}
static bool M_Phase5(
    B15M *const b, const B15M_CONTEXT *const c, const uint64_t host_tick)
{
    if (b->form == B15M_FORM_BAT) {
        if (b->phase5_subphase == 0) {
            b->phase5_subphase = M_BatFly(b, c);
            if (b->bat_pattern == 0) {
                b->bat_pattern = M_Random(c) % 4 + 1;
            }
            M_BatPattern(b, c);
        } else if (b->phase5_subphase == 1) {
            if (!M_Transform(b, c, false)) {
                return false;
            }
            M_AfterTransform(b);
        }
    } else {
        if (b->phase5_subphase == 0) {
            const int16_t result = M_GirlPhase5(b, c, host_tick);
            if (result < 0) {
                return false;
            }
            b->phase5_subphase = result;
        } else {
            if (!M_Transform(b, c, true)) {
                return false;
            }
            M_AfterTransform(b);
        }
    }
    return true;
}
static bool M_Death(B15M *const b, const B15M_CONTEXT *const c)
{
    const BOSS_DEFEAT_CONTEXT context = { .palette = c->hit->palette,
                                          .post_defeat = &b->post_defeat,
                                          .miko = c->miko,
                                          .player = Boss_Player(c->hit),
                                          .origin_top = &c->hit->orb->top,
                                          .stage_cleared = c->stage_cleared };
    if (!BossDefeat_Resume(&b->defeat, &context)) {
        return false;
    }
    Scene_Activate(c->scene, &c->assets->next_scene);
    b->defeating = false;
    b->finished = true;
    return true;
}
static bool M_Tail(B15M *const b, const B15M_CONTEXT *const c)
{
    if (b->call_phase == B15M_PHASE_FORMS) {
        M_Hit(b, c, b->form);
        if (b->hp <= 0) {
            c->fade_music(c->hit->user);
            Pellets_EraseAndResetMoving(c->pellets);
            M_Background(b, c);
            Missiles_Reset(c->missiles);
            ShootoutLasers_EraseAndReset(c->lasers, 10);
            BossDefeat_Begin(&b->defeat, BOSS_DEFEAT_REGULAR);
            b->defeating = true;
            return M_Death(b, c);
        }
    }
    return true;
}
bool B15M_UpdateAndRender(
    B15M *const b, const B15M_CONTEXT *const c, const uint64_t host_tick)
{
    if (b->finished) {
        return true;
    }
    if (b->defeating) {
        return M_Death(b, c);
    }
    if (b->pending == B15M_PENDING_SCROLL) {
        if (!Frame_WaitReady(&b->wait)) {
            return false;
        }
        b->pending = B15M_PENDING_NONE;
        Video_SetScroll(b->pending_scroll);
        if (b->transform_stage == B15M_TRANSFORM_RIFTS) {
            if (!M_TransformFinish(b, c)) {
                return false;
            }
        } else {
            b->frame = 0;
            M_TransformResult(b);
        }
        M_AfterTransform(b);
        return M_Tail(b, c);
    }
    if (b->pending == B15M_PENDING_SAFETY) {
        ASSERT(host_tick >= b->pending_tick);
        if (host_tick - b->pending_tick < 6) {
            return false;
        }
        b->pending = B15M_PENDING_NONE;
        Boss_Player(c->hit)->is_hit = true;
        M_SafetyAfterHit(b, c);
        b->girl_subphase = 1;
        b->phase5_subphase = 0;
        return M_Tail(b, c);
    }
    if (b->entrance_active) {
        return M_Entrance(b, c);
    }
    b->call_phase = b->phase;
    Missiles_UpdateAndRender(
        c->missiles, Boss_Player(c->hit), (uint16_t)c->hit->frame_rand,
        (MISSILES_OUTPUT) { M_MissileAction, (void *)c });
    if (b->phase == B15M_PHASE_ENTRANCE) {
        return M_Entrance(b, c);
    }
    if (b->phase == B15M_PHASE_PATTERNS_1
        || b->phase == B15M_PHASE_PATTERNS_2) {
        if (b->phase == B15M_PHASE_PATTERNS_1 && !b->hp_bar_filled) {
            b->hp_bar_filled = HudHp_Increment(c->hit->health, b->hp, b->frame);
        }
        M_Common(b);
        if (b->pattern != 0) {
            b->pattern = b->phase == B15M_PHASE_PATTERNS_1
                ? M_Phase1(b, c, b->pattern)
                : M_Phase3(b, c, b->pattern);
        } else if (!b->teleport_done) {
            const int16_t top = M_RandomTop(c);
            const int16_t left = M_RandomLeft(c);
            b->teleport_done = M_Teleport(b, c, left, top);
        } else {
            b->pattern = b->phase == B15M_PHASE_PATTERNS_1 ? M_Phase1(b, c, 0)
                                                           : M_Phase3(b, c, 0);
            b->teleport_done = false;
        }
        M_Hit(b, c, B15M_FORM_GIRL);
        if (!b->hit.invincible
            && b->hp <= (b->phase == B15M_PHASE_PATTERNS_1 ? 10 : 6)
            && b->teleport_done) {
            if (b->phase == B15M_PHASE_PATTERNS_1) {
                M_Next(b, B15M_PHASE_CENTER, 0);
            } else {
                M_Next(b, B15M_PHASE_TRANSFORM, 0);
                b->form = B15M_FORM_GIRL;
            }
        }
    } else if (b->phase == B15M_PHASE_CENTER) {
        b->frame = M_W(b->frame + 1);
        b->teleport_done = M_Teleport(b, c, 256, 96);
        if (b->teleport_done) {
            b->phase3_pattern = 0;
            M_Next(b, B15M_PHASE_PATTERNS_2, 1);
        }
    } else if (b->phase == B15M_PHASE_TRANSFORM) {
        b->frame = M_W(b->frame + 1);
        if (!b->teleport_done) {
            b->teleport_done = M_Teleport(b, c, 256, 96);
        } else {
            if (!M_Transform(b, c, true)) {
                return false;
            }
        }
        M_AfterTransform(b);
    } else if (b->phase == B15M_PHASE_FORMS) {
        M_Common(b);
        if (b->form != B15M_FORM_GIRL) {
            BossEntity_LockedMovePut(
                &b->bat, c->sprites, b->bat_velocity_x, b->bat_velocity_y, 2,
                true);
            if (b->frame % 9 == 0) {
                b->bat.image = 0;
            } else if (b->frame % 9 == 3) {
                b->bat.image = 1;
            } else if (b->frame % 9 == 6) {
                b->bat.image = 2;
            }
        }
        if (!M_Phase5(b, c, host_tick)) {
            return false;
        }
    }
    return M_Tail(b, c);
}
