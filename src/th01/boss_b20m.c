#include "th01/boss_b20m.h"
#include "th01/gameplay.h"
#include "th01/session.h"
#include "th01/sound.h"
#include "th01/rng.h"

#include "th01/boss.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/assert.h"
#include "th01/finite.h"
#include "th01/shape.h"
#include "th01/sprite.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

B20M g_B20M;

typedef enum {
    B20M_WAIT_NONE,
    B20M_WAIT_SCROLL,
    B20M_WAIT_DELAY,
    B20M_WAIT_TICKS,
} B20M_WAIT;

typedef enum {
    B20M_ENTRANCE_DONE,
    B20M_ENTRANCE_SCROLL,
    B20M_ENTRANCE_SCROLL_STEP,
    B20M_ENTRANCE_DIAGONAL = 4,
    B20M_ENTRANCE_DIAGONAL_STEP,
} B20M_ENTRANCE;

typedef enum {
    B20M_TRANSFORM_SHAKE = 1,
    B20M_TRANSFORM_SHAKE_STEP,
    B20M_TRANSFORM_SECOND_FORM,
} B20M_TRANSFORM;

typedef enum {
    B20M_DEFEAT_SHAKE = 1,
    B20M_DEFEAT_SHAKE_STEP,
    B20M_DEFEAT_BONUS,
} B20M_DEFEAT;

static RESULT M_LoadGrc(
    VFS *const vfs, const char *const name, const ALLOCATOR *const a,
    GRC *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Grc_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}
static RESULT M_LoadLeaf(
    VFS *const vfs, const char *const name, const ALLOCATOR *const a,
    const uint8_t **const out)
{
    uint8_t *leaf;
    size_t size;
    MUST(SpriteSheet_Load(vfs, name, 8, 8, a, &leaf, &size));
    FAIL_IF(size != 8, "Invalid %s size", name);
    *out = leaf;
    return OK;
}
static RESULT M_LoadMasks(
    VFS *const vfs, const ALLOCATOR *const a, B20M_ASSETS *const out)
{
    INTERLEAVE_ASSETS *interleave;
    MUST(Interleave_LoadAssets(vfs, a, &interleave));
    uint8_t *shapes;
    size_t size;
    MUST(SpriteSheet_Load(vfs, "shape8x8.bmp", 8, 8, a, &shapes, &size));
    FAIL_IF(size != 7 * 8, "Invalid shape8x8.bmp size");
    MUST(M_LoadLeaf(vfs, "leaf_s.bmp", a, &out->spark));
    MUST(M_LoadLeaf(vfs, "leaf_l.bmp", a, &out->leaf_left));
    MUST(M_LoadLeaf(vfs, "leaf_r.bmp", a, &out->leaf_right));
    out->star = shapes + 8;
    out->flake = shapes + 2 * 8;
    out->interleave = interleave;
    return OK;
}
RESULT B20M_ValidateAssets(const B20M_ASSETS *const a)
{
    FAIL_IF(a == nullptr, "Missing Sariel assets");
    const BOS *const banks[] = { &a->shield, &a->wand, &a->dress };
    const int32_t widths[] = { 48, 96, 96 };
    const int32_t counts[] = { 4, 2, 4 };
    for (int32_t i = 0; i < 3; i++) {
        FAIL_IF(
            banks[i]->pixels == nullptr || banks[i]->coverage == nullptr
                || banks[i]->width != widths[i] || banks[i]->height != widths[i]
                || banks[i]->count != counts[i],
            "Invalid Sariel sprite bank");
    }
    const GRC *const grcs[] = { &a->vortex, &a->bird, &a->cross, &a->splash };
    const int32_t image_counts[] = { 6, 8, 2, 3 };
    for (int32_t i = 0; i < 4; i++) {
        FAIL_IF(
            grcs[i]->coverage == nullptr || grcs[i]->width != 32
                || grcs[i]->height != 32 || grcs[i]->count != image_counts[i],
            "Invalid Sariel monochrome bank");
    }
    const PI_IMAGE *const images[] = { &a->lower,          &a->upper,
                                       &a->backgrounds[0], &a->backgrounds[1],
                                       &a->backgrounds[2], &a->backgrounds[3],
                                       &a->backgrounds[4], &a->backgrounds[5] };
    for (int32_t i = 0; i < 8; i++) {
        FAIL_IF(
            images[i]->pixels == nullptr || images[i]->width != 640
                || images[i]->height != 400 || !images[i]->has_palette,
            "Invalid Sariel background");
    }
    FAIL_IF(
        a->interleave == nullptr || a->star == nullptr || a->flake == nullptr
            || a->spark == nullptr || a->leaf_left == nullptr
            || a->leaf_right == nullptr,
        "Missing Sariel shape masks");
    return OK;
}
RESULT B20M_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const a, B20M_ASSETS *const out)
{
    *out = (B20M_ASSETS) {};
    B20M_ASSETS loaded = {};
    MUST(Boss_LoadBos(vfs, "BOSS6_1.BOS", a, &loaded.shield));
    MUST(Boss_LoadBos(vfs, "BOSS6_2.BOS", a, &loaded.wand));
    MUST(Boss_LoadBos(vfs, "BOSS6_3.BOS", a, &loaded.dress));
    MUST(M_LoadGrc(vfs, "BOSS6GR1.GRC", a, &loaded.vortex));
    MUST(M_LoadGrc(vfs, "BOSS6GR2.GRC", a, &loaded.bird));
    MUST(M_LoadGrc(vfs, "BOSS6GR3.GRC", a, &loaded.cross));
    MUST(M_LoadGrc(vfs, "BOSS6GR4.GRC", a, &loaded.splash));
    MUST(Boss_LoadBackground(vfs, "BOSS6_L.GRP", a, &loaded.lower));
    MUST(Boss_LoadBackground(vfs, "BOSS6_H.GRP", a, &loaded.upper));
    const char *const names[] = { "BOSS6_A1.GRP", "BOSS6_A2.GRP",
                                  "BOSS6_A3.GRP", "BOSS6_A4.GRP",
                                  "BOSS6_A5.GRP", "BOSS6_A6.GRP" };
    for (int32_t i = 0; i < 6; i++) {
        MUST(Boss_LoadBackground(vfs, names[i], a, &loaded.backgrounds[i]));
    }
    MUST(M_LoadMasks(vfs, a, &loaded));
    MUST(B20M_ValidateAssets(&loaded));
    *out = loaded;
    return OK;
}
void B20M_Init(void)
{
    memset(&g_B20M, 0, sizeof(g_B20M));
    g_B20M.ray_previous_x = 999;
    g_B20M.ray_previous_y = 999;
    g_B20M.flake.state = -1;
}
void B20M_Setup(const B20M_ASSETS *const assets)
{
    ASSERT(
        g_B20M.sequence == B20M_SEQUENCE_NONE
        && g_B20M.wait_kind == B20M_WAIT_NONE && !g_B20M.entrance_active);
    ASSERT(Result_Ignore(B20M_ValidateAssets(assets)));
    ASSERT(Result_Ignore(BossEntity_Bind(&g_B20M.shield, 0, &assets->shield)));
    ASSERT(Result_Ignore(BossAnimation_Bind(&g_B20M.wand, 0, &assets->wand)));
    ASSERT(Result_Ignore(BossAnimation_Bind(&g_B20M.dress, 1, &assets->dress)));
    BossEntity_SetPosition(
        &g_B20M.shield, 304, 144, (BOSS_BOUNDS) { 0, 640, 0, 400 });
    BossEntity_SetOrbHitbox(&g_B20M.shield, (BOSS_BOUNDS) { -16, 64, -16, 64 });
    g_B20M.shield.orb_inactive = false;
    g_B20M.dress.left = 280;
    g_B20M.dress.top = 192;
    g_B20M.wand.left = 296;
    g_B20M.wand.top = 48;
    g_B20M.hp = 18;
    g_B20M.phase = B20M_PHASE_START;
    g_B20M.frame = 0;
    g_B20M.finished = false;
    g_B20M.boss_palette = g_Palette.saved;
    g_HudHp.first_white = 8;
    g_HudHp.first_redwhite = 2;
}

