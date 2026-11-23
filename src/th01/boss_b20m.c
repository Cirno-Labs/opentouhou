#include "th01/boss_b20m.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/assert.h"
#include "th01/finite.h"
#include "th01/shape.h"
#include "th01/sprite.h"
#include "th01/sprite.h"

#include <string.h>

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
static RESULT M_LoadPi(
    VFS *const vfs, const char *const name, const ALLOCATOR *const a,
    PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Pi_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}
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
static RESULT M_LoadAudio(
    VFS *const vfs, const ALLOCATOR *const a, B20M_ASSETS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "TENSI.MDT", VFS_IN_MEMORY, &reader));
    RESULT result = MdrvData_LoadMusic(&reader, a, &out->music);
    Reader_Close(&reader);
    MUST(result);
    MUST(Vfs_Open(vfs, "SYUGEN.MDT", VFS_IN_MEMORY, &reader));
    result = MdrvData_LoadMusic(&reader, a, &out->second_music);
    Reader_Close(&reader);
    MUST(result);
    MUST(Vfs_Open(vfs, "ZIGOKU.MDE", VFS_IN_MEMORY, &reader));
    result = MdrvData_LoadEffects(&reader, a, &out->effects);
    Reader_Close(&reader);
    return result;
}
static RESULT M_LoadMasks(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, B20M_ASSETS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "REIIDEN.EXE", VFS_IN_MEMORY, &reader));
    INTERLEAVE_ASSETS *interleave;
    RESULT result =
        Interleave_LoadAssets(&reader, scratch, scratch_size, a, &interleave);
    READER image;
    if (IS_OK(result)) {
        Reader_Seek(&reader, 0);
        result = ReiidenImage_Open(&reader, scratch, scratch_size, &image);
    }
    uint8_t masks[40];
    if (IS_OK(result)) {
        Reader_Seek(&image, 0x39252);
        Reader_ReadBytes(&image, masks, 16);
        Reader_Seek(&image, 0x3967d);
        Reader_ReadBytes(&image, masks + 16, 24);
        result = Reader_Result(&image);
    }
    Reader_Close(&reader);
    MUST(result);
    uint8_t *const storage = Allocator_Alloc(a, sizeof(masks));
    memcpy(storage, masks, sizeof(masks));
    out->star = storage;
    out->flake = storage + 8;
    out->spark = storage + 16;
    out->leaf_left = storage + 24;
    out->leaf_right = storage + 32;
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
        a->music.bytes == nullptr || a->second_music.bytes == nullptr
            || a->effects.bytes == nullptr || a->interleave == nullptr
            || a->star == nullptr || a->flake == nullptr || a->spark == nullptr
            || a->leaf_left == nullptr || a->leaf_right == nullptr,
        "Missing Sariel audio or shape masks");
    return OK;
}
RESULT B20M_LoadAssets(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, B20M_ASSETS *const out)
{
    *out = (B20M_ASSETS) {};
    B20M_ASSETS loaded = {};
    MUST(M_LoadBos(vfs, "BOSS6_1.BOS", a, &loaded.shield));
    MUST(M_LoadBos(vfs, "BOSS6_2.BOS", a, &loaded.wand));
    MUST(M_LoadBos(vfs, "BOSS6_3.BOS", a, &loaded.dress));
    MUST(M_LoadGrc(vfs, "BOSS6GR1.GRC", a, &loaded.vortex));
    MUST(M_LoadGrc(vfs, "BOSS6GR2.GRC", a, &loaded.bird));
    MUST(M_LoadGrc(vfs, "BOSS6GR3.GRC", a, &loaded.cross));
    MUST(M_LoadGrc(vfs, "BOSS6GR4.GRC", a, &loaded.splash));
    MUST(M_LoadPi(vfs, "BOSS6_L.GRP", a, &loaded.lower));
    MUST(M_LoadPi(vfs, "BOSS6_H.GRP", a, &loaded.upper));
    const char *const names[] = { "BOSS6_A1.GRP", "BOSS6_A2.GRP",
                                  "BOSS6_A3.GRP", "BOSS6_A4.GRP",
                                  "BOSS6_A5.GRP", "BOSS6_A6.GRP" };
    for (int32_t i = 0; i < 6; i++) {
        MUST(M_LoadPi(vfs, names[i], a, &loaded.backgrounds[i]));
    }
    MUST(M_LoadAudio(vfs, a, &loaded));
    MUST(M_LoadMasks(vfs, scratch, scratch_size, a, &loaded));
    MUST(B20M_ValidateAssets(&loaded));
    *out = loaded;
    return OK;
}
void B20M_Init(B20M *const b, const ALLOCATOR *const a)
{
    *b = (B20M) { .ray_previous_x = 999,
                  .ray_previous_y = 999,
                  .flake_state = -1,
                  .wand_background = Allocator_AllocZ(a, 128 * 96) };
}
void B20M_Setup(B20M *const b, const B20M_CONTEXT *const c)
{
    ASSERT(b->sequence == 0 && b->wait_kind == 0 && !b->entrance_active);
    ASSERT(Result_Ignore(B20M_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->shield, c->sprites, 0, &c->assets->shield)));
    ASSERT(Result_Ignore(
        BossAnimation_Bind(&b->wand, c->sprites, 0, &c->assets->wand)));
    ASSERT(Result_Ignore(
        BossAnimation_Bind(&b->dress, c->sprites, 1, &c->assets->dress)));
    BossEntity_SetPosition(
        &b->shield, 304, 144, (BOSS_BOUNDS) { 0, 640, 0, 400 });
    BossEntity_SetOrbHitbox(&b->shield, (BOSS_BOUNDS) { -16, 64, -16, 64 });
    b->shield.orb_inactive = false;
    b->dress.left = 280;
    b->dress.top = 192;
    b->wand.left = 296;
    b->wand.top = 48;
    b->hp = 18;
    b->phase = 0;
    b->frame = 0;
    b->finished = false;
    b->boss_palette = c->hit->palette->saved;
    c->hit->health->first_white = 8;
    c->hit->health->first_redwhite = 2;
}