static uint16_t M_Random(const B20M_ASSETS *const assets)
{
    return MasterRng_Next(&g_Rng);
}
static uint8_t M_Angle(const int32_t y, const int32_t x)
{
    return Math16_Angle(Math16_Wrap(y), Math16_Wrap(x));
}
static double M_Add(const double x, const double y)
{
    return Finite_ToDouble(
        Finite_Add(Finite_FromDouble(x), Finite_FromDouble(y)));
}
static int16_t M_Pixel(const double x)
{
    return Finite_ToPixel(Finite_FromDouble(x));
}
static int16_t M_PixelAdd(const double x, const int16_t y)
{
    return Finite_ToPixel(
        Finite_Add(Finite_FromDouble(x), Finite_FromDouble(y)));
}
static void M_Pi(
    const B20M_ASSETS *const assets, const PI_IMAGE *const image,
    const bool show)
{
    const VIDEO_IMAGE view = { .pixels = image->pixels,
                               .width = image->width,
                               .height = image->height,
                               .stride = image->width,
                               .key = -1 };
    Video_Blit(&view, (GFX_RECT) { 0, 0, image->width, image->height }, 0, 0);
    Palette_Import(&g_Palette.resource, image->palette);
    if (show) {
        Palette_Set(&g_Palette, &g_Palette.resource);
    }
}
static void M_Palette(const B20M_ASSETS *const assets, const PALETTE *const p)
{
    Palette_Set(&g_Palette, p);
}
static void M_Color(
    const B20M_ASSETS *const assets, const uint8_t i, const int16_t r,
    const int16_t g, const int16_t blue)
{
    Palette_SetColor(&g_Palette, i, r, g, blue);
}
static void M_CopyOther(const B20M_ASSETS *const assets)
{
    const int32_t page = g_Video.drawing_page;
    Video_Copy(page, (GFX_RECT) { 0, 0, 640, 400 }, 1 - page, 0, 0);
}
static void M_CopyRows(
    const B20M_ASSETS *const assets, const int16_t top, const int16_t height)
{
    for (int32_t i = 0; i < height * 80; i++) {
        const uint16_t offset = (uint16_t)(top * 80 + i) & 0x7fff;
        if (offset < 32000) {
            memcpy(
                g_Video.pages[0] + offset * 8, g_Video.pages[1] + offset * 8,
                8);
        }
    }
    Video_SelectDrawingPage(0);
}
static void M_Snap(const B20M_ASSETS *const assets)
{
    const int32_t left = g_B20M.wand.left / 8 * 8;
    for (int32_t y = 0; y < 96; y++) {
        memcpy(
            g_B20M.wand_background + y * 128,
            g_Video.pages[1] + (g_B20M.wand.top + y) * 640 + left, 128);
    }
    Video_SelectDrawingPage(0);
}
static void M_WandPut(const B20M_ASSETS *const assets)
{
    const VIDEO_IMAGE v = { .pixels = g_B20M.wand_background,
                            .width = 128,
                            .height = 96,
                            .stride = 128,
                            .key = -1 };
    Video_Blit(
        &v, (GFX_RECT) { 0, 0, 128, 96 }, g_B20M.wand.left / 8 * 8,
        g_B20M.wand.top);
}
static void M_WandLower(const B20M_ASSETS *const assets)
{
    Video_SelectDrawingPage(1);
    M_WandPut(assets);
    Video_SelectDrawingPage(0);
    M_WandPut(assets);
}
static bool M_WandRaise(const B20M_ASSETS *const assets, const bool restart)
{
    if (restart) {
        g_B20M.wand_frame = 0;
    } else if (g_B20M.wand_frame == 0) {
        g_B20M.wand_frame = 1;
    } else {
        g_B20M.wand_frame = Math16_Wrap(g_B20M.wand_frame + 1);
        if (g_B20M.wand_frame == 2 || g_B20M.wand_frame == 16) {
            g_B20M.wand.image = g_B20M.wand_frame == 2 ? 0 : 1;
            Video_SelectDrawingPage(1);
            BossAnimation_Put(&g_B20M.wand);
            Video_SelectDrawingPage(0);
            BossAnimation_Put(&g_B20M.wand);
            if (g_B20M.wand_frame == 16) {
                g_B20M.wand_frame = 0;
                return true;
            }
        }
    }
    return false;
}
static void M_Shield(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame % 10 != 0) {
        return;
    }
    g_B20M.shield.image = (g_B20M.frame % 40) / 10;
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        g_B20M.shield.lock_frame = 0;
        BossEntity_LockedMovePut(&g_B20M.shield, 0, 0, 1, false);
    }
}
static void M_Dress(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame % 15 != 0) {
        return;
    }
    g_B20M.dress.image = (g_B20M.frame % 60) / 15;
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        BossAnimation_Put(&g_B20M.dress);
    }
}
static void M_Group(
    const B20M_ASSETS *const assets, const int16_t x, const int16_t y,
    const int16_t group, const int16_t speed)
{
    Pellets_AddGroup(g_Session.settings.difficulty, x, y, group, speed);
}
static void M_Single(
    const B20M_ASSETS *const assets, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed)
{
    Pellets_AddSingle(
        g_Session.settings.difficulty, x, y, angle, speed, PELLET_REGULAR, 0, 0,
        0);
}
static void M_Rain(
    const B20M_ASSETS *const assets, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed)
{
    Pellets_AddSingle(
        g_Session.settings.difficulty, x, y, angle, speed, PELLET_GRAVITY, 1, 0,
        0);
}
static void M_Laser(
    const B20M_ASSETS *const assets, const int32_t index, const int16_t x,
    const int16_t y, const int16_t tx, const int16_t ty, const int16_t speed,
    const uint8_t color, const int16_t age, const int16_t width)
{
    ShootoutLasers_Spawn(
        &g_ShootoutLasers.slots[((index % 10) + 10) % 10], x, y, tx, ty, speed,
        color, age, width);
}
static void M_Erase(
    const B20M_ASSETS *const assets, const int16_t x, const int16_t y)
{
    Sprite_Restore16(x, y, 48, 32);
}
static void M_Grc(
    const B20M_ASSETS *const assets, const int16_t left, const int16_t top,
    const int16_t bank, const int16_t image, const uint8_t color)
{
    const GRC *const banks[] = { &assets->vortex, &assets->bird, &assets->cross,
                                 &assets->splash };
    const GRC *const grc = banks[bank];
    if (image < 0 || image >= grc->count || left < -32 || left >= 640
        || top < -32 || top >= 400) {
        return;
    }
    int16_t row = Math16_Wrap(top * 80 + left / 8 - (left % 8 < 0));
    const uint8_t *const pixels = grc->coverage + image * 1024;
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
static void M_Mask(
    const B20M_ASSETS *const assets, const int16_t left, const int16_t top,
    const uint8_t *const mask, const uint8_t color)
{
    if (left < 0 || left >= 640 || top < 0 || top >= 400) {
        return;
    }
    for (int32_t y = 0; y < 8; y++) {
        for (int32_t x = 0; x < 8; x++) {
            const int32_t offset = (top + y) * 640 + left + x;
            if (offset < VIDEO_PAGE_SIZE && (mask[y] & (0x80u >> x)) != 0) {
                g_Video.pages[g_Video.drawing_page][offset] = color & 15;
            }
        }
    }
}
static void M_Ellipse(
    const B20M_ASSETS *const assets, const int16_t x, const int16_t y,
    const int16_t rx, const int16_t ry, const uint8_t color, const uint8_t step)
{
    Shape_EllipseArcPut(x, y, rx, ry, color, step, 0, 255);
}
static void M_EraseEllipse(
    const B20M_ASSETS *const assets, const int16_t x, const int16_t y,
    const int16_t rx, const int16_t ry, const uint8_t step)
{
    Shape_EllipseArcErase(x, y, rx, ry, step, 0, 255);
}
static void M_Ray(
    const B20M_ASSETS *const assets, const int16_t ox, const int16_t oy,
    const int16_t x, const int16_t y, const uint8_t color)
{
    if (color == 99) {
        g_B20M.ray_previous_x = g_B20M.ray_previous_y = 999;
        return;
    }
    if (g_B20M.ray_previous_x >= 0 && g_B20M.ray_previous_x < 640
        && g_B20M.ray_previous_y >= 0 && g_B20M.ray_previous_y < 400) {
        Video_LineErase(ox, oy, g_B20M.ray_previous_x, g_B20M.ray_previous_y);
    }
    if (x >= 0 && x < 640 && y >= 0 && y < 400) {
        Video_Line(ox, oy, x, y, color);
    }
    g_B20M.ray_previous_x = x;
    g_B20M.ray_previous_y = y;
}
static bool M_Delay(const uint16_t frames)
{
    if (Frame_DelayReady(frames)) {
        return true;
    }
    g_B20M.wait_kind = B20M_WAIT_DELAY;
    g_B20M.wait_frames = frames;
    return false;
}
static bool M_Scroll(const int16_t line)
{
    g_B20M.raw_scroll = line;
    g_B20M.wait_kind = B20M_WAIT_SCROLL;
    Frame_BeginWait(&g_B20M.wait);
    return false;
}
static bool M_Ready(const B20M_ASSETS *const assets, const uint64_t tick)
{
    if (g_B20M.wait_kind == B20M_WAIT_SCROLL) {
        if (!Frame_WaitReady(&g_B20M.wait)) {
            return false;
        }
        Video_SetScroll(g_B20M.raw_scroll);
    } else if (g_B20M.wait_kind == B20M_WAIT_DELAY) {
        if (!Frame_DelayReady(g_B20M.wait_frames)) {
            return false;
        }
    } else if (
        g_B20M.wait_kind == B20M_WAIT_TICKS && tick - g_B20M.wait_tick < 6) {
        return false;
    }
    g_B20M.wait_kind = B20M_WAIT_NONE;
    return true;
}
static const PALETTE *M_TransitionTarget(const bool fade)
{
    return fade ? &g_Palette.resource : &g_Palette.saved;
}
static void M_BirdsReset(void)
{
    for (int32_t i = 0; i < 30; i++) {
        g_B20M.birds.alive[i] = false;
    }
}
static void M_BirdSpawn(
    const double x, const double y, const double vx, const double vy)
{
    if (x == 0) {
        return;
    }
    for (int32_t i = 0; i < 30; i++) {
        if (!g_B20M.birds.alive[i]) {
            g_B20M.birds.alive[i] = true;
            g_B20M.birds.left[i] = x;
            g_B20M.birds.top[i] = y;
            g_B20M.birds.velocity_x[i] = vx;
            g_B20M.birds.velocity_y[i] = vy;
            g_B20M.birds.reserved[i] = 1;
            g_B20M.birds.hatch_time[i] = 25;
            g_B20M.birds.hatch_frames[i] = 25;
            return;
        }
    }
}
static void M_BirdsFire(const B20M_ASSETS *const assets, const int16_t group)
{
    for (int32_t i = 0; i < 30; i++) {
        if (!g_B20M.birds.alive[i] || g_B20M.birds.hatch_time[i] != 0) {
            continue;
        }
        const int16_t x = M_PixelAdd(g_B20M.birds.left[i], 12),
                      y = M_PixelAdd(g_B20M.birds.top[i], 12);
        if (group == 0) {
            M_Group(assets, x, y, PELLET_GROUP_SINGLE_AIMED, 48);
        } else if (group == 1) {
            for (int32_t j = 0; j < 6; j++) {
                M_Single(assets, x, y, (uint8_t)(j * 42), 64);
            }
        } else {
            M_Rain(assets, x, y, (uint8_t)((M_Random(assets) & 127) + 128), 64);
        }
    }
}
static bool M_BirdsUpdate(const B20M_ASSETS *const assets, const uint64_t tick)
{
    B20M_BIRDS *const birds = &g_B20M.birds;
    for (int32_t i = 0; i < 30; i++) {
        if (!birds->alive[i]) {
            continue;
        }
        const int16_t x = M_Pixel(birds->left[i]), y = M_Pixel(birds->top[i]);
        if (birds->hatch_time[i] > 0) {
            M_Grc(
                assets, x, y, 1,
                1
                    + (birds->hatch_frames[i] - birds->hatch_time[i])
                        / (birds->hatch_frames[i] / 5),
                15);
            birds->hatch_time[i]--;
        } else {
            M_Erase(assets, x, y);
        }
    }
    for (int32_t i = 0; i < 30; i++) {
        if (!birds->alive[i] || birds->hatch_time[i] != 0) {
            continue;
        }
        birds->left[i] = M_Add(birds->left[i], birds->velocity_x[i]);
        birds->top[i] = M_Add(birds->top[i], birds->velocity_y[i]);
        const int16_t x = M_Pixel(birds->left[i]), y = M_Pixel(birds->top[i]);
        if (birds->left[i] < 0 || birds->left[i] > 639 || birds->top[i] < 0
            || birds->top[i] > 399) {
            M_Erase(assets, x, y);
            birds->alive[i] = false;
            continue;
        }
        M_Grc(assets, x, y, 1, 6 + (g_B20M.frame / 5) % 2, 15);
        if (!Boss_Player()->invincible
            && birds->left[i] > Boss_Player()->left - 8
            && birds->left[i] < Boss_Player()->left + 8 && birds->top[i] > 344
            && birds->top[i] < 384) {
            Boss_Player()->is_hit = true;
            g_B20M.wait_kind = B20M_WAIT_TICKS;
            g_B20M.wait_tick = tick;
            g_B20M.resume_stage = B20M_RESUME_BIRD_HIT;
            return false;
        }
    }
    return true;
}
static int16_t M_DoubleOffset(const double left, const double top)
{
    FINITE x = Finite_FromDouble(left);
    x = Finite_Half(Finite_Half(Finite_Half(x)));
    return Finite_ToPixel(Finite_Add(
        x, Finite_Multiply(Finite_FromDouble(top), Finite_FromDouble(80))));
}
static uint8_t M_CompareByte(
    const B20M_ASSETS *const assets, const int32_t address)
{
    const uint16_t offset = (uint16_t)address & 32767;
    uint8_t result = 0;
    if (offset < 32000) {
        for (int32_t bit = 0; bit < 8; bit++) {
            if (g_Video.pages[1][offset * 8 + bit] == 12) {
                result |= 0x80u >> bit;
            }
        }
    }
    return result;
}
static void M_MaskedByte(
    const B20M_ASSETS *const assets, const int32_t address, const uint8_t mask,
    const uint8_t color)
{
    const uint16_t offset = (uint16_t)address & 32767;
    if (offset < 32000) {
        for (int32_t bit = 0; bit < 8; bit++) {
            if ((mask & (0x80u >> bit)) != 0) {
                g_Video.pages[0][offset * 8 + bit] = color & 15;
            }
        }
    }
}
static void M_Particle(
    const B20M_ASSETS *const assets, const int16_t vo, const int16_t bit,
    const uint8_t color, const bool cross)
{
    if (bit < 0 || bit > 7) {
        return;
    }
    for (int32_t y = 0; y < 2; y++) {
        const int16_t p = Math16_Wrap(vo + y * 80);
        const uint8_t mask = M_CompareByte(assets, p) & (0xc0u >> bit);
        M_MaskedByte(assets, p, mask, color);
        if (cross && bit == 7) {
            M_MaskedByte(
                assets, Math16_Wrap(p + 1),
                M_CompareByte(assets, Math16_Wrap(p + 1)) & 128, color);
        }
    }
    Video_SelectDrawingPage(0);
}
static void M_Vertical(const B20M_ASSETS *const assets, const bool bottom)
{
    B20M_PARTICLES *const p = &g_B20M.vertical;
    if (g_B20M.frame % 7 == 0) {
        for (int32_t i = 0; i < 30; i++) {
            if (p->color[i] == 0) {
                p->left[i] = M_Random(assets) % 640;
                p->top[i] = bottom ? 397 : 0;
                p->velocity[i] = bottom ? -(M_Random(assets) % 15) - 8
                                        : (M_Random(assets) % 15) + 2;
                p->color[i] = 4;
                break;
            }
        }
    }
    if (g_B20M.frame % 2 != 0) {
        return;
    }
    for (int32_t i = 0; i < 30; i++) {
        if (p->color[i] == 0) {
            continue;
        }
        int16_t bit = M_Pixel(p->left[i]) % 8;
        if (bit == 7) {
            bit = 0;
        }
        M_Particle(
            assets, M_DoubleOffset(p->left[i], p->top[i]), bit, 12, false);
        p->top[i] = M_Add(p->top[i], p->velocity[i]);
        const int16_t vo = M_DoubleOffset(p->left[i], p->top[i]);
        if (vo >= 31920 || vo < 0) {
            p->color[i] = 0;
            continue;
        }
        M_Particle(assets, vo, bit, (uint8_t)p->color[i], false);
    }
}
static void M_Wavy(const B20M_ASSETS *const assets)
{
    B20M_WAVES *const p = &g_B20M.waves;
    if (g_B20M.frame % 7 == 0) {
        for (int32_t i = 0; i < 30; i++) {
            if (p->color[i] == 0) {
                p->left[i] = M_Random(assets) % 640;
                p->top[i] = 400;
                p->velocity[i] = -1;
                p->age[i] = 0;
                p->color[i] = 4;
                break;
            }
        }
    }
    if (g_B20M.frame % 2 != 0) {
        return;
    }
    for (int32_t i = 0; i < 30; i++) {
        if (p->color[i] == 0) {
            continue;
        }
        int16_t x = Math16_PolarY(p->left[i], 16, p->age[i]);
        M_Particle(
            assets, Math16_Wrap(p->top[i] * 80 + x / 8 - (x % 8 < 0)),
            (uint16_t)x & 7, 12, false);
        p->top[i] = Math16_Wrap(p->top[i] + p->velocity[i]);
        p->age[i] = Math16_Wrap(p->age[i] + 1);
        x = Math16_PolarY(p->left[i], 16, p->age[i]);
        if (p->age[i] >= 100) {
            p->color[i] = 0;
            continue;
        }
        M_Particle(
            assets, Math16_Wrap(p->top[i] * 80 + x / 8 - (x % 8 < 0)),
            (uint16_t)x & 7, (uint8_t)p->color[i], false);
    }
}
static void M_Horizontal(const B20M_ASSETS *const assets)
{
    B20M_PARTICLES *const p = &g_B20M.horizontal;
    if (g_B20M.frame % 7 == 0) {
        for (int32_t i = 0; i < 30; i++) {
            if (p->color[i] == 0) {
                p->left[i] = M_Random(assets) % 640;
                p->top[i] = M_Random(assets) % 400;
                p->velocity[i] = M_Random(assets) % 2 == 0 ? -6 : 6;
                p->color[i] = 4;
                break;
            }
        }
    }
    if (g_B20M.frame % 2 != 0) {
        return;
    }
    for (int32_t i = 0; i < 30; i++) {
        if (p->color[i] == 0) {
            continue;
        }
        M_Particle(
            assets, M_DoubleOffset(p->left[i], p->top[i]),
            M_Pixel(p->left[i]) % 8, 12, true);
        p->left[i] = M_Add(p->left[i], p->velocity[i]);
        p->velocity[i] = M_Add(p->velocity[i], p->velocity[i] < 0 ? 0.1 : -0.1);
        if (p->left[i] >= 640 || p->left[i] < 0
            || M_Pixel(p->velocity[i]) == 0) {
            p->color[i] = 0;
            continue;
        }
        M_Particle(
            assets, M_DoubleOffset(p->left[i], p->top[i]),
            M_Pixel(p->left[i]) % 8, (uint8_t)p->color[i], true);
    }
}
static void M_Hud(const B20M_ASSETS *assets);
static void M_GustFinish(const B20M_ASSETS *const assets, const int16_t image)
{
    Video_SelectDrawingPage(1);
    M_Pi(assets, &assets->backgrounds[image], true);
    M_CopyOther(assets);
    g_B20M.stage_palette = g_Palette.saved;
    Video_SelectDrawingPage(0);
    M_Hud(assets);
    HudHp_Rerender(g_B20M.hp);
    g_B20M.frame = 0;
}
static bool M_Gust(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame == 20) {
        g_B20M.gust.id = 0;
    }
    if (g_B20M.frame < 50) {
        return true;
    }
    if (g_B20M.frame == 50) {
        g_B20M.gust.x = 0;
        g_B20M.gust.y = 0;
        g_B20M.gust.offset = 0;
        g_B20M.gust.color = 2;
        Sound_Effect(10);
    }
    for (int32_t row = 0; row < 100; row++) {
        const int16_t right = M_Random(assets) % 8;
        uint16_t masks[10] = {};
        const int16_t first = Math16_Wrap(g_B20M.gust.offset + right);
        for (int32_t stripe = 0; stripe < 10; stripe++) {
            const int16_t offset = Math16_Wrap(first - stripe * 2);
            if (first / 80 != offset / 80) {
                break;
            }
            if (offset >= 0 && offset / 80 == g_B20M.gust.offset / 80) {
                masks[stripe] =
                    (uint16_t)(M_CompareByte(assets, offset)
                               | (uint16_t)M_CompareByte(
                                     assets, Math16_Wrap(offset + 1))
                                   << 8);
            }
        }
        for (int32_t stripe = 0; stripe < 5; stripe++) {
            const int16_t offset = Math16_Wrap(first - stripe * 2);
            if (first / 80 != offset / 80) {
                break;
            }
            if (offset >= 0 && offset / 80 == g_B20M.gust.offset / 80) {
                M_MaskedByte(
                    assets, offset, (uint8_t)masks[stripe],
                    (uint8_t)(g_B20M.gust.color
                              + (stripe > 2 ? 4 - stripe : stripe)));
            }
        }
        for (int32_t stripe = 0; stripe < g_B20M.gust.id + 2; stripe++) {
            const int16_t offset = Math16_Wrap(first - 10 - stripe * 2);
            if (offset / 80 != first / 80) {
                break;
            }
            if (offset >= 0 && offset / 80 == g_B20M.gust.offset / 80) {
                M_MaskedByte(assets, offset, (uint8_t)masks[5 + stripe], 12);
            }
        }
        g_B20M.gust.y = Math16_Wrap(g_B20M.gust.y + 4);
        g_B20M.gust.offset = Math16_Wrap(
            g_B20M.gust.y * 80 + g_B20M.gust.x / 8 - (g_B20M.gust.x % 8 < 0));
        if (g_B20M.gust.offset >= 32000) {
            g_B20M.gust.y = 0;
            g_B20M.gust.x = Math16_Wrap(g_B20M.gust.x + 32);
            g_B20M.gust.offset = g_B20M.gust.x / 8;
        }
    }
    Video_SelectDrawingPage(0);
    if (g_B20M.gust.x > 608) {
        g_B20M.gust.id = Math16_Wrap(g_B20M.gust.id + 1);
        g_B20M.frame = 49;
        if (g_B20M.gust.id >= 3) {
            g_B20M.resume_stage = B20M_RESUME_GUST_END;
            return M_Scroll(0);
        }
    }
    return true;
}
static void M_VortexDraw(
    const B20M_ASSETS *const assets, const int32_t i, const bool spread)
{
    M_Erase(
        assets, g_B20M.vortices.previous_left[i],
        g_B20M.vortices.previous_top[i]);
    M_Grc(
        assets, g_B20M.vortices.left[i], g_B20M.vortices.top[i], 0,
        g_B20M.frame % 3, 7);
    if (spread && g_B20M.frame % 4 == 0) {
        const uint8_t angle = M_Angle(
            388 - g_B20M.vortices.top[i],
            g_B20M.vortices.left[1 - i] - g_B20M.vortices.left[i]);
        M_Single(
            assets, Math16_Wrap(g_B20M.vortices.left[i] + 12),
            Math16_Wrap(g_B20M.vortices.top[i] + 12), 64, 112);
        M_Single(
            assets, Math16_Wrap(g_B20M.vortices.left[i] + 12),
            Math16_Wrap(g_B20M.vortices.top[i] + 12), angle, 112);
        M_Single(
            assets, Math16_Wrap(g_B20M.vortices.left[i] + 12),
            Math16_Wrap(g_B20M.vortices.top[i] + 12), (uint8_t)(128 - angle),
            112);
    }
}
static void M_Vortices(const B20M_ASSETS *const assets)
{
    if (!g_B20M.vortex_wand_done) {
        g_B20M.vortex_wand_done = M_WandRaise(assets, false);
    }
    if (g_B20M.frame < 50) {
        return;
    }
    if (g_B20M.frame == 50) {
        g_B20M.pattern_state = Boss_Rank(140, 145, 150, 155);
    }
    if (g_B20M.frame < 100) {
        for (int32_t i = 0; i < 2; i++) {
            g_B20M.vortices.left[i] = (int16_t)(i * 608);
            g_B20M.vortices.top[i] = 200;
            M_VortexDraw(assets, i, false);
            if (g_B20M.frame % 4 == 0) {
                M_Group(
                    assets, Math16_Wrap(g_B20M.vortices.left[i] + 12),
                    Math16_Wrap(g_B20M.vortices.top[i] + 12),
                    PELLET_GROUP_SINGLE, 112);
            }
            g_B20M.vortices.previous_left[i] = g_B20M.vortices.left[i];
            g_B20M.vortices.previous_top[i] = g_B20M.vortices.top[i];
        }
    } else if (g_B20M.frame < g_B20M.pattern_state) {
        for (int32_t i = 0; i < 2; i++) {
            g_B20M.vortices.left[i] =
                Math16_Wrap(g_B20M.vortices.left[i] + (i == 0 ? 5 : -5));
            g_B20M.vortices.top[i] = Math16_Wrap(g_B20M.vortices.top[i] - 2);
            M_VortexDraw(assets, i, false);
            if (g_B20M.frame % 4 == 0) {
                M_Group(
                    assets, Math16_Wrap(g_B20M.vortices.left[i] + 12),
                    Math16_Wrap(g_B20M.vortices.top[i] + 12),
                    PELLET_GROUP_SINGLE, 112);
                M_Single(
                    assets, Math16_Wrap(g_B20M.vortices.left[i] + 12),
                    Math16_Wrap(g_B20M.vortices.top[i] + 12),
                    M_Angle(412 - g_B20M.vortices.top[i], i * 160 - 68), 112);
            }
            g_B20M.vortices.previous_left[i] = g_B20M.vortices.left[i];
            g_B20M.vortices.previous_top[i] = g_B20M.vortices.top[i];
        }
    } else if (g_B20M.frame < 200) {
        for (int32_t i = 0; i < 2; i++) {
            M_VortexDraw(assets, i, true);
        }
    } else if (g_B20M.frame < 240) {
        if (g_B20M.frame == 200) {
            g_B20M.direction_first = M_Random(assets) % 2 != 0;
            if (g_B20M.direction_first) {
                M_WandLower(assets);
            }
        }
        for (int32_t i = 0; i < 2; i++) {
            g_B20M.vortices.left[i] = Math16_Wrap(
                g_B20M.vortices.left[i] + 2 - g_B20M.direction_first * 4);
            M_VortexDraw(assets, i, true);
            g_B20M.vortices.previous_left[i] = g_B20M.vortices.left[i];
        }
    } else if (g_B20M.frame < 320) {
        if (g_B20M.frame == 240) {
            g_B20M.direction_second = M_Random(assets) % 2 != 0;
            if (!g_B20M.direction_second
                && g_B20M.direction_second == g_B20M.direction_first) {
                M_WandLower(assets);
            } else if (
                g_B20M.direction_second
                && g_B20M.direction_second == g_B20M.direction_first) {
                g_B20M.vortex_wand_done = false;
            }
        }
        for (int32_t i = 0; i < 2; i++) {
            g_B20M.vortices.left[i] = Math16_Wrap(
                g_B20M.vortices.left[i]
                - (g_B20M.direction_second != g_B20M.direction_first
                       ? 2 - g_B20M.direction_second * 4
                       : 4 - g_B20M.direction_second * 8));
            M_VortexDraw(assets, i, true);
            g_B20M.vortices.previous_left[i] = g_B20M.vortices.left[i];
        }
    } else {
        for (int32_t i = 0; i < 2; i++) {
            M_Erase(assets, g_B20M.vortices.left[i], g_B20M.vortices.top[i]);
        }
        M_WandLower(assets);
        g_B20M.frame = 0;
        g_B20M.vortex_wand_done = false;
    }
}
static void M_PurpleLasers(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame < 50) {
        return;
    }
    if (g_B20M.frame == 50) {
        g_B20M.pattern_state = Boss_Rank(68, 72, 76, 80);
        for (int32_t i = 0; i < 10; i++) {
            g_B20M.laser.x[i] = 120 + M_Random(assets) % 400;
            g_B20M.laser.y[i] = 100 + M_Random(assets) % 100;
        }
    }
    if (g_B20M.frame < 150) {
        const int32_t i = (g_B20M.frame - 50) / 10;
        if (g_B20M.frame % 10 != 9) {
            if (g_B20M.frame % 10 != 0) {
                const int16_t r = (int16_t)(64 - ((g_B20M.frame - 1) % 10) * 8);
                M_EraseEllipse(
                    assets, g_B20M.laser.x[i], g_B20M.laser.y[i], r, r, 1);
            }
            if (g_B20M.frame % 10 != 8) {
                const int16_t r = (int16_t)(64 - (g_B20M.frame % 10) * 8);
                M_Ellipse(
                    assets, g_B20M.laser.x[i], g_B20M.laser.y[i], r, r, 4, 1);
            }
        } else {
            Sound_Effect(6);
            M_Laser(
                assets, i, g_B20M.laser.x[i], g_B20M.laser.y[i],
                M_Random(assets) % 640, 400, g_B20M.pattern_state, 4, 25, 5);
        }
    } else if (g_B20M.frame > 200) {
        g_B20M.frame = 0;
    }
}
static bool M_In(
    const int16_t x, const int16_t y, const int16_t l, const int16_t t,
    const int16_t r, const int16_t bottom)
{
    return x >= l && x <= r && y >= t && y <= bottom;
}
static void M_Eggs(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame < 40) {
        return;
    }
    if (!g_B20M.bird_wand_done) {
        g_B20M.bird_wand_done = M_WandRaise(assets, false);
    }
    if (g_B20M.frame < 50) {
        return;
    }
    if (g_B20M.frame == 50) {
        g_B20M.eggs.alive = 0;
        g_B20M.spawner.left = 5440;
        g_B20M.spawner.top = 1024;
        g_B20M.spawner.velocity_x =
            (int16_t)(64 - (M_Random(assets) % 2) * 128);
        g_B20M.spawner.velocity_y = (int16_t)(32 - (M_Random(assets) % 2) * 64);
        g_B20M.pattern_state = Boss_Rank(20, 15, 10, 8);
        Sound_Effect(8);
    } else if (g_B20M.frame < 200) {
        g_B20M.spawner.velocity_x = Math16_Wrap(
            g_B20M.spawner.velocity_x + (g_B20M.spawner.left < 5120 ? 1 : -1));
        g_B20M.spawner.velocity_y = Math16_Wrap(
            g_B20M.spawner.velocity_y + (g_B20M.spawner.top < 2624 ? 1 : -1));
        M_Erase(
            assets, Math16_ToPixel(g_B20M.spawner.left),
            Math16_ToPixel(g_B20M.spawner.top));
        for (int32_t i = g_B20M.eggs.alive - 1; i >= 0; i--) {
            M_Grc(assets, g_B20M.eggs.left[i], g_B20M.eggs.top[i], 1, 0, 15);
        }
        g_B20M.spawner.left =
            Math16_Wrap(g_B20M.spawner.left + g_B20M.spawner.velocity_x);
        g_B20M.spawner.top =
            Math16_Wrap(g_B20M.spawner.top + g_B20M.spawner.velocity_y);
        if (g_B20M.frame % g_B20M.pattern_state == 0
            && M_In(
                g_B20M.spawner.left, g_B20M.spawner.top, 0, 0, 9728, 5888)) {
            if (g_B20M.eggs.alive < 30) {
                g_B20M.eggs.left[g_B20M.eggs.alive] =
                    Math16_ToPixel(g_B20M.spawner.left);
                g_B20M.eggs.top[g_B20M.eggs.alive] =
                    Math16_ToPixel(g_B20M.spawner.top);
                g_B20M.eggs.alive++;
            }
            M_Grc(
                assets, Math16_ToPixel(g_B20M.spawner.left),
                Math16_ToPixel(g_B20M.spawner.top), 1, 0, 15);
        }
        M_Grc(
            assets, Math16_ToPixel(g_B20M.spawner.left),
            Math16_ToPixel(g_B20M.spawner.top), 2, g_B20M.frame % 2,
            (uint8_t)(g_B20M.frame % 4 + 2));
    } else if (g_B20M.frame < 400) {
        if (g_B20M.frame == 200) {
            if (g_B20M.spawner.velocity_y > -4
                && g_B20M.spawner.velocity_y < 4) {
                g_B20M.spawner.velocity_y =
                    g_B20M.spawner.velocity_y < 0 ? -4 : 4;
            }
            if (g_B20M.spawner.velocity_x > -4
                && g_B20M.spawner.velocity_x < 4) {
                g_B20M.spawner.velocity_x =
                    g_B20M.spawner.velocity_x < 0 ? -4 : 4;
            }
            for (int32_t i = g_B20M.eggs.alive - 1; i >= 0; i--) {
                const MATH16_VECTOR v =
                    Math16_Vector(3, (M_Random(assets) & 127) + 128);
                M_BirdSpawn(g_B20M.eggs.left[i], g_B20M.eggs.top[i], v.x, v.y);
            }
            M_WandLower(assets);
            g_B20M.bird_pellet_group = M_Random(assets) % 2;
        }
        if (M_In(
                g_B20M.spawner.left, g_B20M.spawner.top, -512, -512, 10240,
                6400)) {
            M_Erase(
                assets, Math16_ToPixel(g_B20M.spawner.left),
                Math16_ToPixel(g_B20M.spawner.top));
            g_B20M.spawner.left =
                Math16_Wrap(g_B20M.spawner.left + g_B20M.spawner.velocity_x);
            g_B20M.spawner.top =
                Math16_Wrap(g_B20M.spawner.top + g_B20M.spawner.velocity_y);
            if (M_In(g_B20M.spawner.left, g_B20M.spawner.top, 0, 0, 608, 368)) {
                M_Grc(
                    assets, g_B20M.spawner.left, g_B20M.spawner.top, 2,
                    g_B20M.frame % 2, (uint8_t)(g_B20M.frame % 4 + 2));
            }
        }
        if (g_B20M.frame % 16 == 0 || g_B20M.frame % 16 == 2) {
            M_BirdsFire(assets, g_B20M.bird_pellet_group);
        }
    } else if (g_B20M.frame == 400) {
        g_B20M.frame = 0;
        g_B20M.bird_wand_done = false;
    }
}
static void M_FlakeEllipse(
    const B20M_ASSETS *const assets, const int16_t rx, const int16_t ry,
    const uint8_t color, const uint8_t step, const bool erase)
{
    const int16_t x = Math16_ToPixel(g_B20M.flake.left);
    if (erase) {
        M_EraseEllipse(assets, x, 384, rx, ry, step);
    } else {
        M_Ellipse(assets, x, 384, rx, ry, color, step);
    }
}
static void M_Flake(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame < 10) {
        g_B20M.flake.state = -1;
        g_B20M.pattern_state = 160;
    }
    if (g_B20M.frame % g_B20M.pattern_state == 0) {
        g_B20M.flake.state = 0;
    }
    if (g_B20M.flake.state == 0) {
        g_B20M.flake.state = M_WandRaise(assets, false);
    }
    if (g_B20M.flake.state == 1) {
        Sound_Effect(6);
        g_B20M.flake.left = 5440;
        g_B20M.flake.top = 1024;
        const MATH16_VECTOR v = Math16_Aim(
            (MATH16_VECTOR) { 340, 64 },
            (MATH16_VECTOR) { Math16_Wrap(Boss_Player()->left + 12), 380 }, 112,
            0);
        g_B20M.flake.velocity_x = v.x;
        g_B20M.flake.velocity_y = v.y;
        g_B20M.flake.state = 2;
    }
    if (g_B20M.flake.state == 2) {
        Sprite_Restore16(
            Math16_ToPixel(g_B20M.flake.left), Math16_ToPixel(g_B20M.flake.top),
            16, 8);
        g_B20M.flake.top =
            Math16_Wrap(g_B20M.flake.top + g_B20M.flake.velocity_y);
        if (g_B20M.flake.top >= 6336) {
            g_B20M.flake.state = 3;
            M_WandLower(assets);
            return;
        }
        g_B20M.flake.left =
            Math16_Wrap(g_B20M.flake.left + g_B20M.flake.velocity_x);
        M_Mask(
            assets, Math16_ToPixel(g_B20M.flake.left),
            Math16_ToPixel(g_B20M.flake.top), assets->flake, 7);
    }
    if (g_B20M.flake.state == 3) {
        M_FlakeEllipse(assets, 48, 16, 6, 8, false);
        M_FlakeEllipse(assets, 16, 48, 6, 8, false);
        M_FlakeEllipse(assets, 24, 24, 7, 6, false);
        g_B20M.star.radius_outer_1 = 48;
        g_B20M.star.radius_outer_2 = 16;
        g_B20M.star.radius_inner = 24;
        g_B20M.star.left = g_B20M.flake.left;
        g_B20M.star.top = 400 - M_Random(assets) % 48;
        Sound_Effect(10);
    }
    if (g_B20M.flake.state >= 3) {
        g_B20M.flake.state = Math16_Wrap(g_B20M.flake.state + 1);
    }
    if (g_B20M.flake.state >= 5 && g_B20M.flake.state % 8 == 0) {
        Sprite_Restore16(g_B20M.star.left, g_B20M.star.top, 16, 8);
        M_FlakeEllipse(
            assets, g_B20M.star.radius_outer_1, g_B20M.star.radius_outer_2, 0,
            8, true);
        M_FlakeEllipse(
            assets, g_B20M.star.radius_outer_2, g_B20M.star.radius_outer_1, 0,
            8, true);
        M_FlakeEllipse(
            assets, g_B20M.star.radius_inner, g_B20M.star.radius_inner, 0, 6,
            true);
        g_B20M.star.radius_outer_1 =
            Math16_Wrap(g_B20M.star.radius_outer_1 - 6);
        g_B20M.star.radius_outer_2 =
            Math16_Wrap(g_B20M.star.radius_outer_2 + 6);
        g_B20M.star.radius_inner = Math16_Wrap(g_B20M.star.radius_inner + 4);
        g_B20M.star.left = Math16_Wrap(
            M_Random(assets) % 64 + Math16_ToPixel(g_B20M.flake.left) - 28);
        g_B20M.star.top = 400 - M_Random(assets) % 48;
        if (g_B20M.star.radius_outer_1 <= 8) {
            g_B20M.flake.state = -1;
            return;
        }
        M_FlakeEllipse(
            assets, g_B20M.star.radius_outer_1, g_B20M.star.radius_outer_2, 5,
            8, false);
        M_FlakeEllipse(
            assets, g_B20M.star.radius_outer_2, g_B20M.star.radius_outer_1, 5,
            8, false);
        M_FlakeEllipse(
            assets, g_B20M.star.radius_inner, g_B20M.star.radius_inner, 7, 6,
            false);
        M_Mask(assets, g_B20M.star.left, g_B20M.star.top, assets->star, 7);
    }
    const int16_t x = Math16_ToPixel(g_B20M.flake.left);
    if (!Boss_Player()->invincible && x + 32 > Boss_Player()->left
        && x - 64 < Boss_Player()->left && g_B20M.flake.state >= 3) {
        Boss_Player()->is_hit = true;
    }
}
static void M_Rings(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame < 5) {
        g_B20M.rings_angle = 0;
        g_B20M.rings_interval = Boss_Rank(18, 12, 10, 8);
    }
    if (g_B20M.frame % g_B20M.rings_interval == 0) {
        g_Pellets.spawn_with_cloud = true;
        M_Single(assets, 568, 160, (uint8_t)(0 - g_B20M.rings_angle), 56);
        M_Single(assets, 568, 160, (uint8_t)(128 - g_B20M.rings_angle), 56);
        M_Single(assets, 64, 160, g_B20M.rings_angle, 56);
        M_Single(assets, 64, 160, (uint8_t)(128 + g_B20M.rings_angle), 56);
        g_B20M.rings_angle =
            (uint8_t)(g_B20M.rings_angle + g_B20M.rings_interval);
    }
    if (g_B20M.frame > 479) {
        g_B20M.frame = 0;
    }
}
static void M_Sling(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame == 10) {
        g_B20M.pattern_state = Boss_Rank(4, 3, 2, 2);
    }
    if (g_B20M.frame % 200 >= 150 && g_B20M.frame % g_B20M.pattern_state == 0) {
        const uint8_t angle = (uint8_t)M_Random(assets);
        Pellets_AddSingle(
            g_Session.settings.difficulty, Math16_PolarY(320, 40, angle),
            Math16_PolarX(180, 40, angle), 0, 0, PELLET_SLING_AIMED, 72, 0, 0);
    }
    if (g_B20M.frame >= 299) {
        g_B20M.frame = 0;
    }
}
static void M_FourLasers(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame < 5) {
        M_Ray(assets, 0, 0, 0, 0, 99);
    }
    if (g_B20M.frame < 50) {
        return;
    }
    int16_t x = 0, y = 0, target = 0;
    int32_t laser = -1;
    if (g_B20M.frame == 50) {
        x = 460;
        y = 120;
        target = Boss_Player()->left;
        laser = 0;
        g_B20M.laser.target = target;
        g_B20M.pattern_state = Boss_Rank(60, 64, 68, 72);
    } else if (g_B20M.frame == 100) {
        x = 180;
        y = 120;
        target = Math16_Wrap(639 - g_B20M.laser.target);
        laser = 1;
    } else if (g_B20M.frame == 150) {
        x = 608;
        y = 164;
        target = Boss_Player()->left;
        laser = 2;
    } else if (g_B20M.frame == 200) {
        x = 24;
        y = 164;
        target = Boss_Player()->left;
        laser = 3;
    }
    if (laser >= 0) {
        g_B20M.laser.origin_x = x;
        g_B20M.laser.origin_y = y;
        const MATH16_VECTOR v = Math16_Aim(
            (MATH16_VECTOR) { x, y }, (MATH16_VECTOR) { target, 400 }, 16, 0);
        g_B20M.laser.velocity_x = v.x;
        g_B20M.laser.velocity_y = v.y;
        M_Laser(
            assets, laser, x, y, target, 400, g_B20M.pattern_state, 7, 50, 8);
        Sound_Effect(6);
    } else if (g_B20M.frame < 250) {
        M_Ray(
            assets, 320, 120, g_B20M.laser.origin_x, g_B20M.laser.origin_y, 7);
        g_B20M.laser.origin_x =
            Math16_Wrap(g_B20M.laser.origin_x + g_B20M.laser.velocity_x);
        g_B20M.laser.origin_y =
            Math16_Wrap(g_B20M.laser.origin_y + g_B20M.laser.velocity_y);
    } else if (g_B20M.frame > 300) {
        g_B20M.frame = 0;
    }
}
static bool M_Shake(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame < 50) {
        if (g_B20M.frame % 8 == 0 || g_B20M.frame % 8 == 4) {
            g_B20M.resume_stage = B20M_RESUME_SHAKE;
            return M_Scroll(g_B20M.frame % 8 == 0 ? 396 : 404);
        }
        if (g_B20M.frame % 4 == 0) {
            Sound_Effect(7);
        }
    } else if (g_B20M.frame == 50) {
        g_B20M.resume_stage = B20M_RESUME_SHAKE_END;
        return M_Scroll(0);
    }
    return true;
}
static void M_TopRain(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame % 8 == 0) {
        M_Group(
            assets, M_Random(assets) % 640, 64,
            g_Session.settings.difficulty == 3 ? PELLET_GROUP_RANDOM_WIDE
                                               : PELLET_GROUP_SINGLE,
            44);
    }
}
static void M_Radial(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame < 5) {
        M_Ray(assets, 0, 0, 0, 0, 99);
    }
    if (g_B20M.frame < 150) {
        return;
    }
    if (g_B20M.frame == 150) {
        g_B20M.radial_angle = 0;
        g_B20M.radial_velocity = 1;
        g_B20M.pattern_state = Boss_Rank(60, 65, 70, 75);
        Sound_Effect(6);
    } else if (g_B20M.frame < 200) {
        M_Ray(
            assets, 320, 160, Math16_PolarX(320, 140, g_B20M.radial_angle),
            Math16_PolarY(160, 140, g_B20M.radial_angle), 7);
        g_B20M.radial_angle =
            (uint8_t)(g_B20M.radial_angle + g_B20M.radial_velocity);
        g_B20M.radial_velocity++;
        if (g_B20M.frame % 10 == 0) {
            Sound_Effect(6);
        }
    } else if (g_B20M.frame == 200) {
        M_Ray(assets, 320, 160, 999, 999, 7);
        g_B20M.radial_angle = 0;
    } else if (g_B20M.frame <= 400 && g_B20M.frame % 5 == 0) {
        Sound_Effect(7);
        if (g_B20M.frame % 15 == 0) {
            M_Laser(
                assets, (g_B20M.frame - 215) / 15, 320, 160,
                Math16_PolarX(320, 600, g_B20M.radial_angle),
                Math16_PolarY(160, 600, g_B20M.radial_angle),
                g_B20M.pattern_state, 7, 20, 4);
        } else {
            M_Single(assets, 320, 160, g_B20M.radial_angle, 100);
        }
        M_Single(assets, 320, 160, (uint8_t)(g_B20M.radial_angle - 4), 96);
        M_Single(assets, 320, 160, (uint8_t)(g_B20M.radial_angle + 4), 96);
        if (g_Session.settings.difficulty >= 2) {
            M_Single(assets, 320, 160, (uint8_t)(g_B20M.radial_angle - 4), 64);
            M_Single(assets, 320, 160, (uint8_t)(g_B20M.radial_angle + 4), 64);
            if (g_Session.settings.difficulty == 3) {
                M_Single(assets, 320, 160, g_B20M.radial_angle, 64);
                M_Single(assets, 320, 160, g_B20M.radial_angle, 48);
            }
        }
        g_B20M.radial_angle = (uint8_t)(g_B20M.radial_angle + 12);
    } else if (g_B20M.frame == 450) {
        g_B20M.frame = 0;
    }
}
static int16_t M_DebrisNext(const int16_t image)
{
    return image < 5 ? Math16_Wrap(image + 1) : 3;
}
static int16_t M_RayX(const B20M_RAYS *const r, const bool left)
{
    return left ? r->x : Math16_Wrap(639 - r->x);
}
static int16_t M_DebrisX(
    const B20M_RAYS *const r, const bool left, const int16_t offset,
    const int16_t movement)
{
    return left ? Math16_Wrap(r->x - 32 + offset - movement)
                : Math16_Wrap(640 - offset - r->x + movement);
}
static void M_RaysDraw(
    const B20M_ASSETS *const assets, const B20M_RAYS *const r, const bool left,
    const int16_t ox, const int16_t oy, const int16_t offset,
    const int16_t speed, const bool broken)
{
    if (broken) {
        M_Erase(
            assets, M_DebrisX(r, left, offset, Math16_Wrap(speed * 2)),
            Math16_Wrap(r->y - 32));
        M_Grc(
            assets, M_DebrisX(r, left, offset, 0), Math16_Wrap(r->y - 16), 0,
            r->image, 7);
        M_Ray(assets, ox, oy, M_RayX(r, left), r->y, 7);
    } else {
        Sprite_Restore16(
            M_DebrisX(r, left, offset, 0), Math16_Wrap(r->y - 8), 32, 32);
        M_Ray(assets, ox, oy, M_RayX(r, left), r->y, 7);
        M_Grc(
            assets, M_DebrisX(r, left, offset, 0), Math16_Wrap(r->y - 16), 0,
            r->image, 7);
    }
}
static void M_SymmetricBirds(const B20M_ASSETS *const assets)
{
    B20M_RAYS *const r = &g_B20M.bird_rays;
    if (g_B20M.frame < 50) {
        return;
    }
    if (g_B20M.frame == 50) {
        g_B20M.pattern_state = Boss_Rank(6, 7, 8, 9);
        r->x = 2;
        r->y = 399;
        r->velocity_x = 2;
        r->velocity_y = -2;
        r->image = 3;
        return;
    }
    if (g_B20M.frame < 100) {
        M_RaysDraw(assets, r, g_B20M.frame % 2 == 0, 320, 120, 8, 2, true);
        if (g_B20M.frame % 2 != 0) {
            r->image = M_DebrisNext(r->image);
        }
        r->x = Math16_Wrap(r->x + 2);
        if (g_B20M.frame % 10 == 0) {
            Sound_Effect(6);
            const int16_t target = Math16_Wrap((g_B20M.frame - 60) * 10 + r->x);
            const MATH16_VECTOR v = Math16_Aim(
                (MATH16_VECTOR) { r->x, Math16_Wrap(r->y - 32) },
                (MATH16_VECTOR) { target, 0 }, 4, 0);
            r->velocity_x = v.x;
            r->velocity_y = v.y;
            M_BirdSpawn(r->x, Math16_Wrap(r->y - 32), v.x, v.y);
            M_BirdSpawn(M_RayX(r, false), Math16_Wrap(r->y - 32), -v.x, v.y);
        }
        if (g_B20M.frame % 6 == 0) {
            M_BirdsFire(assets, 2);
        }
        return;
    }
    if (g_B20M.frame == 100) {
        Video_LineErase(320, 120, Math16_Wrap(M_RayX(r, false) + 2), r->y);
        return;
    }
    if (g_B20M.frame % 10 == 0) {
        M_BirdsFire(assets, 2);
    }
    if (g_B20M.frame >= 300) {
        g_B20M.frame = 0;
    }
}
static void M_Semicircles(const B20M_ASSETS *const assets)
{
    int32_t count = 0, angle = 0, step = 0, speed = 0;
    if (g_B20M.frame == 50) {
        count = 20;
        step = 7;
        speed = 32;
    } else if (g_B20M.frame == 60) {
        count = 10;
        angle = 4;
        step = 12;
        speed = 48;
    } else if (g_B20M.frame == 70) {
        count = 8;
        angle = 4;
        step = 14;
        speed = 64;
    } else if (g_B20M.frame == 80) {
        count = 10;
        angle = 14;
        step = 10;
        speed = 48;
    } else if (g_B20M.frame > 150) {
        g_B20M.frame = 0;
    }
    for (int32_t i = 0; i < count; i++) {
        M_Single(assets, 320, 160, (uint8_t)angle, (int16_t)speed);
        angle += step;
    }
}
static void M_Stacks(const B20M_ASSETS *const assets)
{
    B20M_RAYS *const r = &g_B20M.stack_rays;
    if (g_B20M.frame < 50) {
        return;
    }
    if (g_B20M.frame == 50) {
        g_B20M.pattern_state = Boss_Rank(16, 13, 10, 8);
        r->x = 2;
        r->y = 399;
    } else if (g_B20M.frame < 130) {
        M_RaysDraw(assets, r, g_B20M.frame % 2 == 0, 320, 120, 8, 8, true);
        if (g_B20M.frame % 2 != 0) {
            r->image = M_DebrisNext(r->image);
        }
        r->x = Math16_Wrap(r->x + 8);
    } else if (g_B20M.frame == 130) {
        Video_LineErase(320, 120, Math16_Wrap(M_RayX(r, false) + 8), r->y);
        r->x = 0;
    } else if (g_B20M.frame > 150 && g_B20M.frame < 350) {
        if (g_B20M.frame % 10 == 0) {
            for (int16_t i = 0; i < 64; i += 16) {
                if (Boss_Player()->left + 16 - r->x > 32
                    || Boss_Player()->left + 16 - r->x < -32) {
                    M_Single(
                        assets, r->x, Math16_Wrap(r->y - 8), 192,
                        Math16_Wrap(i + 48));
                }
                const int16_t rx = M_RayX(r, false);
                if (Boss_Player()->left + 16 - rx > 32
                    || Boss_Player()->left + 16 - rx < -32) {
                    M_Single(
                        assets, rx, Math16_Wrap(r->y - 8), 192,
                        Math16_Wrap(i + 48));
                }
            }
            r->x = Math16_Wrap(r->x + 32);
            Sound_Effect(7);
        }
    } else if (g_B20M.frame == 350) {
        r->x = 0;
        r->y = 64;
    } else if (g_B20M.frame < 550) {
        if (g_B20M.frame % g_B20M.pattern_state == 0) {
            for (int32_t i = 0; i < 2; i++) {
                uint8_t angle =
                    M_Angle(400 - r->y, (M_Random(assets) % 640) - r->x);
                M_Single(assets, r->x, r->y, angle, 48);
                angle = M_Angle(400 - r->y, (M_Random(assets) % 640) - r->x);
                M_Single(assets, M_RayX(r, false), r->y, angle, 48);
            }
            r->x = Math16_Wrap(r->x + 32);
        }
    } else if (g_B20M.frame > 600) {
        g_B20M.frame = 0;
    }
}
static void M_Dotted(
    const B20M_ASSETS *const assets, const int16_t frame,
    const int16_t interval, const int16_t step, const int16_t initial,
    const int16_t frames)
{
    const int16_t radius = Math16_Wrap((frame / interval) * step + initial);
    if (frame == 1) {
        g_B20M.dotted_active = true;
    } else {
        if (!g_B20M.dotted_active || frame % interval != 0) {
            return;
        }
        M_EraseEllipse(
            assets, 320, 185, g_B20M.dotted_radius, g_B20M.dotted_radius, 1);
        if (frame >= frames) {
            g_B20M.dotted_active = false;
            return;
        }
        M_Ellipse(assets, 320, 185, radius, radius, 7, 1);
    }
    g_B20M.dotted_radius = radius;
}
static void M_SprayInit(B20M_SPRAY *const s, const B20M_ASSETS *const assets)
{
    s->x = 0;
    s->y = 400;
    s->speed = 112;
    s->subpattern = 0;
    g_B20M.pattern_state = Boss_Rank(32, 24, 18, 16);
}
static void M_SprayFire(
    B20M_SPRAY *const s, const B20M_ASSETS *const assets, const bool right)
{
    s->angle = M_Angle(s->y - 185, s->x - 320);
    M_Single(assets, 320, 185, s->angle, s->speed);
    s->speed = Math16_Wrap(s->speed - 4);
    s->x = Math16_Wrap(
        s->x + (right ? g_B20M.pattern_state : -g_B20M.pattern_state));
}
static void M_SprayOnce(const B20M_ASSETS *const assets)
{
    B20M_SPRAY *const s = &g_B20M.sprays[0];
    if (g_B20M.frame < 50) {
        return;
    }
    if (g_B20M.frame == 50) {
        M_Dotted(assets, 1, 4, 16, 32, 160);
    }
    M_Dotted(assets, Math16_Wrap(g_B20M.frame - 49), 4, 16, 32, 90);
    if (g_B20M.frame < 100) {
        return;
    }
    if (g_B20M.frame == 100) {
        M_SprayInit(s, assets);
    }
    if (s->subpattern == 0) {
        M_SprayFire(s, assets, true);
        if (s->x >= 640) {
            s->subpattern = 1;
            s->speed = 112;
            s->x = 640;
        }
    } else {
        M_SprayFire(s, assets, false);
        if (s->x < 0) {
            g_B20M.frame = 0;
        }
    }
}
static void M_SealRain(const B20M_ASSETS *const assets)
{
    B20M_RAYS *const r = &g_B20M.seal_rays;
    if (g_B20M.frame < 50) {
        return;
    }
    if (g_B20M.frame == 50) {
        r->x = 0;
        r->y = 399;
        r->image = 3;
        r->previous_image = 3;
        g_B20M.pattern_state = Boss_Rank(30, 35, 40, 45);
    }
    if (r->y > 185) {
        M_RaysDraw(assets, r, g_B20M.frame % 2 == 0, 320, 185, 32, 0, false);
        if (g_B20M.frame % 2 != 0) {
            r->previous_image = r->image;
            r->image = M_DebrisNext(r->image);
            r->y = Math16_Wrap(r->y - 8);
        }
        if (g_B20M.frame % 10 == 0) {
            Sound_Effect(6);
        }
        return;
    }
    Sound_Effect(3);
    Sprite_Restore16(
        M_DebrisX(r, false, 32, 0), Math16_Wrap(r->y - 16), 32, 40);
    Sprite_Restore16(M_DebrisX(r, true, 32, 0), Math16_Wrap(r->y - 16), 32, 50);
    Video_LineErase(320, 185, M_RayX(r, true), Math16_Wrap(r->y + 4));
    Video_LineErase(320, 185, M_RayX(r, false), Math16_Wrap(r->y + 4));
    for (int32_t i = 0; i < g_B20M.pattern_state; i++) {
        M_Rain(
            assets, (int16_t)((640 / g_B20M.pattern_state) * i), 185,
            (uint8_t)M_Random(assets), 16);
    }
    g_B20M.frame = 0;
}
static void M_SprayTwice(const B20M_ASSETS *const assets)
{
    B20M_SPRAY *const s = &g_B20M.sprays[1];
    if (g_B20M.frame < 80) {
        return;
    }
    if (g_B20M.frame == 80) {
        M_SprayInit(s, assets);
    }
    if (s->subpattern % 2 == 0) {
        M_SprayFire(s, assets, true);
        if (s->x >= 640) {
            s->subpattern = Math16_Wrap(s->subpattern + 1);
            s->speed = 112;
            s->x = 640;
        }
    } else {
        M_SprayFire(s, assets, false);
        if (s->x < 0) {
            s->subpattern = Math16_Wrap(s->subpattern + 1);
            s->speed = 112;
            s->x = 0;
            if (s->subpattern == 4) {
                g_B20M.frame = 0;
            }
        }
    }
}
static bool M_LeafVisible(const B20M_LEAVES *const p, const int32_t i)
{
    return p->left[i] >= 0 && p->left[i] < 6320 && p->top[i] >= 0
        && p->top[i] < 3920;
}
static void M_Leaves(const B20M_ASSETS *const assets, const int16_t interval)
{
    B20M_LEAVES *const p = &g_B20M.leaves;
    if (interval == 999) {
        for (int32_t i = 0; i < 30; i++) {
            p->flag[i] = 0;
        }
        return;
    }
    if (g_B20M.frame % interval == 0) {
        for (int32_t i = 0; i < 30; i++) {
            if (p->flag[i] != 0) {
                continue;
            }
            p->left[i] = M_Random(assets) % 6400;
            p->top[i] = 1200 + M_Random(assets) % 1000;
            const MATH16_VECTOR v = Math16_Aim(
                (MATH16_VECTOR) { p->left[i] / 10, p->top[i] / 10 },
                (MATH16_VECTOR) { M_Random(assets) % 640, 64 }, 40, 0);
            p->velocity_x[i] = v.x;
            p->velocity_y[i] = v.y;
            p->flag[i] = 1;
            break;
        }
    }
    if (g_B20M.frame % 2 != 0) {
        return;
    }
    for (int32_t i = 0; i < 30; i++) {
        if (p->flag[i] == 0) {
            continue;
        }
        if (p->flag[i] == 1 || p->flag[i] == 6) {
            if (M_LeafVisible(p, i)) {
                Sprite_Restore16(p->left[i] / 10, p->top[i] / 10, 16, 8);
            }
            p->left[i] = Math16_Wrap(p->left[i] + p->velocity_x[i]);
            p->top[i] = Math16_Wrap(p->top[i] + p->velocity_y[i]);
            if (p->flag[i] == 1) {
                if (p->top[i] < 640) {
                    p->flag[i] = 2;
                }
            } else {
                p->velocity_y[i] = Math16_Wrap(p->velocity_y[i] - 1);
                if (p->velocity_y[i] < -1) {
                    p->velocity_y[i] = 30 + M_Random(assets) % 20;
                    p->velocity_x[i] = p->velocity_x[i] < 0 ? 20 : -20;
                }
                if (p->top[i] / 10 > 400) {
                    p->flag[i] = 0;
                }
            }
        } else if (p->flag[i] <= 5) {
            if (M_LeafVisible(p, i)) {
                M_Erase(assets, p->left[i] / 10, p->top[i] / 10);
            }
            if (p->flag[i] == 5) {
                p->flag[i] = 6;
                p->velocity_y[i] = 3 + M_Random(assets) % 2;
                p->velocity_x[i] = 2;
            }
        }
    }
    for (int32_t i = 0; i < 30; i++) {
        if (p->flag[i] == 0) {
            continue;
        }
        if (p->flag[i] == 1) {
            if (M_LeafVisible(p, i)) {
                M_Mask(
                    assets, p->left[i] / 10, p->top[i] / 10, assets->spark, 7);
            }
        } else if (p->flag[i] <= 5) {
            if (M_LeafVisible(p, i)) {
                M_Grc(
                    assets, p->left[i], p->top[i], 3,
                    Math16_Wrap(p->flag[i] - 2), 7);
            }
            if (g_B20M.frame % 4 == 0) {
                p->flag[i] = Math16_Wrap(p->flag[i] + 1);
            }
        } else if (p->flag[i] == 6) {
            if (M_LeafVisible(p, i)) {
                M_Mask(
                    assets, p->left[i] / 10, p->top[i] / 10,
                    p->velocity_x[i] < 0 ? assets->leaf_left
                                         : assets->leaf_right,
                    7);
            }
            if (p->top[i] > 3680 && p->top[i] < 3920
                && p->left[i] / 10 > Boss_Player()->left
                && p->left[i] / 10 < Boss_Player()->left + 24
                && !Boss_Player()->invincible) {
                Boss_Player()->is_hit = true;
            }
        }
    }
}
static void M_Hud(const B20M_ASSETS *const assets)
{
    g_Stage.first_stage = true;
    StageTimer_Redraw(true, g_Stage.timer_initialized, 0);
    Hud_ScoreAndCombo();
}
void B20M_BeginEntrance(const B20M_ASSETS *const assets)
{
    ASSERT(
        !g_B20M.entrance_active && g_B20M.sequence == B20M_SEQUENCE_NONE
        && g_B20M.wait_kind == B20M_WAIT_NONE);
    g_Pellets.interlace = true;
    Tram_Fill((GFX_RECT) { 0, 0, 640, 400 }, TRAM_BLACK);
    M_Pi(assets, &assets->lower, true);
    Video_SelectDrawingPage(1);
    M_Pi(assets, &assets->upper, true);
    Video_SelectDrawingPage(0);
    StageObjects_Init(nullptr, 19, false, nullptr, nullptr);
    Sound_Load(13);
    Sound_Play();
    Tram_Clear();
    g_B20M.scroll_line = 399;
    g_B20M.sequence_stage = B20M_ENTRANCE_SCROLL;
    g_B20M.entrance_active = true;
}
bool B20M_ResumeEntrance(const B20M_ASSETS *const assets, const uint64_t tick)
{
    if (!g_B20M.entrance_active) {
        return true;
    }
    if (!M_Ready(assets, tick)) {
        return false;
    }
    for (;;) {
        switch (g_B20M.sequence_stage) {
        case B20M_ENTRANCE_SCROLL:
            if (g_B20M.scroll_line < 0) {
                g_B20M.sequence_stage = B20M_ENTRANCE_DIAGONAL;
                return M_Scroll(0);
            }
            g_B20M.sequence_stage = B20M_ENTRANCE_SCROLL_STEP;
            return M_Scroll(g_B20M.scroll_line);
        case B20M_ENTRANCE_SCROLL_STEP:
            M_CopyRows(assets, g_B20M.scroll_line, 1);
            g_B20M.scroll_line--;
            g_B20M.sequence_stage = B20M_ENTRANCE_SCROLL;
            if (!M_Delay(1)) {
                return false;
            }
            continue;
        case B20M_ENTRANCE_DIAGONAL:
            Video_SelectDrawingPage(1);
            M_Pi(assets, &assets->backgrounds[0], true);
            Video_SelectDrawingPage(0);
            g_B20M.sequence_stage = B20M_ENTRANCE_DIAGONAL_STEP;
            if (!Transition_BeginDiagonal(
                    &g_B20M.transition, &g_Palette, &g_Palette.saved,
                    M_TransitionTarget(false), assets->interleave, 40, tick)) {
                return false;
            }
            continue;
        case B20M_ENTRANCE_DIAGONAL_STEP:
            if (!Transition_Advance(
                    &g_B20M.transition, &g_Palette, &g_Palette.saved,
                    M_TransitionTarget(false), assets->interleave, tick)) {
                return false;
            }
            g_Rng = g_Session.frame_rand;
            g_B20M.entrance_active = false;
            g_B20M.sequence_stage = B20M_ENTRANCE_DONE;
            return true;
        default:
            ASSERT(false);
            return false;
        }
    }
}
static int16_t M_InvertRings(
    const B20M_ASSETS *const assets, const int16_t base)
{
    int16_t outside = 0;
    for (int32_t i = 32; i < 160; i++) {
        const int16_t r = Math16_Wrap(base * (i / 32));
        outside += !Video_Invert32(
            Math16_PolarY(304, r, i * 8), Math16_PolarX(180, r, i * 8), 2);
    }
    return outside;
}
static bool M_RingEntrance(const B20M_ASSETS *const assets, const uint64_t tick)
{
    if (!M_Ready(assets, tick)) {
        return false;
    }
    for (;;) {
        g_B20M.frame = Math16_Wrap(g_B20M.frame + 1);
        bool finish = false;
        if (g_B20M.frame == 1) {
            g_B20M.entrance_radius = 16;
        } else if (g_B20M.frame == 2) {
            M_InvertRings(assets, g_B20M.entrance_radius);
        } else if (g_B20M.frame > 2 && g_B20M.frame % 4 == 0) {
            M_InvertRings(assets, g_B20M.entrance_radius);
            g_B20M.entrance_radius = Math16_Wrap(g_B20M.entrance_radius + 16);
            finish = M_InvertRings(assets, g_B20M.entrance_radius) >= 128;
        }
        if (finish) {
            g_B20M.phase = B20M_PHASE_BIRDS;
            g_B20M.pattern = 0;
            g_B20M.patterns_done = 0;
            g_B20M.patterns_until_next = 1 + M_Random(assets) % 6;
            g_B20M.frame = 0;
            g_B20M.hp_bar_filled = false;
            M_Palette(assets, &g_B20M.boss_palette);
            g_B20M.shield.left = 304;
            g_B20M.shield.top = 144;
            M_Snap(assets);
            M_WandRaise(assets, true);
            M_BirdsReset();
            g_B20M.sequence = B20M_SEQUENCE_NONE;
            return true;
        }
        if (g_B20M.frame % 2 != 0 && !M_Delay(1)) {
            return false;
        }
    }
}
static void M_BeginTransform(const B20M_ASSETS *const assets)
{
    g_B20M.frame = 0;
    g_B20M.hit.frame = 399;
    Shots_EraseAndReset();
    Pellets_EraseAndResetMoving();
    ShootoutLasers_EraseAndReset(10);
    ORB *const orb = &g_Orb;
    orb->left = 600;
    orb->top = 280;
    orb->force = -8;
    orb->force_frame = 0;
    orb->velocity_x = ORB_X_LEFT_4;
    Boss_Player()->left = 304;
    Boss_Player()->deflecting = false;
    Boss_Player()->bomb_damaging = false;
    Player_Reset(g_Session.settings.difficulty);
    g_B20M.hit.invincible = false;
    g_Rng = g_Session.frame_rand;
    Sound_Fade();
    Video_SelectDrawingPage(1);
    // The absent BOSS6.GRP leaves the previous image and palette intact.
    M_Color(assets, 15, 0, 0, 0);
    g_B20M.boss_palette = g_Palette.saved;
    M_CopyOther(assets);
    Video_SelectDrawingPage(1);
    M_Pi(assets, &assets->backgrounds[4], false);
    Video_SelectDrawingPage(0);
    g_B20M.sequence = B20M_SEQUENCE_TRANSFORM;
    g_B20M.sequence_stage = B20M_TRANSFORM_SHAKE;
}
static bool M_Transform(const B20M_ASSETS *const assets, const uint64_t tick)
{
    if (!M_Ready(assets, tick)) {
        return false;
    }
    for (;;) {
        switch (g_B20M.sequence_stage) {
        case B20M_TRANSFORM_SHAKE:
            g_B20M.frame = Math16_Wrap(g_B20M.frame + 1);
            if (g_B20M.frame < 200) {
                if (g_B20M.frame % 2 == 0) {
                    M_Color(assets, 15, 15, 15, 15);
                } else {
                    M_Palette(assets, &g_B20M.boss_palette);
                }
                g_B20M.sequence_stage = B20M_TRANSFORM_SHAKE_STEP;
                return M_Scroll(g_B20M.frame % 2 == 0 ? 416 : 386);
            }
            if (g_B20M.frame == 200) {
                M_Palette(assets, &g_B20M.boss_palette);
                g_B20M.sequence_stage = B20M_TRANSFORM_SHAKE_STEP;
                return M_Scroll(0);
            }
            for (int32_t i = 0; i < 3; i++) {
                g_Palette.resource.colors[15][i] = 0;
            }
            ASSERT(Transition_BeginDiagonal(
                &g_B20M.transition, &g_Palette, &g_Palette.saved,
                M_TransitionTarget(true), assets->interleave, 0, tick));
            Video_SelectDrawingPage(1);
            Sound_Load(14);
            Sound_Play();
            M_Pi(assets, &assets->backgrounds[5], true);
            M_Color(assets, 6, 0, 0, 0);
            M_CopyOther(assets);
            M_Hud(assets);
            g_B20M.sequence_stage = B20M_TRANSFORM_SECOND_FORM;
            return M_Scroll(0);
        case B20M_TRANSFORM_SHAKE_STEP:
            if (g_B20M.frame < 200 && g_B20M.frame % 4 == 0) {
                Sound_Effect(9);
            }
            g_B20M.sequence_stage = B20M_TRANSFORM_SHAKE;
            if (!M_Delay(1)) {
                return false;
            }
            continue;
        case B20M_TRANSFORM_SECOND_FORM:
            g_B20M.frame = 0;
            g_B20M.hit.frame = 0;
            g_B20M.pattern = 0;
            g_B20M.patterns_done = 0;
            g_B20M.phase = B20M_PHASE_SECOND_FORM;
            Boss_Player()->invincibility_time = 0;
            Boss_Player()->invincible = false;
            g_B20M.boss_palette = g_Palette.saved;
            g_B20M.stage_palette = g_B20M.boss_palette;
            g_B20M.hp = 6;
            g_HudHp.first_white = 10;
            g_HudHp.first_redwhite = 3;
            g_B20M.hp_bar_filled = false;
            M_Leaves(assets, 999);
            g_B20M.sequence = B20M_SEQUENCE_NONE;
            return true;
        default:
            ASSERT(false);
            return false;
        }
    }
}
static void M_BeginDefeat(const B20M_ASSETS *const assets)
{
    Video_SelectDrawingPage(1);
    M_Pi(assets, &assets->backgrounds[5], true);
    M_Color(assets, 6, 0, 0, 0);
    g_B20M.boss_palette = g_Palette.saved;
    M_CopyOther(assets);
    Video_SelectDrawingPage(0);
    g_B20M.sequence = B20M_SEQUENCE_DEFEAT;
    g_B20M.sequence_stage = B20M_DEFEAT_SHAKE;
}
static bool M_Defeat(const B20M_ASSETS *const assets, const uint64_t tick)
{
    if (!M_Ready(assets, tick)) {
        return false;
    }
    for (;;) {
        switch (g_B20M.sequence_stage) {
        case B20M_DEFEAT_SHAKE:
            g_B20M.frame = Math16_Wrap(g_B20M.frame + 1);
            if (g_B20M.frame < 230) {
                g_B20M.sequence_stage = B20M_DEFEAT_SHAKE_STEP;
                return M_Scroll(g_B20M.frame % 2 == 0 ? 416 : 386);
            }
            g_B20M.sequence_stage = B20M_DEFEAT_SHAKE_STEP;
            continue;
        case B20M_DEFEAT_SHAKE_STEP:
            if (g_B20M.frame < 200 && g_B20M.frame % 4 == 0) {
                Sound_Effect(9);
            }
            if (g_B20M.frame == 170) {
                Sound_Fade();
            }
            if (g_B20M.frame > 190) {
                if (g_B20M.frame % 2 == 0) {
                    for (int32_t i = 0; i < 16; i++) {
                        for (int32_t j = 0; j < 3; j++) {
                            g_Palette.saved.colors[i][j]--;
                        }
                    }
                    M_Palette(assets, &g_Palette.saved);
                }
                if (g_B20M.frame > 230) {
                    g_B20M.sequence_stage = B20M_DEFEAT_BONUS;
                    return M_Scroll(0);
                }
            }
            g_B20M.sequence_stage = B20M_DEFEAT_SHAKE;
            if (!M_Delay(2)) {
                return false;
            }
            continue;
        case B20M_DEFEAT_BONUS:
            for (int32_t i = 0; i < 5; i++) {
                g_Stage.score += 10000;
            }
            g_Stage.game_cleared = true;
            g_B20M.finished = true;
            g_B20M.sequence = B20M_SEQUENCE_NONE;
            return true;
        default:
            ASSERT(false);
            return false;
        }
    }
}
static void M_Hit(const B20M_ASSETS *const assets)
{
    const ORB *const orb = &g_Orb;
    const bool collision = (orb->left >= 314 && orb->left <= 346
                            && orb->top >= 104 && orb->top <= 136)
        || BossEntity_HitTestOrb(&g_B20M.shield);
    const uint8_t colors[] = { 3, 4, 5 };
    BossHit_UpdateAndRender(
        &g_B20M.hit, &g_B20M.hp, &g_B20M.boss_palette, colors, 3, 10000,
        collision, (BOSS_SHOT_HITBOX) { 314, 104, 64, 32 });
}
static void M_PhaseNext(const B20M_ASSETS *const assets, const int8_t phase)
{
    g_B20M.phase = phase;
    g_B20M.frame = 0;
    M_Snap(assets);
}
static void M_PatternNext(const int16_t first, const int16_t last)
{
    g_B20M.pattern = last == 1   ? Math16_Wrap(1 - g_B20M.pattern)
        : g_B20M.pattern == last ? first
                                 : Math16_Wrap(g_B20M.pattern + 1);
    g_B20M.patterns_done = Math16_Wrap(g_B20M.patterns_done + 1);
}
static void M_Pulse(const B20M_ASSETS *const assets)
{
    if (g_B20M.frame % 10 != 0) {
        return;
    }
    M_Palette(assets, &g_B20M.boss_palette);
    int8_t *const color = g_Palette.saved.colors[6];
    if (g_B20M.patterns_done == 0) {
        if (color[0] < 10) {
            color[0]++;
        } else {
            g_B20M.patterns_done = 1;
        }
        if (color[1] < 10) {
            color[1]++;
        }
        if (color[2] < 6) {
            color[2]++;
        }
    } else {
        if (color[0] > 0) {
            color[0]--;
        } else {
            g_B20M.patterns_done = 0;
        }
        if (color[1] > 0) {
            color[1]--;
        }
        if (color[2] > 0) {
            color[2]--;
        }
    }
    M_Palette(assets, &g_Palette.saved);
    g_B20M.boss_palette = g_Palette.saved;
}
static bool M_Pattern(const B20M_ASSETS *const assets)
{
    switch (g_B20M.call_phase) {
    case B20M_PHASE_BIRDS:
        if (g_B20M.pattern == 0) {
            M_PurpleLasers(assets);
        } else if (g_B20M.pattern == 1) {
            M_Vortices(assets);
        } else if (g_B20M.pattern == 2) {
            M_Eggs(assets);
        }
        break;
    case B20M_PHASE_VERTICAL:
        if (g_B20M.pattern == 0) {
            M_Flake(assets);
            M_Rings(assets);
        } else if (g_B20M.pattern == 1) {
            M_Sling(assets);
            M_Rings(assets);
        } else if (g_B20M.frame < 60) {
            g_B20M.frame = 0;
        }
        break;
    case B20M_PHASE_WAVY:
        if (g_B20M.pattern == 0) {
            M_FourLasers(assets);
        } else if (g_B20M.pattern == 1 || g_B20M.pattern == 4) {
            return M_Shake(assets);
        } else if (g_B20M.pattern == 2 || g_B20M.pattern == 5) {
            M_FourLasers(assets);
            M_TopRain(assets);
        } else if (g_B20M.pattern == 3) {
            M_Radial(assets);
        }
        break;
    case B20M_PHASE_VERTICAL_BIRDS:
        if (g_B20M.pattern == 0) {
            M_SymmetricBirds(assets);
        } else if (g_B20M.pattern == 1) {
            M_Semicircles(assets);
        } else if (g_B20M.pattern == 2) {
            M_Stacks(assets);
        }
        break;
    case B20M_PHASE_SECOND_FORM:
        switch (g_B20M.pattern) {
        case 0:
            g_B20M.pattern_state = Boss_Rank(56, 32, 24, 20);
            if (g_B20M.frame > 100) {
                g_B20M.frame = 0;
            }
            break;
        case 1:
            M_SprayOnce(assets);
            break;
        case 2:
            M_SealRain(assets);
            break;
        case 3:
            M_SprayTwice(assets);
            break;
        case 4:
            M_Leaves(assets, g_B20M.pattern_state);
            if (g_B20M.frame > 300) {
                g_B20M.frame = 0;
            }
            break;
        case 5:
            M_Leaves(assets, 500);
            if (g_B20M.frame > 200) {
                g_B20M.frame = 0;
            }
            break;
        case 6:
            M_Leaves(assets, 500);
            M_SprayOnce(assets);
            break;
        case 7:
            M_Leaves(assets, 500);
            M_SealRain(assets);
            break;
        case 8:
            M_Leaves(assets, 32);
            M_SprayTwice(assets);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return true;
}
static bool M_Tail(const B20M_ASSETS *const assets)
{
    const int8_t phase = g_B20M.call_phase;
    if (g_B20M.frame == 0) {
        M_PatternNext(
            phase == B20M_PHASE_WAVY              ? 10
                : phase == B20M_PHASE_SECOND_FORM ? 4
                                                  : 0,
            phase == B20M_PHASE_BIRDS || phase == B20M_PHASE_VERTICAL_BIRDS ? 2
                : phase == B20M_PHASE_VERTICAL                              ? 1
                : phase == B20M_PHASE_WAVY                                  ? 5
                                           : 8);
    }
    M_Hit(assets);
    if (phase == B20M_PHASE_SECOND_FORM) {
        M_Pulse(assets);
        if (g_B20M.hp <= 0) {
            Sound_Stop();
            g_B20M.phase = B20M_PHASE_DEFEAT;
            g_B20M.frame = 0;
            g_B20M.hit.frame = 0;
        }
    } else {
        if (g_B20M.hp <= 0) {
            g_B20M.phase = B20M_PHASE_TRANSFORM;
        }
        if (g_B20M.patterns_done >= g_B20M.patterns_until_next
            && !g_B20M.hit.invincible) {
            g_B20M.patterns_done = 0;
            g_B20M.phase = phase + 1;
            g_B20M.pattern = 0;
            g_B20M.frame = 0;
        }
    }
    return true;
}
static void M_AfterGust(const B20M_ASSETS *const assets)
{
    const int8_t phase = g_B20M.call_phase;
    M_Dress(assets);
    if (g_B20M.frame != 0) {
        return;
    }
    M_PhaseNext(
        assets, phase == B20M_PHASE_GUST_4 ? B20M_PHASE_BIRDS : phase + 1);
    if (phase != B20M_PHASE_GUST_1) {
        g_B20M.boss_palette = g_Palette.saved;
    }
    g_B20M.patterns_until_next = phase == B20M_PHASE_GUST_1
        ? 1 + M_Random(assets) % 5
        : phase == B20M_PHASE_GUST_2 ? 3 + M_Random(assets) % 4
        : phase == B20M_PHASE_GUST_3 ? 2 + M_Random(assets) % 5
                                     : 1 + M_Random(assets) % 6;
}
bool B20M_UpdateAndRender(const B20M_ASSETS *const assets, const uint64_t tick)
{
    if (g_B20M.finished) {
        return true;
    }
    if (g_B20M.sequence == B20M_SEQUENCE_RINGS) {
        return M_RingEntrance(assets, tick);
    }
    if (g_B20M.sequence == B20M_SEQUENCE_TRANSFORM) {
        return M_Transform(assets, tick);
    }
    if (g_B20M.sequence == B20M_SEQUENCE_DEFEAT) {
        return M_Defeat(assets, tick);
    }
    if (g_B20M.resume_stage != B20M_RESUME_NONE) {
        if (!M_Ready(assets, tick)) {
            return false;
        }
        const uint8_t resume = g_B20M.resume_stage;
        g_B20M.resume_stage = B20M_RESUME_NONE;
        if (resume == B20M_RESUME_SHAKE) {
            Sound_Effect(7);
            return M_Tail(assets);
        }
        if (resume == B20M_RESUME_SHAKE_END) {
            g_B20M.frame = 0;
            return M_Tail(assets);
        }
        if (resume == B20M_RESUME_GUST_END) {
            const int16_t image = g_B20M.call_phase == B20M_PHASE_GUST_1 ? 1
                : g_B20M.call_phase == B20M_PHASE_GUST_2                 ? 2
                : g_B20M.call_phase == B20M_PHASE_GUST_3                 ? 3
                                                                         : 0;
            M_GustFinish(assets, image);
            M_AfterGust(assets);
            return true;
        }
        if (!M_Pattern(assets)) {
            return false;
        }
        return M_Tail(assets);
    }
    if (g_B20M.phase == B20M_PHASE_START) {
        g_B20M.frame = -1;
        g_B20M.hit.frame = 0;
        g_B20M.hit.invincible = false;
        g_B20M.boss_palette = g_Palette.saved;
        g_B20M.stage_palette = g_B20M.boss_palette;
        g_Rng = g_Session.frame_rand;
        g_B20M.sequence = B20M_SEQUENCE_RINGS;
        return M_RingEntrance(assets, tick);
    }
    if (g_B20M.phase == B20M_PHASE_TRANSFORM) {
        M_BeginTransform(assets);
        return M_Transform(assets, tick);
    }
    if (g_B20M.phase == B20M_PHASE_DEFEAT) {
        M_BeginDefeat(assets);
        return M_Defeat(assets, tick);
    }
    g_B20M.call_phase = g_B20M.phase;
    if (g_B20M.phase >= B20M_PHASE_GUST_1 && g_B20M.phase <= B20M_PHASE_GUST_4
        && g_B20M.phase % 2 == 0) {
        g_B20M.frame = Math16_Wrap(g_B20M.frame + 1);
        if (!M_Gust(assets)) {
            return false;
        }
        M_AfterGust(assets);
        return true;
    }
    if (g_B20M.phase != B20M_PHASE_BIRDS && g_B20M.phase != B20M_PHASE_VERTICAL
        && g_B20M.phase != B20M_PHASE_WAVY
        && g_B20M.phase != B20M_PHASE_VERTICAL_BIRDS
        && g_B20M.phase != B20M_PHASE_SECOND_FORM) {
        return true;
    }
    if ((g_B20M.phase == B20M_PHASE_BIRDS
         || g_B20M.phase == B20M_PHASE_SECOND_FORM)
        && !g_B20M.hp_bar_filled) {
        g_B20M.hp_bar_filled = HudHp_Increment(g_B20M.hp, g_B20M.frame);
    }
    g_B20M.frame = Math16_Wrap(g_B20M.frame + 1);
    g_B20M.hit.frame = Math16_Wrap(g_B20M.hit.frame + 1);
    if (g_B20M.phase != B20M_PHASE_SECOND_FORM) {
        M_Shield(assets);
        M_Dress(assets);
    }
    if (g_B20M.phase == B20M_PHASE_VERTICAL
        || g_B20M.phase == B20M_PHASE_VERTICAL_BIRDS) {
        M_Vertical(assets, g_B20M.phase == B20M_PHASE_VERTICAL_BIRDS);
    } else if (g_B20M.phase == B20M_PHASE_WAVY) {
        M_Wavy(assets);
    } else if (g_B20M.phase == B20M_PHASE_SECOND_FORM) {
        M_Horizontal(assets);
    }
    if ((g_B20M.phase == B20M_PHASE_BIRDS
         || g_B20M.phase == B20M_PHASE_VERTICAL_BIRDS)
        && !M_BirdsUpdate(assets, tick)) {
        return false;
    }
    if (!M_Pattern(assets)) {
        return false;
    }
    return M_Tail(assets);
}