static VIDEO *M_Video(const B20M_CONTEXT *const c)
{
    return c->hit->display->video;
}
static PLAYER *M_Player(const B20M_CONTEXT *const c)
{
    return c->hit->display->player;
}
static int16_t M_W(const int32_t x)
{
    return Math16_Wrap(x);
}
static int16_t M_Rank(
    const B20M_CONTEXT *const c, const int16_t e, const int16_t n,
    const int16_t h, const int16_t l)
{
    const int32_t r = c->hit->display->rank;
    return r == 0 ? e : r == 2 ? h : r == 3 ? l : n;
}
static uint16_t M_Random(const B20M_CONTEXT *const c)
{
    return MasterRng_Next(c->rng);
}
static uint8_t M_Angle(const int32_t y, const int32_t x)
{
    return Math16_Angle(M_W(y), M_W(x));
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
static void M_Effect(const B20M_CONTEXT *const c, const int16_t e)
{
    c->hit->effect(c->hit->user, e);
}
static void M_Audio(const B20M_CONTEXT *const c, const B20M_AUDIO_ACTION action)
{
    c->audio(c->hit->user, action, c->assets);
}
static void M_Pi(
    const B20M_CONTEXT *const c, const PI_IMAGE *const image, const bool show)
{
    const VIDEO_IMAGE view = { .pixels = image->pixels,
                               .width = image->width,
                               .height = image->height,
                               .stride = image->width,
                               .key = -1 };
    Video_Blit(
        M_Video(c), &view, (GFX_RECT) { 0, 0, image->width, image->height }, 0,
        0);
    Palette_Import(&c->hit->palette->resource, image->palette);
    if (show) {
        Palette_Set(c->hit->palette, M_Video(c), &c->hit->palette->resource);
    }
}
static void M_Palette(const B20M_CONTEXT *const c, const PALETTE *const p)
{
    Palette_Set(c->hit->palette, M_Video(c), p);
}
static void M_Color(
    const B20M_CONTEXT *const c, const uint8_t i, const int16_t r,
    const int16_t g, const int16_t blue)
{
    Palette_SetColor(c->hit->palette, M_Video(c), i, r, g, blue);
}
static void M_CopyOther(const B20M_CONTEXT *const c)
{
    const int32_t page = M_Video(c)->drawing_page;
    Video_Copy(M_Video(c), page, (GFX_RECT) { 0, 0, 640, 400 }, 1 - page, 0, 0);
}
static void M_CopyRows(
    const B20M_CONTEXT *const c, const int16_t top, const int16_t height)
{
    for (int32_t i = 0; i < height * 80; i++) {
        const uint16_t offset = (uint16_t)(top * 80 + i) & 0x7fff;
        if (offset < 32000) {
            memcpy(
                M_Video(c)->pages[0] + offset * 8,
                M_Video(c)->pages[1] + offset * 8, 8);
        }
    }
    Video_SelectDrawingPage(M_Video(c), 0);
}
static void M_Snap(B20M *const b, const B20M_CONTEXT *const c)
{
    const int32_t left = b->wand.left / 8 * 8;
    for (int32_t y = 0; y < 96; y++) {
        memcpy(
            b->wand_background + y * 128,
            M_Video(c)->pages[1] + (b->wand.top + y) * 640 + left, 128);
    }
    Video_SelectDrawingPage(M_Video(c), 0);
}
static void M_WandPut(const B20M *const b, const B20M_CONTEXT *const c)
{
    const VIDEO_IMAGE v = { .pixels = b->wand_background,
                            .width = 128,
                            .height = 96,
                            .stride = 128,
                            .key = -1 };
    Video_Blit(
        M_Video(c), &v, (GFX_RECT) { 0, 0, 128, 96 }, b->wand.left / 8 * 8,
        b->wand.top);
}
static void M_WandLower(const B20M *const b, const B20M_CONTEXT *const c)
{
    Video_SelectDrawingPage(M_Video(c), 1);
    M_WandPut(b, c);
    Video_SelectDrawingPage(M_Video(c), 0);
    M_WandPut(b, c);
}
static bool M_WandRaise(
    B20M *const b, const B20M_CONTEXT *const c, const bool restart)
{
    if (restart) {
        b->wand_frame = 0;
    } else if (b->wand_frame == 0) {
        b->wand_frame = 1;
    } else {
        b->wand_frame = M_W(b->wand_frame + 1);
        if (b->wand_frame == 2 || b->wand_frame == 16) {
            b->wand.image = b->wand_frame == 2 ? 0 : 1;
            Video_SelectDrawingPage(M_Video(c), 1);
            BossAnimation_Put(&b->wand, c->sprites, M_Video(c));
            Video_SelectDrawingPage(M_Video(c), 0);
            BossAnimation_Put(&b->wand, c->sprites, M_Video(c));
            if (b->wand_frame == 16) {
                b->wand_frame = 0;
                return true;
            }
        }
    }
    return false;
}
static void M_Shield(B20M *const b, const B20M_CONTEXT *const c)
{
    if (b->frame % 10 != 0) {
        return;
    }
    b->shield.image = (b->frame % 40) / 10;
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(M_Video(c), page);
        b->shield.lock_frame = 0;
        BossEntity_LockedMovePut(
            &b->shield, c->sprites, M_Video(c), 0, 0, 1, false);
    }
}
static void M_Dress(B20M *const b, const B20M_CONTEXT *const c)
{
    if (b->frame % 15 != 0) {
        return;
    }
    b->dress.image = (b->frame % 60) / 15;
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(M_Video(c), page);
        BossAnimation_Put(&b->dress, c->sprites, M_Video(c));
    }
}
static void M_Group(
    const B20M_CONTEXT *const c, const int16_t x, const int16_t y,
    const int16_t group, const int16_t speed)
{
    Pellets_AddGroup(
        c->pellets, c->pellet_shared, M_Player(c), c->rng,
        c->hit->display->rank, x, y, group, speed);
}
static void M_Single(
    const B20M_CONTEXT *const c, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed)
{
    Pellets_AddSingle(
        c->pellets, c->pellet_shared, c->hit->display->rank, x, y, angle, speed,
        PELLET_REGULAR, 0, 0, 0);
}
static void M_Rain(
    const B20M_CONTEXT *const c, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed)
{
    Pellets_AddSingle(
        c->pellets, c->pellet_shared, c->hit->display->rank, x, y, angle, speed,
        PELLET_GRAVITY, 1, 0, 0);
}
static void M_Laser(
    const B20M_CONTEXT *const c, const int32_t index, const int16_t x,
    const int16_t y, const int16_t tx, const int16_t ty, const int16_t speed,
    const uint8_t color, const int16_t age, const int16_t width)
{
    ShootoutLasers_Spawn(
        &c->lasers->slots[((index % 10) + 10) % 10], x, y, tx, ty, speed, color,
        age, width);
}
static void M_Erase(
    const B20M_CONTEXT *const c, const int16_t x, const int16_t y)
{
    Sprite_Restore16(M_Video(c), x, y, 48, 32);
}
static void M_Grc(
    const B20M_CONTEXT *const c, const int16_t left, const int16_t top,
    const int16_t bank, const int16_t image, const uint8_t color)
{
    const GRC *const banks[] = { &c->assets->vortex, &c->assets->bird,
                                 &c->assets->cross, &c->assets->splash };
    const GRC *const grc = banks[bank];
    if (image < 0 || image >= grc->count || left < -32 || left >= 640
        || top < -32 || top >= 400) {
        return;
    }
    int16_t row = M_W(top * 80 + left / 8 - (left % 8 < 0));
    const uint8_t *const pixels = grc->coverage + image * 1024;
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
                            M_Video(c)->pages[M_Video(c)->drawing_page]
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
static void M_Mask(
    const B20M_CONTEXT *const c, const int16_t left, const int16_t top,
    const uint8_t *const mask, const uint8_t color)
{
    if (left < 0 || left >= 640 || top < 0 || top >= 400) {
        return;
    }
    for (int32_t y = 0; y < 8; y++) {
        for (int32_t x = 0; x < 8; x++) {
            const int32_t offset = (top + y) * 640 + left + x;
            if (offset < VIDEO_PAGE_SIZE && (mask[y] & (0x80u >> x)) != 0) {
                M_Video(c)->pages[M_Video(c)->drawing_page][offset] =
                    color & 15;
            }
        }
    }
}
static void M_Ellipse(
    const B20M_CONTEXT *const c, const int16_t x, const int16_t y,
    const int16_t rx, const int16_t ry, const uint8_t color, const uint8_t step)
{
    Shape_EllipseArcPut(M_Video(c), x, y, rx, ry, color, step, 0, 255);
}
static void M_EraseEllipse(
    const B20M_CONTEXT *const c, const int16_t x, const int16_t y,
    const int16_t rx, const int16_t ry, const uint8_t step)
{
    Shape_EllipseArcErase(M_Video(c), x, y, rx, ry, step, 0, 255);
}
static void M_Ray(
    B20M *const b, const B20M_CONTEXT *const c, const int16_t ox,
    const int16_t oy, const int16_t x, const int16_t y, const uint8_t color)
{
    if (color == 99) {
        b->ray_previous_x = b->ray_previous_y = 999;
        return;
    }
    if (b->ray_previous_x >= 0 && b->ray_previous_x < 640
        && b->ray_previous_y >= 0 && b->ray_previous_y < 400) {
        Video_LineErase(
            M_Video(c), ox, oy, b->ray_previous_x, b->ray_previous_y);
    }
    if (x >= 0 && x < 640 && y >= 0 && y < 400) {
        Video_Line(M_Video(c), ox, oy, x, y, color);
    }
    b->ray_previous_x = x;
    b->ray_previous_y = y;
}
static bool M_Delay(
    B20M *const b, FRAME_VSYNC *const vsync, const uint16_t frames)
{
    if (Frame_DelayReady(vsync, frames)) {
        return true;
    }
    b->wait_kind = 2;
    b->wait_frames = frames;
    return false;
}
static bool M_Scroll(
    B20M *const b, FRAME_VSYNC *const vsync, const int16_t line)
{
    b->raw_scroll = line;
    b->wait_kind = 1;
    Frame_BeginWait(&b->wait, vsync);
    return false;
}
static bool M_Ready(
    B20M *const b, const B20M_CONTEXT *const c, FRAME_VSYNC *const vsync,
    const uint64_t tick)
{
    if (b->wait_kind == 1) {
        if (!Frame_WaitReady(&b->wait, vsync)) {
            return false;
        }
        Video_SetScroll(M_Video(c), b->raw_scroll);
    } else if (b->wait_kind == 2) {
        if (!Frame_DelayReady(vsync, b->wait_frames)) {
            return false;
        }
    } else if (b->wait_kind == 3 && tick - b->wait_tick < 6) {
        return false;
    }
    b->wait_kind = 0;
    return true;
}
static TRANSITION_CONTEXT M_TransitionContext(
    B20M *const b, const B20M_CONTEXT *const c, const bool fade)
{
    return (TRANSITION_CONTEXT) { .video = M_Video(c),
                                  .palette = c->hit->palette,
                                  .current = &c->hit->palette->saved,
                                  .target = fade ? &c->hit->palette->resource
                                                 : &c->hit->palette->saved,
                                  .assets = c->assets->interleave };
}
static void M_BirdsReset(B20M *const b)
{
    for (int32_t i = 0; i < 30; i++) {
        b->birds.alive[i] = false;
    }
}
static void M_BirdSpawn(
    B20M *const b, const double x, const double y, const double vx,
    const double vy)
{
    if (x == 0) {
        return;
    }
    for (int32_t i = 0; i < 30; i++) {
        if (!b->birds.alive[i]) {
            b->birds.alive[i] = true;
            b->birds.left[i] = x;
            b->birds.top[i] = y;
            b->birds.velocity_x[i] = vx;
            b->birds.velocity_y[i] = vy;
            b->birds.reserved[i] = 1;
            b->birds.hatch_time[i] = 25;
            b->birds.hatch_frames[i] = 25;
            return;
        }
    }
}
static void M_BirdsFire(
    B20M *const b, const B20M_CONTEXT *const c, const int16_t group)
{
    for (int32_t i = 0; i < 30; i++) {
        if (!b->birds.alive[i] || b->birds.hatch_time[i] != 0) {
            continue;
        }
        const int16_t x = M_PixelAdd(b->birds.left[i], 12),
                      y = M_PixelAdd(b->birds.top[i], 12);
        if (group == 0) {
            M_Group(c, x, y, PELLET_GROUP_SINGLE_AIMED, 48);
        } else if (group == 1) {
            for (int32_t j = 0; j < 6; j++) {
                M_Single(c, x, y, (uint8_t)(j * 42), 64);
            }
        } else {
            M_Rain(c, x, y, (uint8_t)((M_Random(c) & 127) + 128), 64);
        }
    }
}
static bool M_BirdsUpdate(
    B20M *const b, const B20M_CONTEXT *const c, const uint64_t tick)
{
    B20M_BIRDS *const birds = &b->birds;
    for (int32_t i = 0; i < 30; i++) {
        if (!birds->alive[i]) {
            continue;
        }
        const int16_t x = M_Pixel(birds->left[i]), y = M_Pixel(birds->top[i]);
        if (birds->hatch_time[i] > 0) {
            M_Grc(
                c, x, y, 1,
                1
                    + (birds->hatch_frames[i] - birds->hatch_time[i])
                        / (birds->hatch_frames[i] / 5),
                15);
            birds->hatch_time[i]--;
        } else {
            M_Erase(c, x, y);
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
            M_Erase(c, x, y);
            birds->alive[i] = false;
            continue;
        }
        M_Grc(c, x, y, 1, 6 + (b->frame / 5) % 2, 15);
        if (!M_Player(c)->invincible && birds->left[i] > M_Player(c)->left - 8
            && birds->left[i] < M_Player(c)->left + 8 && birds->top[i] > 344
            && birds->top[i] < 384) {
            M_Player(c)->is_hit = true;
            b->wait_kind = 3;
            b->wait_tick = tick;
            b->resume_stage = 3;
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
static uint8_t M_CompareByte(const B20M_CONTEXT *const c, const int32_t address)
{
    const uint16_t offset = (uint16_t)address & 32767;
    uint8_t result = 0;
    if (offset < 32000) {
        for (int32_t bit = 0; bit < 8; bit++) {
            if (M_Video(c)->pages[1][offset * 8 + bit] == 12) {
                result |= 0x80u >> bit;
            }
        }
    }
    return result;
}
static void M_MaskedByte(
    const B20M_CONTEXT *const c, const int32_t address, const uint8_t mask,
    const uint8_t color)
{
    const uint16_t offset = (uint16_t)address & 32767;
    if (offset < 32000) {
        for (int32_t bit = 0; bit < 8; bit++) {
            if ((mask & (0x80u >> bit)) != 0) {
                M_Video(c)->pages[0][offset * 8 + bit] = color & 15;
            }
        }
    }
}
static void M_Particle(
    const B20M_CONTEXT *const c, const int16_t vo, const int16_t bit,
    const uint8_t color, const bool cross)
{
    if (bit < 0 || bit > 7) {
        return;
    }
    for (int32_t y = 0; y < 2; y++) {
        const int16_t p = M_W(vo + y * 80);
        const uint8_t mask = M_CompareByte(c, p) & (0xc0u >> bit);
        M_MaskedByte(c, p, mask, color);
        if (cross && bit == 7) {
            M_MaskedByte(
                c, M_W(p + 1), M_CompareByte(c, M_W(p + 1)) & 128, color);
        }
    }
    Video_SelectDrawingPage(M_Video(c), 0);
}
static void M_Vertical(
    B20M *const b, const B20M_CONTEXT *const c, const bool bottom)
{
    B20M_PARTICLES *const p = &b->vertical;
    if (b->frame % 7 == 0) {
        for (int32_t i = 0; i < 30; i++) {
            if (p->color[i] == 0) {
                p->left[i] = M_Random(c) % 640;
                p->top[i] = bottom ? 397 : 0;
                p->velocity[i] =
                    bottom ? -(M_Random(c) % 15) - 8 : (M_Random(c) % 15) + 2;
                p->color[i] = 4;
                break;
            }
        }
    }
    if (b->frame % 2 != 0) {
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
        M_Particle(c, M_DoubleOffset(p->left[i], p->top[i]), bit, 12, false);
        p->top[i] = M_Add(p->top[i], p->velocity[i]);
        const int16_t vo = M_DoubleOffset(p->left[i], p->top[i]);
        if (vo >= 31920 || vo < 0) {
            p->color[i] = 0;
            continue;
        }
        M_Particle(c, vo, bit, (uint8_t)p->color[i], false);
    }
}
static void M_Wavy(B20M *const b, const B20M_CONTEXT *const c)
{
    B20M_WAVES *const p = &b->waves;
    if (b->frame % 7 == 0) {
        for (int32_t i = 0; i < 30; i++) {
            if (p->color[i] == 0) {
                p->left[i] = M_Random(c) % 640;
                p->top[i] = 400;
                p->velocity[i] = -1;
                p->age[i] = 0;
                p->color[i] = 4;
                break;
            }
        }
    }
    if (b->frame % 2 != 0) {
        return;
    }
    for (int32_t i = 0; i < 30; i++) {
        if (p->color[i] == 0) {
            continue;
        }
        int16_t x = Math16_PolarY(p->left[i], 16, p->age[i]);
        M_Particle(
            c, M_W(p->top[i] * 80 + x / 8 - (x % 8 < 0)), (uint16_t)x & 7, 12,
            false);
        p->top[i] = M_W(p->top[i] + p->velocity[i]);
        p->age[i] = M_W(p->age[i] + 1);
        x = Math16_PolarY(p->left[i], 16, p->age[i]);
        if (p->age[i] >= 100) {
            p->color[i] = 0;
            continue;
        }
        M_Particle(
            c, M_W(p->top[i] * 80 + x / 8 - (x % 8 < 0)), (uint16_t)x & 7,
            (uint8_t)p->color[i], false);
    }
}
static void M_Horizontal(B20M *const b, const B20M_CONTEXT *const c)
{
    B20M_PARTICLES *const p = &b->horizontal;
    if (b->frame % 7 == 0) {
        for (int32_t i = 0; i < 30; i++) {
            if (p->color[i] == 0) {
                p->left[i] = M_Random(c) % 640;
                p->top[i] = M_Random(c) % 400;
                p->velocity[i] = M_Random(c) % 2 == 0 ? -6 : 6;
                p->color[i] = 4;
                break;
            }
        }
    }
    if (b->frame % 2 != 0) {
        return;
    }
    for (int32_t i = 0; i < 30; i++) {
        if (p->color[i] == 0) {
            continue;
        }
        M_Particle(
            c, M_DoubleOffset(p->left[i], p->top[i]), M_Pixel(p->left[i]) % 8,
            12, true);
        p->left[i] = M_Add(p->left[i], p->velocity[i]);
        p->velocity[i] = M_Add(p->velocity[i], p->velocity[i] < 0 ? 0.1 : -0.1);
        if (p->left[i] >= 640 || p->left[i] < 0
            || M_Pixel(p->velocity[i]) == 0) {
            p->color[i] = 0;
            continue;
        }
        M_Particle(
            c, M_DoubleOffset(p->left[i], p->top[i]), M_Pixel(p->left[i]) % 8,
            (uint8_t)p->color[i], true);
    }
}
static void M_Hud(const B20M_CONTEXT *c, FRAME_VSYNC *vsync);
static void M_GustFinish(
    B20M *const b, const B20M_CONTEXT *const c, const int16_t image,
    FRAME_VSYNC *const vsync)
{
    Video_SelectDrawingPage(M_Video(c), 1);
    M_Pi(c, &c->assets->backgrounds[image], true);
    M_CopyOther(c);
    b->stage_palette = c->hit->palette->saved;
    Video_SelectDrawingPage(M_Video(c), 0);
    M_Hud(c, vsync);
    HudHp_Rerender(c->hit->health, M_Video(c), b->hp);
    b->frame = 0;
}
static bool M_Gust(
    B20M *const b, const B20M_CONTEXT *const c, FRAME_VSYNC *const vsync)
{
    if (b->frame == 20) {
        b->gust_id = 0;
    }
    if (b->frame < 50) {
        return true;
    }
    if (b->frame == 50) {
        b->gust_x = 0;
        b->gust_y = 0;
        b->gust_offset = 0;
        b->gust_color = 2;
        M_Effect(c, 10);
    }
    for (int32_t row = 0; row < 100; row++) {
        const int16_t right = M_Random(c) % 8;
        uint16_t masks[10] = {};
        const int16_t first = M_W(b->gust_offset + right);
        for (int32_t stripe = 0; stripe < 10; stripe++) {
            const int16_t offset = M_W(first - stripe * 2);
            if (first / 80 != offset / 80) {
                break;
            }
            if (offset >= 0 && offset / 80 == b->gust_offset / 80) {
                masks[stripe] =
                    (uint16_t)(M_CompareByte(c, offset)
                               | (uint16_t)M_CompareByte(c, M_W(offset + 1))
                                   << 8);
            }
        }
        for (int32_t stripe = 0; stripe < 5; stripe++) {
            const int16_t offset = M_W(first - stripe * 2);
            if (first / 80 != offset / 80) {
                break;
            }
            if (offset >= 0 && offset / 80 == b->gust_offset / 80) {
                M_MaskedByte(
                    c, offset, (uint8_t)masks[stripe],
                    (uint8_t)(b->gust_color
                              + (stripe > 2 ? 4 - stripe : stripe)));
            }
        }
        for (int32_t stripe = 0; stripe < b->gust_id + 2; stripe++) {
            const int16_t offset = M_W(first - 10 - stripe * 2);
            if (offset / 80 != first / 80) {
                break;
            }
            if (offset >= 0 && offset / 80 == b->gust_offset / 80) {
                M_MaskedByte(c, offset, (uint8_t)masks[5 + stripe], 12);
            }
        }
        b->gust_y = M_W(b->gust_y + 4);
        b->gust_offset =
            M_W(b->gust_y * 80 + b->gust_x / 8 - (b->gust_x % 8 < 0));
        if (b->gust_offset >= 32000) {
            b->gust_y = 0;
            b->gust_x = M_W(b->gust_x + 32);
            b->gust_offset = b->gust_x / 8;
        }
    }
    Video_SelectDrawingPage(M_Video(c), 0);
    if (b->gust_x > 608) {
        b->gust_id = M_W(b->gust_id + 1);
        b->frame = 49;
        if (b->gust_id >= 3) {
            b->resume_stage = 4;
            return M_Scroll(b, vsync, 0);
        }
    }
    return true;
}
static void M_VortexDraw(
    B20M *const b, const B20M_CONTEXT *const c, const int32_t i,
    const bool spread)
{
    M_Erase(c, b->vortex_previous_left[i], b->vortex_previous_top[i]);
    M_Grc(c, b->vortex_left[i], b->vortex_top[i], 0, b->frame % 3, 7);
    if (spread && b->frame % 4 == 0) {
        const uint8_t angle = M_Angle(
            388 - b->vortex_top[i], b->vortex_left[1 - i] - b->vortex_left[i]);
        M_Single(
            c, M_W(b->vortex_left[i] + 12), M_W(b->vortex_top[i] + 12), 64,
            112);
        M_Single(
            c, M_W(b->vortex_left[i] + 12), M_W(b->vortex_top[i] + 12), angle,
            112);
        M_Single(
            c, M_W(b->vortex_left[i] + 12), M_W(b->vortex_top[i] + 12),
            (uint8_t)(128 - angle), 112);
    }
}
static void M_Vortices(B20M *const b, const B20M_CONTEXT *const c)
{
    if (!b->vortex_wand_done) {
        b->vortex_wand_done = M_WandRaise(b, c, false);
    }
    if (b->frame < 50) {
        return;
    }
    if (b->frame == 50) {
        b->pattern_state = M_Rank(c, 140, 145, 150, 155);
    }
    if (b->frame < 100) {
        for (int32_t i = 0; i < 2; i++) {
            b->vortex_left[i] = (int16_t)(i * 608);
            b->vortex_top[i] = 200;
            M_VortexDraw(b, c, i, false);
            if (b->frame % 4 == 0) {
                M_Group(
                    c, M_W(b->vortex_left[i] + 12), M_W(b->vortex_top[i] + 12),
                    PELLET_GROUP_SINGLE, 112);
            }
            b->vortex_previous_left[i] = b->vortex_left[i];
            b->vortex_previous_top[i] = b->vortex_top[i];
        }
    } else if (b->frame < b->pattern_state) {
        for (int32_t i = 0; i < 2; i++) {
            b->vortex_left[i] = M_W(b->vortex_left[i] + (i == 0 ? 5 : -5));
            b->vortex_top[i] = M_W(b->vortex_top[i] - 2);
            M_VortexDraw(b, c, i, false);
            if (b->frame % 4 == 0) {
                M_Group(
                    c, M_W(b->vortex_left[i] + 12), M_W(b->vortex_top[i] + 12),
                    PELLET_GROUP_SINGLE, 112);
                M_Single(
                    c, M_W(b->vortex_left[i] + 12), M_W(b->vortex_top[i] + 12),
                    M_Angle(412 - b->vortex_top[i], i * 160 - 68), 112);
            }
            b->vortex_previous_left[i] = b->vortex_left[i];
            b->vortex_previous_top[i] = b->vortex_top[i];
        }
    } else if (b->frame < 200) {
        for (int32_t i = 0; i < 2; i++) {
            M_VortexDraw(b, c, i, true);
        }
    } else if (b->frame < 240) {
        if (b->frame == 200) {
            b->direction_first = M_Random(c) % 2 != 0;
            if (b->direction_first) {
                M_WandLower(b, c);
            }
        }
        for (int32_t i = 0; i < 2; i++) {
            b->vortex_left[i] =
                M_W(b->vortex_left[i] + 2 - b->direction_first * 4);
            M_VortexDraw(b, c, i, true);
            b->vortex_previous_left[i] = b->vortex_left[i];
        }
    } else if (b->frame < 320) {
        if (b->frame == 240) {
            b->direction_second = M_Random(c) % 2 != 0;
            if (!b->direction_second
                && b->direction_second == b->direction_first) {
                M_WandLower(b, c);
            } else if (
                b->direction_second
                && b->direction_second == b->direction_first) {
                b->vortex_wand_done = false;
            }
        }
        for (int32_t i = 0; i < 2; i++) {
            b->vortex_left[i] =
                M_W(b->vortex_left[i]
                    - (b->direction_second != b->direction_first
                           ? 2 - b->direction_second * 4
                           : 4 - b->direction_second * 8));
            M_VortexDraw(b, c, i, true);
            b->vortex_previous_left[i] = b->vortex_left[i];
        }
    } else {
        for (int32_t i = 0; i < 2; i++) {
            M_Erase(c, b->vortex_left[i], b->vortex_top[i]);
        }
        M_WandLower(b, c);
        b->frame = 0;
        b->vortex_wand_done = false;
    }
}
static void M_PurpleLasers(B20M *const b, const B20M_CONTEXT *const c)
{
    if (b->frame < 50) {
        return;
    }
    if (b->frame == 50) {
        b->pattern_state = M_Rank(c, 68, 72, 76, 80);
        for (int32_t i = 0; i < 10; i++) {
            b->laser_x[i] = 120 + M_Random(c) % 400;
            b->laser_y[i] = 100 + M_Random(c) % 100;
        }
    }
    if (b->frame < 150) {
        const int32_t i = (b->frame - 50) / 10;
        if (b->frame % 10 != 9) {
            if (b->frame % 10 != 0) {
                const int16_t r = (int16_t)(64 - ((b->frame - 1) % 10) * 8);
                M_EraseEllipse(c, b->laser_x[i], b->laser_y[i], r, r, 1);
            }
            if (b->frame % 10 != 8) {
                const int16_t r = (int16_t)(64 - (b->frame % 10) * 8);
                M_Ellipse(c, b->laser_x[i], b->laser_y[i], r, r, 4, 1);
            }
        } else {
            M_Effect(c, 6);
            M_Laser(
                c, i, b->laser_x[i], b->laser_y[i], M_Random(c) % 640, 400,
                b->pattern_state, 4, 25, 5);
        }
    } else if (b->frame > 200) {
        b->frame = 0;
    }
}
static bool M_In(
    const int16_t x, const int16_t y, const int16_t l, const int16_t t,
    const int16_t r, const int16_t bottom)
{
    return x >= l && x <= r && y >= t && y <= bottom;
}
static void M_Eggs(B20M *const b, const B20M_CONTEXT *const c)
{
    if (b->frame < 40) {
        return;
    }
    if (!b->bird_wand_done) {
        b->bird_wand_done = M_WandRaise(b, c, false);
    }
    if (b->frame < 50) {
        return;
    }
    if (b->frame == 50) {
        b->eggs_alive = 0;
        b->spawner_left = 5440;
        b->spawner_top = 1024;
        b->spawner_velocity_x = (int16_t)(64 - (M_Random(c) % 2) * 128);
        b->spawner_velocity_y = (int16_t)(32 - (M_Random(c) % 2) * 64);
        b->pattern_state = M_Rank(c, 20, 15, 10, 8);
        M_Effect(c, 8);
    } else if (b->frame < 200) {
        b->spawner_velocity_x =
            M_W(b->spawner_velocity_x + (b->spawner_left < 5120 ? 1 : -1));
        b->spawner_velocity_y =
            M_W(b->spawner_velocity_y + (b->spawner_top < 2624 ? 1 : -1));
        M_Erase(
            c, Math16_ToPixel(b->spawner_left), Math16_ToPixel(b->spawner_top));
        for (int32_t i = b->eggs_alive - 1; i >= 0; i--) {
            M_Grc(c, b->egg_left[i], b->egg_top[i], 1, 0, 15);
        }
        b->spawner_left = M_W(b->spawner_left + b->spawner_velocity_x);
        b->spawner_top = M_W(b->spawner_top + b->spawner_velocity_y);
        if (b->frame % b->pattern_state == 0
            && M_In(b->spawner_left, b->spawner_top, 0, 0, 9728, 5888)) {
            if (b->eggs_alive < 30) {
                b->egg_left[b->eggs_alive] = Math16_ToPixel(b->spawner_left);
                b->egg_top[b->eggs_alive] = Math16_ToPixel(b->spawner_top);
                b->eggs_alive++;
            }
            M_Grc(
                c, Math16_ToPixel(b->spawner_left),
                Math16_ToPixel(b->spawner_top), 1, 0, 15);
        }
        M_Grc(
            c, Math16_ToPixel(b->spawner_left), Math16_ToPixel(b->spawner_top),
            2, b->frame % 2, (uint8_t)(b->frame % 4 + 2));
    } else if (b->frame < 400) {
        if (b->frame == 200) {
            if (b->spawner_velocity_y > -4 && b->spawner_velocity_y < 4) {
                b->spawner_velocity_y = b->spawner_velocity_y < 0 ? -4 : 4;
            }
            if (b->spawner_velocity_x > -4 && b->spawner_velocity_x < 4) {
                b->spawner_velocity_x = b->spawner_velocity_x < 0 ? -4 : 4;
            }
            for (int32_t i = b->eggs_alive - 1; i >= 0; i--) {
                const MATH16_VECTOR v =
                    Math16_Vector(3, (M_Random(c) & 127) + 128);
                M_BirdSpawn(b, b->egg_left[i], b->egg_top[i], v.x, v.y);
            }
            M_WandLower(b, c);
            b->bird_pellet_group = M_Random(c) % 2;
        }
        if (M_In(b->spawner_left, b->spawner_top, -512, -512, 10240, 6400)) {
            M_Erase(
                c, Math16_ToPixel(b->spawner_left),
                Math16_ToPixel(b->spawner_top));
            b->spawner_left = M_W(b->spawner_left + b->spawner_velocity_x);
            b->spawner_top = M_W(b->spawner_top + b->spawner_velocity_y);
            if (M_In(b->spawner_left, b->spawner_top, 0, 0, 608, 368)) {
                M_Grc(
                    c, b->spawner_left, b->spawner_top, 2, b->frame % 2,
                    (uint8_t)(b->frame % 4 + 2));
            }
        }
        if (b->frame % 16 == 0 || b->frame % 16 == 2) {
            M_BirdsFire(b, c, b->bird_pellet_group);
        }
    } else if (b->frame == 400) {
        b->frame = 0;
        b->bird_wand_done = false;
    }
}
static void M_FlakeEllipse(
    const B20M *const b, const B20M_CONTEXT *const c, const int16_t rx,
    const int16_t ry, const uint8_t color, const uint8_t step, const bool erase)
{
    const int16_t x = Math16_ToPixel(b->flake_left);
    if (erase) {
        M_EraseEllipse(c, x, 384, rx, ry, step);
    } else {
        M_Ellipse(c, x, 384, rx, ry, color, step);
    }
}
static void M_Flake(B20M *const b, const B20M_CONTEXT *const c)
{
    if (b->frame < 10) {
        b->flake_state = -1;
        b->pattern_state = 160;
    }
    if (b->frame % b->pattern_state == 0) {
        b->flake_state = 0;
    }
    if (b->flake_state == 0) {
        b->flake_state = M_WandRaise(b, c, false);
    }
    if (b->flake_state == 1) {
        M_Effect(c, 6);
        b->flake_left = 5440;
        b->flake_top = 1024;
        const MATH16_VECTOR v = Math16_Aim(
            (MATH16_VECTOR) { 340, 64 },
            (MATH16_VECTOR) { M_W(M_Player(c)->left + 12), 380 }, 112, 0);
        b->flake_velocity_x = v.x;
        b->flake_velocity_y = v.y;
        b->flake_state = 2;
    }
    if (b->flake_state == 2) {
        Sprite_Restore16(
            M_Video(c), Math16_ToPixel(b->flake_left),
            Math16_ToPixel(b->flake_top), 16, 8);
        b->flake_top = M_W(b->flake_top + b->flake_velocity_y);
        if (b->flake_top >= 6336) {
            b->flake_state = 3;
            M_WandLower(b, c);
            return;
        }
        b->flake_left = M_W(b->flake_left + b->flake_velocity_x);
        M_Mask(
            c, Math16_ToPixel(b->flake_left), Math16_ToPixel(b->flake_top),
            c->assets->flake, 7);
    }
    if (b->flake_state == 3) {
        M_FlakeEllipse(b, c, 48, 16, 6, 8, false);
        M_FlakeEllipse(b, c, 16, 48, 6, 8, false);
        M_FlakeEllipse(b, c, 24, 24, 7, 6, false);
        b->radius_outer_1 = 48;
        b->radius_outer_2 = 16;
        b->radius_inner = 24;
        b->star_left = b->flake_left;
        b->star_top = 400 - M_Random(c) % 48;
        M_Effect(c, 10);
    }
    if (b->flake_state >= 3) {
        b->flake_state = M_W(b->flake_state + 1);
    }
    if (b->flake_state >= 5 && b->flake_state % 8 == 0) {
        Sprite_Restore16(M_Video(c), b->star_left, b->star_top, 16, 8);
        M_FlakeEllipse(b, c, b->radius_outer_1, b->radius_outer_2, 0, 8, true);
        M_FlakeEllipse(b, c, b->radius_outer_2, b->radius_outer_1, 0, 8, true);
        M_FlakeEllipse(b, c, b->radius_inner, b->radius_inner, 0, 6, true);
        b->radius_outer_1 = M_W(b->radius_outer_1 - 6);
        b->radius_outer_2 = M_W(b->radius_outer_2 + 6);
        b->radius_inner = M_W(b->radius_inner + 4);
        b->star_left =
            M_W(M_Random(c) % 64 + Math16_ToPixel(b->flake_left) - 28);
        b->star_top = 400 - M_Random(c) % 48;
        if (b->radius_outer_1 <= 8) {
            b->flake_state = -1;
            return;
        }
        M_FlakeEllipse(b, c, b->radius_outer_1, b->radius_outer_2, 5, 8, false);
        M_FlakeEllipse(b, c, b->radius_outer_2, b->radius_outer_1, 5, 8, false);
        M_FlakeEllipse(b, c, b->radius_inner, b->radius_inner, 7, 6, false);
        M_Mask(c, b->star_left, b->star_top, c->assets->star, 7);
    }
    const int16_t x = Math16_ToPixel(b->flake_left);
    if (!M_Player(c)->invincible && x + 32 > M_Player(c)->left
        && x - 64 < M_Player(c)->left && b->flake_state >= 3) {
        M_Player(c)->is_hit = true;
    }
}
static void M_Rings(B20M *const b, const B20M_CONTEXT *const c)
{
    if (b->frame < 5) {
        b->rings_angle = 0;
        b->rings_interval = M_Rank(c, 18, 12, 10, 8);
    }
    if (b->frame % b->rings_interval == 0) {
        c->pellets->spawn_with_cloud = true;
        M_Single(c, 568, 160, (uint8_t)(0 - b->rings_angle), 56);
        M_Single(c, 568, 160, (uint8_t)(128 - b->rings_angle), 56);
        M_Single(c, 64, 160, b->rings_angle, 56);
        M_Single(c, 64, 160, (uint8_t)(128 + b->rings_angle), 56);
        b->rings_angle = (uint8_t)(b->rings_angle + b->rings_interval);
    }
    if (b->frame > 479) {
        b->frame = 0;
    }
}
static void M_Sling(B20M *const b, const B20M_CONTEXT *const c)
{
    if (b->frame == 10) {
        b->pattern_state = M_Rank(c, 4, 3, 2, 2);
    }
    if (b->frame % 200 >= 150 && b->frame % b->pattern_state == 0) {
        const uint8_t angle = (uint8_t)M_Random(c);
        Pellets_AddSingle(
            c->pellets, c->pellet_shared, c->hit->display->rank,
            Math16_PolarY(320, 40, angle), Math16_PolarX(180, 40, angle), 0, 0,
            PELLET_SLING_AIMED, 72, 0, 0);
    }
    if (b->frame >= 299) {
        b->frame = 0;
    }
}
static void M_FourLasers(B20M *const b, const B20M_CONTEXT *const c)
{
    if (b->frame < 5) {
        M_Ray(b, c, 0, 0, 0, 0, 99);
    }
    if (b->frame < 50) {
        return;
    }
    int16_t x = 0, y = 0, target = 0;
    int32_t laser = -1;
    if (b->frame == 50) {
        x = 460;
        y = 120;
        target = M_Player(c)->left;
        laser = 0;
        b->laser_target = target;
        b->pattern_state = M_Rank(c, 60, 64, 68, 72);
    } else if (b->frame == 100) {
        x = 180;
        y = 120;
        target = M_W(639 - b->laser_target);
        laser = 1;
    } else if (b->frame == 150) {
        x = 608;
        y = 164;
        target = M_Player(c)->left;
        laser = 2;
    } else if (b->frame == 200) {
        x = 24;
        y = 164;
        target = M_Player(c)->left;
        laser = 3;
    }
    if (laser >= 0) {
        b->laser_origin_x = x;
        b->laser_origin_y = y;
        const MATH16_VECTOR v = Math16_Aim(
            (MATH16_VECTOR) { x, y }, (MATH16_VECTOR) { target, 400 }, 16, 0);
        b->laser_velocity_x = v.x;
        b->laser_velocity_y = v.y;
        M_Laser(c, laser, x, y, target, 400, b->pattern_state, 7, 50, 8);
        M_Effect(c, 6);
    } else if (b->frame < 250) {
        M_Ray(b, c, 320, 120, b->laser_origin_x, b->laser_origin_y, 7);
        b->laser_origin_x = M_W(b->laser_origin_x + b->laser_velocity_x);
        b->laser_origin_y = M_W(b->laser_origin_y + b->laser_velocity_y);
    } else if (b->frame > 300) {
        b->frame = 0;
    }
}
static bool M_Shake(
    B20M *const b, const B20M_CONTEXT *const c, FRAME_VSYNC *const vsync)
{
    if (b->frame < 50) {
        if (b->frame % 8 == 0 || b->frame % 8 == 4) {
            b->resume_stage = 1;
            return M_Scroll(b, vsync, b->frame % 8 == 0 ? 396 : 404);
        }
        if (b->frame % 4 == 0) {
            M_Effect(c, 7);
        }
    } else if (b->frame == 50) {
        b->resume_stage = 2;
        return M_Scroll(b, vsync, 0);
    }
    return true;
}
static void M_TopRain(B20M *const b, const B20M_CONTEXT *const c)
{
    if (b->frame % 8 == 0) {
        M_Group(
            c, M_Random(c) % 640, 64,
            c->hit->display->rank == 3 ? PELLET_GROUP_RANDOM_WIDE
                                       : PELLET_GROUP_SINGLE,
            44);
    }
}
static void M_Radial(B20M *const b, const B20M_CONTEXT *const c)
{
    if (b->frame < 5) {
        M_Ray(b, c, 0, 0, 0, 0, 99);
    }
    if (b->frame < 150) {
        return;
    }
    if (b->frame == 150) {
        b->radial_angle = 0;
        b->radial_velocity = 1;
        b->pattern_state = M_Rank(c, 60, 65, 70, 75);
        M_Effect(c, 6);
    } else if (b->frame < 200) {
        M_Ray(
            b, c, 320, 160, Math16_PolarX(320, 140, b->radial_angle),
            Math16_PolarY(160, 140, b->radial_angle), 7);
        b->radial_angle = (uint8_t)(b->radial_angle + b->radial_velocity);
        b->radial_velocity++;
        if (b->frame % 10 == 0) {
            M_Effect(c, 6);
        }
    } else if (b->frame == 200) {
        M_Ray(b, c, 320, 160, 999, 999, 7);
        b->radial_angle = 0;
    } else if (b->frame <= 400 && b->frame % 5 == 0) {
        M_Effect(c, 7);
        if (b->frame % 15 == 0) {
            M_Laser(
                c, (b->frame - 215) / 15, 320, 160,
                Math16_PolarX(320, 600, b->radial_angle),
                Math16_PolarY(160, 600, b->radial_angle), b->pattern_state, 7,
                20, 4);
        } else {
            M_Single(c, 320, 160, b->radial_angle, 100);
        }
        M_Single(c, 320, 160, (uint8_t)(b->radial_angle - 4), 96);
        M_Single(c, 320, 160, (uint8_t)(b->radial_angle + 4), 96);
        if (c->hit->display->rank >= 2) {
            M_Single(c, 320, 160, (uint8_t)(b->radial_angle - 4), 64);
            M_Single(c, 320, 160, (uint8_t)(b->radial_angle + 4), 64);
            if (c->hit->display->rank == 3) {
                M_Single(c, 320, 160, b->radial_angle, 64);
                M_Single(c, 320, 160, b->radial_angle, 48);
            }
        }
        b->radial_angle = (uint8_t)(b->radial_angle + 12);
    } else if (b->frame == 450) {
        b->frame = 0;
    }
}
static int16_t M_DebrisNext(const int16_t image)
{
    return image < 5 ? M_W(image + 1) : 3;
}
static int16_t M_RayX(const B20M_RAYS *const r, const bool left)
{
    return left ? r->x : M_W(639 - r->x);
}
static int16_t M_DebrisX(
    const B20M_RAYS *const r, const bool left, const int16_t offset,
    const int16_t movement)
{
    return left ? M_W(r->x - 32 + offset - movement)
                : M_W(640 - offset - r->x + movement);
}
static void M_RaysDraw(
    B20M *const b, const B20M_CONTEXT *const c, const B20M_RAYS *const r,
    const bool left, const int16_t ox, const int16_t oy, const int16_t offset,
    const int16_t speed, const bool broken)
{
    if (broken) {
        M_Erase(c, M_DebrisX(r, left, offset, M_W(speed * 2)), M_W(r->y - 32));
        M_Grc(c, M_DebrisX(r, left, offset, 0), M_W(r->y - 16), 0, r->image, 7);
        M_Ray(b, c, ox, oy, M_RayX(r, left), r->y, 7);
    } else {
        Sprite_Restore16(
            M_Video(c), M_DebrisX(r, left, offset, 0), M_W(r->y - 8), 32, 32);
        M_Ray(b, c, ox, oy, M_RayX(r, left), r->y, 7);
        M_Grc(c, M_DebrisX(r, left, offset, 0), M_W(r->y - 16), 0, r->image, 7);
    }
}
static void M_SymmetricBirds(B20M *const b, const B20M_CONTEXT *const c)
{
    B20M_RAYS *const r = &b->bird_rays;
    if (b->frame < 50) {
        return;
    }
    if (b->frame == 50) {
        b->pattern_state = M_Rank(c, 6, 7, 8, 9);
        r->x = 2;
        r->y = 399;
        r->velocity_x = 2;
        r->velocity_y = -2;
        r->image = 3;
        return;
    }
    if (b->frame < 100) {
        M_RaysDraw(b, c, r, b->frame % 2 == 0, 320, 120, 8, 2, true);
        if (b->frame % 2 != 0) {
            r->image = M_DebrisNext(r->image);
        }
        r->x = M_W(r->x + 2);
        if (b->frame % 10 == 0) {
            M_Effect(c, 6);
            const int16_t target = M_W((b->frame - 60) * 10 + r->x);
            const MATH16_VECTOR v = Math16_Aim(
                (MATH16_VECTOR) { r->x, M_W(r->y - 32) },
                (MATH16_VECTOR) { target, 0 }, 4, 0);
            r->velocity_x = v.x;
            r->velocity_y = v.y;
            M_BirdSpawn(b, r->x, M_W(r->y - 32), v.x, v.y);
            M_BirdSpawn(b, M_RayX(r, false), M_W(r->y - 32), -v.x, v.y);
        }
        if (b->frame % 6 == 0) {
            M_BirdsFire(b, c, 2);
        }
        return;
    }
    if (b->frame == 100) {
        Video_LineErase(M_Video(c), 320, 120, M_W(M_RayX(r, false) + 2), r->y);
        return;
    }
    if (b->frame % 10 == 0) {
        M_BirdsFire(b, c, 2);
    }
    if (b->frame >= 300) {
        b->frame = 0;
    }
}
static void M_Semicircles(B20M *const b, const B20M_CONTEXT *const c)
{
    int32_t count = 0, angle = 0, step = 0, speed = 0;
    if (b->frame == 50) {
        count = 20;
        step = 7;
        speed = 32;
    } else if (b->frame == 60) {
        count = 10;
        angle = 4;
        step = 12;
        speed = 48;
    } else if (b->frame == 70) {
        count = 8;
        angle = 4;
        step = 14;
        speed = 64;
    } else if (b->frame == 80) {
        count = 10;
        angle = 14;
        step = 10;
        speed = 48;
    } else if (b->frame > 150) {
        b->frame = 0;
    }
    for (int32_t i = 0; i < count; i++) {
        M_Single(c, 320, 160, (uint8_t)angle, (int16_t)speed);
        angle += step;
    }
}
static void M_Stacks(B20M *const b, const B20M_CONTEXT *const c)
{
    B20M_RAYS *const r = &b->stack_rays;
    if (b->frame < 50) {
        return;
    }
    if (b->frame == 50) {
        b->pattern_state = M_Rank(c, 16, 13, 10, 8);
        r->x = 2;
        r->y = 399;
    } else if (b->frame < 130) {
        M_RaysDraw(b, c, r, b->frame % 2 == 0, 320, 120, 8, 8, true);
        if (b->frame % 2 != 0) {
            r->image = M_DebrisNext(r->image);
        }
        r->x = M_W(r->x + 8);
    } else if (b->frame == 130) {
        Video_LineErase(M_Video(c), 320, 120, M_W(M_RayX(r, false) + 8), r->y);
        r->x = 0;
    } else if (b->frame > 150 && b->frame < 350) {
        if (b->frame % 10 == 0) {
            for (int16_t i = 0; i < 64; i += 16) {
                if (M_Player(c)->left + 16 - r->x > 32
                    || M_Player(c)->left + 16 - r->x < -32) {
                    M_Single(c, r->x, M_W(r->y - 8), 192, M_W(i + 48));
                }
                const int16_t rx = M_RayX(r, false);
                if (M_Player(c)->left + 16 - rx > 32
                    || M_Player(c)->left + 16 - rx < -32) {
                    M_Single(c, rx, M_W(r->y - 8), 192, M_W(i + 48));
                }
            }
            r->x = M_W(r->x + 32);
            M_Effect(c, 7);
        }
    } else if (b->frame == 350) {
        r->x = 0;
        r->y = 64;
    } else if (b->frame < 550) {
        if (b->frame % b->pattern_state == 0) {
            for (int32_t i = 0; i < 2; i++) {
                uint8_t angle = M_Angle(400 - r->y, (M_Random(c) % 640) - r->x);
                M_Single(c, r->x, r->y, angle, 48);
                angle = M_Angle(400 - r->y, (M_Random(c) % 640) - r->x);
                M_Single(c, M_RayX(r, false), r->y, angle, 48);
            }
            r->x = M_W(r->x + 32);
        }
    } else if (b->frame > 600) {
        b->frame = 0;
    }
}
static void M_Dotted(
    B20M *const b, const B20M_CONTEXT *const c, const int16_t frame,
    const int16_t interval, const int16_t step, const int16_t initial,
    const int16_t frames)
{
    const int16_t radius = M_W((frame / interval) * step + initial);
    if (frame == 1) {
        b->dotted_active = true;
    } else {
        if (!b->dotted_active || frame % interval != 0) {
            return;
        }
        M_EraseEllipse(c, 320, 185, b->dotted_radius, b->dotted_radius, 1);
        if (frame >= frames) {
            b->dotted_active = false;
            return;
        }
        M_Ellipse(c, 320, 185, radius, radius, 7, 1);
    }
    b->dotted_radius = radius;
}
static void M_SprayInit(
    B20M_SPRAY *const s, B20M *const b, const B20M_CONTEXT *const c)
{
    s->x = 0;
    s->y = 400;
    s->speed = 112;
    s->subpattern = 0;
    b->pattern_state = M_Rank(c, 32, 24, 18, 16);
}
static void M_SprayFire(
    B20M_SPRAY *const s, B20M *const b, const B20M_CONTEXT *const c,
    const bool right)
{
    s->angle = M_Angle(s->y - 185, s->x - 320);
    M_Single(c, 320, 185, s->angle, s->speed);
    s->speed = M_W(s->speed - 4);
    s->x = M_W(s->x + (right ? b->pattern_state : -b->pattern_state));
}
static void M_SprayOnce(B20M *const b, const B20M_CONTEXT *const c)
{
    B20M_SPRAY *const s = &b->sprays[0];
    if (b->frame < 50) {
        return;
    }
    if (b->frame == 50) {
        M_Dotted(b, c, 1, 4, 16, 32, 160);
    }
    M_Dotted(b, c, M_W(b->frame - 49), 4, 16, 32, 90);
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        M_SprayInit(s, b, c);
    }
    if (s->subpattern == 0) {
        M_SprayFire(s, b, c, true);
        if (s->x >= 640) {
            s->subpattern = 1;
            s->speed = 112;
            s->x = 640;
        }
    } else {
        M_SprayFire(s, b, c, false);
        if (s->x < 0) {
            b->frame = 0;
        }
    }
}
static void M_SealRain(B20M *const b, const B20M_CONTEXT *const c)
{
    B20M_RAYS *const r = &b->seal_rays;
    if (b->frame < 50) {
        return;
    }
    if (b->frame == 50) {
        r->x = 0;
        r->y = 399;
        r->image = 3;
        r->previous_image = 3;
        b->pattern_state = M_Rank(c, 30, 35, 40, 45);
    }
    if (r->y > 185) {
        M_RaysDraw(b, c, r, b->frame % 2 == 0, 320, 185, 32, 0, false);
        if (b->frame % 2 != 0) {
            r->previous_image = r->image;
            r->image = M_DebrisNext(r->image);
            r->y = M_W(r->y - 8);
        }
        if (b->frame % 10 == 0) {
            M_Effect(c, 6);
        }
        return;
    }
    M_Effect(c, 3);
    Sprite_Restore16(
        M_Video(c), M_DebrisX(r, false, 32, 0), M_W(r->y - 16), 32, 40);
    Sprite_Restore16(
        M_Video(c), M_DebrisX(r, true, 32, 0), M_W(r->y - 16), 32, 50);
    Video_LineErase(M_Video(c), 320, 185, M_RayX(r, true), M_W(r->y + 4));
    Video_LineErase(M_Video(c), 320, 185, M_RayX(r, false), M_W(r->y + 4));
    for (int32_t i = 0; i < b->pattern_state; i++) {
        M_Rain(
            c, (int16_t)((640 / b->pattern_state) * i), 185,
            (uint8_t)M_Random(c), 16);
    }
    b->frame = 0;
}
static void M_SprayTwice(B20M *const b, const B20M_CONTEXT *const c)
{
    B20M_SPRAY *const s = &b->sprays[1];
    if (b->frame < 80) {
        return;
    }
    if (b->frame == 80) {
        M_SprayInit(s, b, c);
    }
    if (s->subpattern % 2 == 0) {
        M_SprayFire(s, b, c, true);
        if (s->x >= 640) {
            s->subpattern = M_W(s->subpattern + 1);
            s->speed = 112;
            s->x = 640;
        }
    } else {
        M_SprayFire(s, b, c, false);
        if (s->x < 0) {
            s->subpattern = M_W(s->subpattern + 1);
            s->speed = 112;
            s->x = 0;
            if (s->subpattern == 4) {
                b->frame = 0;
            }
        }
    }
}
static bool M_LeafVisible(const B20M_LEAVES *const p, const int32_t i)
{
    return p->left[i] >= 0 && p->left[i] < 6320 && p->top[i] >= 0
        && p->top[i] < 3920;
}
static void M_Leaves(
    B20M *const b, const B20M_CONTEXT *const c, const int16_t interval)
{
    B20M_LEAVES *const p = &b->leaves;
    if (interval == 999) {
        for (int32_t i = 0; i < 30; i++) {
            p->flag[i] = 0;
        }
        return;
    }
    if (b->frame % interval == 0) {
        for (int32_t i = 0; i < 30; i++) {
            if (p->flag[i] != 0) {
                continue;
            }
            p->left[i] = M_Random(c) % 6400;
            p->top[i] = 1200 + M_Random(c) % 1000;
            const MATH16_VECTOR v = Math16_Aim(
                (MATH16_VECTOR) { p->left[i] / 10, p->top[i] / 10 },
                (MATH16_VECTOR) { M_Random(c) % 640, 64 }, 40, 0);
            p->velocity_x[i] = v.x;
            p->velocity_y[i] = v.y;
            p->flag[i] = 1;
            break;
        }
    }
    if (b->frame % 2 != 0) {
        return;
    }
    for (int32_t i = 0; i < 30; i++) {
        if (p->flag[i] == 0) {
            continue;
        }
        if (p->flag[i] == 1 || p->flag[i] == 6) {
            if (M_LeafVisible(p, i)) {
                Sprite_Restore16(
                    M_Video(c), p->left[i] / 10, p->top[i] / 10, 16, 8);
            }
            p->left[i] = M_W(p->left[i] + p->velocity_x[i]);
            p->top[i] = M_W(p->top[i] + p->velocity_y[i]);
            if (p->flag[i] == 1) {
                if (p->top[i] < 640) {
                    p->flag[i] = 2;
                }
            } else {
                p->velocity_y[i] = M_W(p->velocity_y[i] - 1);
                if (p->velocity_y[i] < -1) {
                    p->velocity_y[i] = 30 + M_Random(c) % 20;
                    p->velocity_x[i] = p->velocity_x[i] < 0 ? 20 : -20;
                }
                if (p->top[i] / 10 > 400) {
                    p->flag[i] = 0;
                }
            }
        } else if (p->flag[i] <= 5) {
            if (M_LeafVisible(p, i)) {
                M_Erase(c, p->left[i] / 10, p->top[i] / 10);
            }
            if (p->flag[i] == 5) {
                p->flag[i] = 6;
                p->velocity_y[i] = 3 + M_Random(c) % 2;
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
                M_Mask(c, p->left[i] / 10, p->top[i] / 10, c->assets->spark, 7);
            }
        } else if (p->flag[i] <= 5) {
            if (M_LeafVisible(p, i)) {
                M_Grc(c, p->left[i], p->top[i], 3, M_W(p->flag[i] - 2), 7);
            }
            if (b->frame % 4 == 0) {
                p->flag[i] = M_W(p->flag[i] + 1);
            }
        } else if (p->flag[i] == 6) {
            if (M_LeafVisible(p, i)) {
                M_Mask(
                    c, p->left[i] / 10, p->top[i] / 10,
                    p->velocity_x[i] < 0 ? c->assets->leaf_left
                                         : c->assets->leaf_right,
                    7);
            }
            if (p->top[i] > 3680 && p->top[i] < 3920
                && p->left[i] / 10 > M_Player(c)->left
                && p->left[i] / 10 < M_Player(c)->left + 24
                && !M_Player(c)->invincible) {
                M_Player(c)->is_hit = true;
            }
        }
    }
}
static void M_Hud(const B20M_CONTEXT *const c, FRAME_VSYNC *const vsync)
{
    const TIMER_CONTEXT timer = { .hud = c->hit->hud,
                                  .display = c->hit->display,
                                  .tram = c->tram,
                                  .vsync = vsync,
                                  .pellets = c->pellets,
                                  .shared = c->pellet_shared,
                                  .rng = c->rng,
                                  .effect = c->hit->effect,
                                  .user = c->hit->user };
    *c->first_stage = true;
    Timer_Redraw(c->timer, &timer, true, *c->timer_initialized, 0);
    Hud_ScoreAndCombo(c->hit->hud, c->hit->display);
}
void B20M_BeginEntrance(B20M *const b, const B20M_CONTEXT *const c)
{
    ASSERT(!b->entrance_active && b->sequence == 0 && b->wait_kind == 0);
    c->pellets->interlace = true;
    Tram_Fill(c->tram, (GFX_RECT) { 0, 0, 640, 400 }, TRAM_BLACK);
    M_Pi(c, &c->assets->lower, true);
    Video_SelectDrawingPage(M_Video(c), 1);
    M_Pi(c, &c->assets->upper, true);
    Video_SelectDrawingPage(M_Video(c), 0);
    StageObjects_Init(
        c->objects, nullptr, 19, false, nullptr, M_Video(c), nullptr, c->rng);
    M_Audio(c, B20M_AUDIO_LOAD_MUSIC);
    M_Audio(c, B20M_AUDIO_LOAD_EFFECTS);
    M_Audio(c, B20M_AUDIO_PLAY);
    Tram_Clear(c->tram);
    b->scroll_line = 399;
    b->sequence_stage = 1;
    b->entrance_active = true;
}
bool B20M_ResumeEntrance(
    B20M *const b, const B20M_CONTEXT *const c, FRAME_VSYNC *const vsync,
    const uint64_t tick)
{
    if (!b->entrance_active) {
        return true;
    }
    if (!M_Ready(b, c, vsync, tick)) {
        return false;
    }
    for (;;) {
        switch (b->sequence_stage) {
        case 1:
            if (b->scroll_line < 0) {
                b->sequence_stage = 4;
                return M_Scroll(b, vsync, 0);
            }
            b->sequence_stage = 2;
            return M_Scroll(b, vsync, b->scroll_line);
        case 2:
            M_CopyRows(c, b->scroll_line, 1);
            b->scroll_line--;
            b->sequence_stage = 1;
            if (!M_Delay(b, vsync, 1)) {
                return false;
            }
            continue;
        case 4:
            Video_SelectDrawingPage(M_Video(c), 1);
            M_Pi(c, &c->assets->backgrounds[0], true);
            Video_SelectDrawingPage(M_Video(c), 0);
            b->sequence_stage = 5;
            {
                const TRANSITION_CONTEXT t = M_TransitionContext(b, c, false);
                if (!Transition_BeginDiagonal(&b->transition, &t, 40, tick)) {
                    return false;
                }
            }
            continue;
        case 5: {
            const TRANSITION_CONTEXT t = M_TransitionContext(b, c, false);
            if (!Transition_Advance(&b->transition, &t, tick)) {
                return false;
            }
        }
            *c->rng = c->hit->frame_rand;
            b->entrance_active = false;
            b->sequence_stage = 0;
            return true;
        default:
            ASSERT(false);
            return false;
        }
    }
}
static int16_t M_InvertRings(const B20M_CONTEXT *const c, const int16_t base)
{
    int16_t outside = 0;
    for (int32_t i = 32; i < 160; i++) {
        const int16_t r = M_W(base * (i / 32));
        outside += !Video_Invert32(
            M_Video(c), Math16_PolarY(304, r, i * 8),
            Math16_PolarX(180, r, i * 8), 2);
    }
    return outside;
}
static bool M_RingEntrance(
    B20M *const b, const B20M_CONTEXT *const c, FRAME_VSYNC *const vsync,
    const uint64_t tick)
{
    if (!M_Ready(b, c, vsync, tick)) {
        return false;
    }
    for (;;) {
        b->frame = M_W(b->frame + 1);
        bool finish = false;
        if (b->frame == 1) {
            b->entrance_radius = 16;
        } else if (b->frame == 2) {
            M_InvertRings(c, b->entrance_radius);
        } else if (b->frame > 2 && b->frame % 4 == 0) {
            M_InvertRings(c, b->entrance_radius);
            b->entrance_radius = M_W(b->entrance_radius + 16);
            finish = M_InvertRings(c, b->entrance_radius) >= 128;
        }
        if (finish) {
            b->phase = 1;
            b->pattern = 0;
            b->patterns_done = 0;
            b->patterns_until_next = 1 + M_Random(c) % 6;
            b->frame = 0;
            b->hp_bar_filled = false;
            M_Palette(c, &b->boss_palette);
            b->shield.left = 304;
            b->shield.top = 144;
            M_Snap(b, c);
            M_WandRaise(b, c, true);
            M_BirdsReset(b);
            b->sequence = 0;
            return true;
        }
        if (b->frame % 2 != 0 && !M_Delay(b, vsync, 1)) {
            return false;
        }
    }
}
static void M_BeginTransform(B20M *const b, const B20M_CONTEXT *const c)
{
    b->frame = 0;
    b->hit.frame = 399;
    Shots_EraseAndReset(c->hit->shots, c->hit->shots_output);
    Pellets_EraseAndResetMoving(c->pellets, M_Video(c));
    ShootoutLasers_EraseAndReset(c->lasers, M_Video(c), 10);
    ORB *const orb = c->hit->orb;
    orb->left = 600;
    orb->top = 280;
    orb->force = -8;
    orb->force_frame = 0;
    orb->velocity_x = ORB_X_LEFT_4;
    M_Player(c)->left = 304;
    M_Player(c)->deflecting = false;
    M_Player(c)->bomb_damaging = false;
    Player_Reset(M_Player(c), c->input, c->hit->display->rank);
    b->hit.invincible = false;
    *c->rng = c->hit->frame_rand;
    M_Audio(c, B20M_AUDIO_FADE);
    Video_SelectDrawingPage(M_Video(c), 1);
    // The absent BOSS6.GRP leaves the previous image and palette intact.
    M_Color(c, 15, 0, 0, 0);
    b->boss_palette = c->hit->palette->saved;
    M_CopyOther(c);
    Video_SelectDrawingPage(M_Video(c), 1);
    M_Pi(c, &c->assets->backgrounds[4], false);
    Video_SelectDrawingPage(M_Video(c), 0);
    b->sequence = 2;
    b->sequence_stage = 1;
}
static bool M_Transform(
    B20M *const b, const B20M_CONTEXT *const c, FRAME_VSYNC *const vsync,
    const uint64_t tick)
{
    if (!M_Ready(b, c, vsync, tick)) {
        return false;
    }
    for (;;) {
        switch (b->sequence_stage) {
        case 1:
            b->frame = M_W(b->frame + 1);
            if (b->frame < 200) {
                if (b->frame % 2 == 0) {
                    M_Color(c, 15, 15, 15, 15);
                } else {
                    M_Palette(c, &b->boss_palette);
                }
                b->sequence_stage = 2;
                return M_Scroll(b, vsync, b->frame % 2 == 0 ? 416 : 386);
            }
            if (b->frame == 200) {
                M_Palette(c, &b->boss_palette);
                b->sequence_stage = 2;
                return M_Scroll(b, vsync, 0);
            }
            for (int32_t i = 0; i < 3; i++) {
                c->hit->palette->resource.colors[15][i] = 0;
            }
            {
                const TRANSITION_CONTEXT t = M_TransitionContext(b, c, true);
                ASSERT(Transition_BeginDiagonal(&b->transition, &t, 0, tick));
            }
            Video_SelectDrawingPage(M_Video(c), 1);
            M_Audio(c, B20M_AUDIO_LOAD_SECOND_MUSIC);
            M_Audio(c, B20M_AUDIO_PLAY);
            M_Pi(c, &c->assets->backgrounds[5], true);
            M_Color(c, 6, 0, 0, 0);
            M_CopyOther(c);
            M_Hud(c, vsync);
            b->sequence_stage = 3;
            return M_Scroll(b, vsync, 0);
        case 2:
            if (b->frame < 200 && b->frame % 4 == 0) {
                M_Effect(c, 9);
            }
            b->sequence_stage = 1;
            if (!M_Delay(b, vsync, 1)) {
                return false;
            }
            continue;
        case 3:
            b->frame = 0;
            b->hit.frame = 0;
            b->pattern = 0;
            b->patterns_done = 0;
            b->phase = 100;
            M_Player(c)->invincibility_time = 0;
            M_Player(c)->invincible = false;
            b->boss_palette = c->hit->palette->saved;
            b->stage_palette = b->boss_palette;
            b->hp = 6;
            c->hit->health->first_white = 10;
            c->hit->health->first_redwhite = 3;
            b->hp_bar_filled = false;
            M_Leaves(b, c, 999);
            b->sequence = 0;
            return true;
        default:
            ASSERT(false);
            return false;
        }
    }
}
static void M_BeginDefeat(B20M *const b, const B20M_CONTEXT *const c)
{
    Video_SelectDrawingPage(M_Video(c), 1);
    M_Pi(c, &c->assets->backgrounds[5], true);
    M_Color(c, 6, 0, 0, 0);
    b->boss_palette = c->hit->palette->saved;
    M_CopyOther(c);
    Video_SelectDrawingPage(M_Video(c), 0);
    b->sequence = 3;
    b->sequence_stage = 1;
}
static bool M_Defeat(
    B20M *const b, const B20M_CONTEXT *const c, FRAME_VSYNC *const vsync,
    const uint64_t tick)
{
    if (!M_Ready(b, c, vsync, tick)) {
        return false;
    }
    for (;;) {
        switch (b->sequence_stage) {
        case 1:
            b->frame = M_W(b->frame + 1);
            if (b->frame < 230) {
                b->sequence_stage = 2;
                return M_Scroll(b, vsync, b->frame % 2 == 0 ? 416 : 386);
            }
            b->sequence_stage = 2;
            continue;
        case 2:
            if (b->frame < 200 && b->frame % 4 == 0) {
                M_Effect(c, 9);
            }
            if (b->frame == 170) {
                M_Audio(c, B20M_AUDIO_FADE);
            }
            if (b->frame > 190) {
                if (b->frame % 2 == 0) {
                    for (int32_t i = 0; i < 16; i++) {
                        for (int32_t j = 0; j < 3; j++) {
                            c->hit->palette->saved.colors[i][j]--;
                        }
                    }
                    M_Palette(c, &c->hit->palette->saved);
                }
                if (b->frame > 230) {
                    b->sequence_stage = 3;
                    return M_Scroll(b, vsync, 0);
                }
            }
            b->sequence_stage = 1;
            if (!M_Delay(b, vsync, 2)) {
                return false;
            }
            continue;
        case 3:
            for (int32_t i = 0; i < 5; i++) {
                c->hit->display->gameplay->score += 10000;
            }
            *c->game_cleared = true;
            b->finished = true;
            b->sequence = 0;
            return true;
        default:
            ASSERT(false);
            return false;
        }
    }
}
static void M_Hit(B20M *const b, const B20M_CONTEXT *const c)
{
    BOSS_HIT_CONTEXT hit = *c->hit;
    hit.hp = &b->hp;
    hit.boss_palette = &b->boss_palette;
    const ORB *const orb = hit.orb;
    const bool collision = (orb->left >= 314 && orb->left <= 346
                            && orb->top >= 104 && orb->top <= 136)
        || BossEntity_HitTestOrb(&b->shield, orb);
    const uint8_t colors[] = { 3, 4, 5 };
    BossHit_UpdateAndRender(
        &b->hit, &hit, colors, 3, 10000, collision,
        (BOSS_SHOT_HITBOX) { 314, 104, 64, 32 });
}
static void M_PhaseNext(
    B20M *const b, const B20M_CONTEXT *const c, const int8_t phase)
{
    b->phase = phase;
    b->frame = 0;
    M_Snap(b, c);
}
static void M_PatternNext(
    B20M *const b, const int16_t first, const int16_t last)
{
    b->pattern = last == 1   ? M_W(1 - b->pattern)
        : b->pattern == last ? first
                             : M_W(b->pattern + 1);
    b->patterns_done = M_W(b->patterns_done + 1);
}
static void M_Pulse(B20M *const b, const B20M_CONTEXT *const c)
{
    if (b->frame % 10 != 0) {
        return;
    }
    M_Palette(c, &b->boss_palette);
    int8_t *const color = c->hit->palette->saved.colors[6];
    if (b->patterns_done == 0) {
        if (color[0] < 10) {
            color[0]++;
        } else {
            b->patterns_done = 1;
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
            b->patterns_done = 0;
        }
        if (color[1] > 0) {
            color[1]--;
        }
        if (color[2] > 0) {
            color[2]--;
        }
    }
    M_Palette(c, &c->hit->palette->saved);
    b->boss_palette = c->hit->palette->saved;
}
static bool M_Pattern(
    B20M *const b, const B20M_CONTEXT *const c, FRAME_VSYNC *const vsync)
{
    switch (b->call_phase) {
    case 1:
        if (b->pattern == 0) {
            M_PurpleLasers(b, c);
        } else if (b->pattern == 1) {
            M_Vortices(b, c);
        } else if (b->pattern == 2) {
            M_Eggs(b, c);
        }
        break;
    case 3:
        if (b->pattern == 0) {
            M_Flake(b, c);
            M_Rings(b, c);
        } else if (b->pattern == 1) {
            M_Sling(b, c);
            M_Rings(b, c);
        } else if (b->frame < 60) {
            b->frame = 0;
        }
        break;
    case 5:
        if (b->pattern == 0) {
            M_FourLasers(b, c);
        } else if (b->pattern == 1 || b->pattern == 4) {
            return M_Shake(b, c, vsync);
        } else if (b->pattern == 2 || b->pattern == 5) {
            M_FourLasers(b, c);
            M_TopRain(b, c);
        } else if (b->pattern == 3) {
            M_Radial(b, c);
        }
        break;
    case 7:
        if (b->pattern == 0) {
            M_SymmetricBirds(b, c);
        } else if (b->pattern == 1) {
            M_Semicircles(b, c);
        } else if (b->pattern == 2) {
            M_Stacks(b, c);
        }
        break;
    case 100:
        switch (b->pattern) {
        case 0:
            b->pattern_state = M_Rank(c, 56, 32, 24, 20);
            if (b->frame > 100) {
                b->frame = 0;
            }
            break;
        case 1:
            M_SprayOnce(b, c);
            break;
        case 2:
            M_SealRain(b, c);
            break;
        case 3:
            M_SprayTwice(b, c);
            break;
        case 4:
            M_Leaves(b, c, b->pattern_state);
            if (b->frame > 300) {
                b->frame = 0;
            }
            break;
        case 5:
            M_Leaves(b, c, 500);
            if (b->frame > 200) {
                b->frame = 0;
            }
            break;
        case 6:
            M_Leaves(b, c, 500);
            M_SprayOnce(b, c);
            break;
        case 7:
            M_Leaves(b, c, 500);
            M_SealRain(b, c);
            break;
        case 8:
            M_Leaves(b, c, 32);
            M_SprayTwice(b, c);
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
static bool M_Tail(B20M *const b, const B20M_CONTEXT *const c)
{
    const int8_t phase = b->call_phase;
    if (b->frame == 0) {
        M_PatternNext(
            b,
            phase == 5         ? 10
                : phase == 100 ? 4
                               : 0,
            phase == 1 || phase == 7 ? 2
                : phase == 3         ? 1
                : phase == 5         ? 5
                                     : 8);
    }
    M_Hit(b, c);
    if (phase == 100) {
        M_Pulse(b, c);
        if (b->hp <= 0) {
            M_Audio(c, B20M_AUDIO_STOP);
            b->phase = 101;
            b->frame = 0;
            b->hit.frame = 0;
        }
    } else {
        if (b->hp <= 0) {
            b->phase = 99;
        }
        if (b->patterns_done >= b->patterns_until_next && !b->hit.invincible) {
            b->patterns_done = 0;
            b->phase = phase + 1;
            b->pattern = 0;
            b->frame = 0;
        }
    }
    return true;
}
static void M_AfterGust(B20M *const b, const B20M_CONTEXT *const c)
{
    const int8_t phase = b->call_phase;
    M_Dress(b, c);
    if (b->frame != 0) {
        return;
    }
    M_PhaseNext(b, c, phase == 8 ? 1 : phase + 1);
    if (phase != 2) {
        b->boss_palette = c->hit->palette->saved;
    }
    b->patterns_until_next = phase == 2 ? 1 + M_Random(c) % 5
        : phase == 4                    ? 3 + M_Random(c) % 4
        : phase == 6                    ? 2 + M_Random(c) % 5
                                        : 1 + M_Random(c) % 6;
}
bool B20M_UpdateAndRender(
    B20M *const b, const B20M_CONTEXT *const c, FRAME_VSYNC *const vsync,
    const uint64_t tick)
{
    if (b->finished) {
        return true;
    }
    if (b->sequence == 1) {
        return M_RingEntrance(b, c, vsync, tick);
    }
    if (b->sequence == 2) {
        return M_Transform(b, c, vsync, tick);
    }
    if (b->sequence == 3) {
        return M_Defeat(b, c, vsync, tick);
    }
    if (b->resume_stage != 0) {
        if (!M_Ready(b, c, vsync, tick)) {
            return false;
        }
        const uint8_t resume = b->resume_stage;
        b->resume_stage = 0;
        if (resume == 1) {
            M_Effect(c, 7);
            return M_Tail(b, c);
        }
        if (resume == 2) {
            b->frame = 0;
            return M_Tail(b, c);
        }
        if (resume == 4) {
            const int16_t image = b->call_phase == 2 ? 1
                : b->call_phase == 4                 ? 2
                : b->call_phase == 6                 ? 3
                                                     : 0;
            M_GustFinish(b, c, image, vsync);
            M_AfterGust(b, c);
            return true;
        }
        if (!M_Pattern(b, c, vsync)) {
            return false;
        }
        return M_Tail(b, c);
    }
    if (b->phase == 0) {
        b->frame = -1;
        b->hit.frame = 0;
        b->hit.invincible = false;
        b->boss_palette = c->hit->palette->saved;
        b->stage_palette = b->boss_palette;
        *c->rng = c->hit->frame_rand;
        b->sequence = 1;
        return M_RingEntrance(b, c, vsync, tick);
    }
    if (b->phase == 99) {
        M_BeginTransform(b, c);
        return M_Transform(b, c, vsync, tick);
    }
    if (b->phase == 101) {
        M_BeginDefeat(b, c);
        return M_Defeat(b, c, vsync, tick);
    }
    b->call_phase = b->phase;
    if (b->phase >= 2 && b->phase <= 8 && b->phase % 2 == 0) {
        b->frame = M_W(b->frame + 1);
        if (!M_Gust(b, c, vsync)) {
            return false;
        }
        M_AfterGust(b, c);
        return true;
    }
    if (b->phase != 1 && b->phase != 3 && b->phase != 5 && b->phase != 7
        && b->phase != 100) {
        return true;
    }
    if ((b->phase == 1 || b->phase == 100) && !b->hp_bar_filled) {
        b->hp_bar_filled =
            HudHp_Increment(c->hit->health, M_Video(c), b->hp, b->frame);
    }
    b->frame = M_W(b->frame + 1);
    b->hit.frame = M_W(b->hit.frame + 1);
    if (b->phase != 100) {
        M_Shield(b, c);
        M_Dress(b, c);
    }
    if (b->phase == 3 || b->phase == 7) {
        M_Vertical(b, c, b->phase == 7);
    } else if (b->phase == 5) {
        M_Wavy(b, c);
    } else if (b->phase == 100) {
        M_Horizontal(b, c);
    }
    if ((b->phase == 1 || b->phase == 7) && !M_BirdsUpdate(b, c, tick)) {
        return false;
    }
    if (!M_Pattern(b, c, vsync)) {
        return false;
    }
    return M_Tail(b, c);
}

RESULT B20M_ValidateContext(const B20M_CONTEXT *const c)
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
            || c->hit->display->mask == nullptr
            || c->hit->display->video == nullptr
            || c->hit->display->player == nullptr
            || c->hit->display->gameplay == nullptr || c->hit->shots == nullptr
            || c->hit->orb == nullptr || c->hit->test_damage == nullptr
            || c->hit->effect == nullptr || c->pellets == nullptr
            || c->pellet_shared == nullptr || c->lasers == nullptr
            || c->objects == nullptr || c->tram == nullptr
            || c->input == nullptr || c->rng == nullptr
            || c->game_cleared == nullptr || c->audio == nullptr
            || c->timer == nullptr || c->first_stage == nullptr
            || c->timer_initialized == nullptr,
        "Missing Sariel context");
    MUST(B20M_ValidateAssets(c->assets));
    MUST(Shots_ValidateSprites(c->hit->display->sprites));
    MUST(Hud_ValidateAssets(
        c->hit->display->mask, c->hit->display->sprites,
        c->hit->display->font));
    FAIL_IF(
        c->hit->health->backgrounds == nullptr
            || c->hit->hud->numbers == nullptr || c->tram->pixels == nullptr,
        "Missing Sariel display storage");
    return OK;
}
