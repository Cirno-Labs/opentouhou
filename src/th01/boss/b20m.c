#include "th01/boss/private.h"
#include "th01/finite.h"
#include "th01/gameplay.h"
#include "th01/grc.h"
#include "th01/pellets.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "th01/shape.h"
#include "th01/shootout_lasers.h"
#include "th01/sound.h"
#include "th01/sprite.h"
#include "th01/stage/objects.h"
#include "th01/timer.h"
#include "th01/tram.h"
#include "th01/transition.h"
#include "th01/wait.h"

#include "core/base/assert.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/pi.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

typedef struct {
    BOS shield;
    BOS wand;
    BOS dress;
    GRC vortex;
    GRC bird;
    GRC cross;
    GRC splash;
    PI_IMAGE lower;
    PI_IMAGE upper;
    PI_IMAGE backgrounds[6];
    const INTERLEAVE_ASSETS *interleave;
    const uint8_t *star;
    const uint8_t *flake;
    const uint8_t *spark;
    const uint8_t *leaf_left;
    const uint8_t *leaf_right;
} B20M_ASSETS;

typedef struct {
    double left[30];
    double top[30];
    double velocity_x[30];
    double velocity_y[30];
    int8_t reserved[30];
    int8_t hatch_time[30];
    int8_t hatch_frames[30];
    bool alive[30];
} B20M_BIRDS;
typedef struct {
    double left[30];
    double top[30];
    double velocity[30];
    int8_t color[30];
} B20M_PARTICLES;
typedef struct {
    int16_t left[30];
    int16_t top[30];
    int16_t velocity[30];
    int16_t age[30];
    int8_t color[30];
} B20M_WAVES;
typedef struct {
    int16_t left[30];
    int16_t top[30];
    int16_t velocity_x[30];
    int16_t velocity_y[30];
    int16_t flag[30];
} B20M_LEAVES;
typedef struct {
    int16_t x;
    int16_t y;
    int16_t velocity_x;
    int16_t velocity_y;
    int16_t image;
    int16_t previous_image;
} B20M_RAYS;
typedef struct {
    int16_t x;
    int16_t y;
    int16_t speed;
    int16_t subpattern;
    uint8_t angle;
} B20M_SPRAY;
typedef struct {
    int16_t left[2];
    int16_t top[2];
    int16_t previous_left[2];
    int16_t previous_top[2];
} B20M_VORTICES;
typedef struct {
    int16_t x[10];
    int16_t y[10];
    int16_t origin_x;
    int16_t origin_y;
    int16_t velocity_x;
    int16_t velocity_y;
    int16_t target;
} B20M_LASER;
typedef struct {
    int16_t left[30];
    int16_t top[30];
    int16_t alive;
} B20M_EGGS;
typedef struct {
    int16_t left;
    int16_t top;
    int16_t velocity_x;
    int16_t velocity_y;
} B20M_SPAWNER;
typedef struct {
    int16_t state;
    int16_t left;
    int16_t top;
    int16_t velocity_x;
    int16_t velocity_y;
} B20M_FLAKE;
typedef struct {
    int16_t left;
    int16_t top;
    int16_t radius_outer_1;
    int16_t radius_outer_2;
    int16_t radius_inner;
} B20M_STAR;
typedef struct {
    int16_t x;
    int16_t y;
    int16_t offset;
    int16_t color;
    int16_t id;
} B20M_GUST;
typedef enum {
    B20M_PHASE_START = 0,
    B20M_PHASE_BIRDS = 1,
    B20M_PHASE_GUST_1 = 2,
    B20M_PHASE_VERTICAL = 3,
    B20M_PHASE_GUST_2 = 4,
    B20M_PHASE_WAVY = 5,
    B20M_PHASE_GUST_3 = 6,
    B20M_PHASE_VERTICAL_BIRDS = 7,
    B20M_PHASE_GUST_4 = 8,
    B20M_PHASE_TRANSFORM = 99,
    B20M_PHASE_SECOND_FORM = 100,
    B20M_PHASE_DEFEAT = 101,
} B20M_PHASE;

typedef enum {
    B20M_SEQUENCE_NONE,
    B20M_SEQUENCE_RINGS,
    B20M_SEQUENCE_TRANSFORM,
    B20M_SEQUENCE_DEFEAT,
} B20M_SEQUENCE;

typedef enum {
    B20M_RESUME_NONE,
    B20M_RESUME_SHAKE,
    B20M_RESUME_SHAKE_END,
    B20M_RESUME_BIRD_HIT,
    B20M_RESUME_GUST_END,
} B20M_RESUME;

typedef struct {
    BOSS_ENTITY shield;
    BOSS_ANIMATION dress;
    BOSS_ANIMATION wand;
    BOSS_HIT hit;
    PALETTE boss_palette;
    PALETTE stage_palette;
    uint8_t wand_background[128 * 96];
    B20M_BIRDS birds;
    B20M_PARTICLES vertical;
    B20M_PARTICLES horizontal;
    B20M_WAVES waves;
    B20M_LEAVES leaves;
    B20M_RAYS bird_rays;
    B20M_RAYS stack_rays;
    B20M_RAYS seal_rays;
    B20M_SPRAY sprays[2];
    B20M_VORTICES vortices;
    B20M_LASER laser;
    B20M_EGGS eggs;
    B20M_SPAWNER spawner;
    B20M_FLAKE flake;
    B20M_STAR star;
    B20M_GUST gust;
    BOSS_STATE status;
    int16_t pattern;
    int16_t pattern_state;
    int16_t patterns_done;
    int16_t patterns_until_next;
    int16_t entrance_radius;
    int16_t wand_frame;
    int16_t bird_pellet_group;
    int16_t ray_previous_x;
    int16_t ray_previous_y;
    int16_t dotted_radius;
    int16_t rings_interval;
    int16_t scroll_line;
    WAIT wait;
    uint8_t rings_angle, radial_angle, radial_velocity, sequence,
        sequence_stage, resume_stage;
    int8_t call_phase;
    bool vortex_wand_done;
    bool bird_wand_done;
    bool direction_first;
    bool direction_second;
    bool hp_bar_filled;
    bool dotted_active;
    bool entrance_active;
    bool finished;
} B20M;

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

static B20M m_B20M;

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
static RESULT M_ValidateAssets(const B20M_ASSETS *const a)
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
static RESULT M_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const a, void *const data)
{
    B20M_ASSETS *const out = data;
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
    MUST(M_ValidateAssets(&loaded));
    *out = loaded;
    return OK;
}
static void M_Init(void)
{
    memset(&m_B20M, 0, sizeof(m_B20M));
    Transition_Init();
    m_B20M.ray_previous_x = 999;
    m_B20M.ray_previous_y = 999;
    m_B20M.flake.state = -1;
}
static void M_Setup(const void *const data, const BOSS_CONTEXT *const ctx)
{
    const B20M_ASSETS *const assets = data;
    Boss_Select(&m_B20M.status);
    ASSERT(
        m_B20M.sequence == B20M_SEQUENCE_NONE && Wait_Idle(&m_B20M.wait)
        && !m_B20M.entrance_active);
    ASSERT(Result_Ignore(M_ValidateAssets(assets)));
    ASSERT(Result_Ignore(BossEntity_Bind(&m_B20M.shield, 0, &assets->shield)));
    ASSERT(Result_Ignore(BossAnimation_Bind(&m_B20M.wand, 0, &assets->wand)));
    ASSERT(Result_Ignore(BossAnimation_Bind(&m_B20M.dress, 1, &assets->dress)));
    BossEntity_SetPosition(
        &m_B20M.shield, 304, 144, (BOSS_BOUNDS) { 0, 640, 0, 400 });
    BossEntity_SetOrbHitbox(&m_B20M.shield, (BOSS_BOUNDS) { -16, 64, -16, 64 });
    m_B20M.shield.orb_inactive = false;
    m_B20M.dress.left = 280;
    m_B20M.dress.top = 192;
    m_B20M.wand.left = 296;
    m_B20M.wand.top = 48;
    m_B20M.status.hp = 18;
    m_B20M.status.phase = B20M_PHASE_START;
    m_B20M.status.frame = 0;
    m_B20M.finished = false;
    m_B20M.boss_palette = g_Palette->saved;
    HudHp_Begin(2, 8);
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
    Palette_ImportResource(image->palette);
    if (show) {
        Palette_SetGlobal(&g_Palette->resource);
    }
}
static void M_Palette(const B20M_ASSETS *const assets, const PALETTE *const p)
{
    Palette_SetGlobal(p);
}
static void M_Color(
    const B20M_ASSETS *const assets, const uint8_t i, const int16_t r,
    const int16_t g, const int16_t blue)
{
    Palette_SetGlobalColor(i, r, g, blue);
}
static void M_CopyOther(const B20M_ASSETS *const assets)
{
    const int32_t page = g_Video->drawing_page;
    Video_Copy(page, (GFX_RECT) { 0, 0, 640, 400 }, 1 - page, 0, 0);
}
static void M_CopyRows(
    const B20M_ASSETS *const assets, const int16_t top, const int16_t height)
{
    for (int32_t i = 0; i < height * 80; i++) {
        const uint16_t offset = (uint16_t)(top * 80 + i) & 0x7fff;
        if (offset < 32000) {
            memcpy(
                g_Video->pages[0] + offset * 8, g_Video->pages[1] + offset * 8,
                8);
        }
    }
    Video_SelectDrawingPage(0);
}
static void M_Snap(const B20M_ASSETS *const assets)
{
    const int32_t left = m_B20M.wand.left / 8 * 8;
    for (int32_t y = 0; y < 96; y++) {
        memcpy(
            m_B20M.wand_background + y * 128,
            g_Video->pages[1] + (m_B20M.wand.top + y) * 640 + left, 128);
    }
    Video_SelectDrawingPage(0);
}
static void M_WandPut(const B20M_ASSETS *const assets)
{
    const VIDEO_IMAGE v = { .pixels = m_B20M.wand_background,
                            .width = 128,
                            .height = 96,
                            .stride = 128,
                            .key = -1 };
    Video_Blit(
        &v, (GFX_RECT) { 0, 0, 128, 96 }, m_B20M.wand.left / 8 * 8,
        m_B20M.wand.top);
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
        m_B20M.wand_frame = 0;
    } else if (m_B20M.wand_frame == 0) {
        m_B20M.wand_frame = 1;
    } else {
        m_B20M.wand_frame = Math16_Wrap(m_B20M.wand_frame + 1);
        if (m_B20M.wand_frame == 2 || m_B20M.wand_frame == 16) {
            m_B20M.wand.image = m_B20M.wand_frame == 2 ? 0 : 1;
            Video_SelectDrawingPage(1);
            BossAnimation_Put(&m_B20M.wand);
            Video_SelectDrawingPage(0);
            BossAnimation_Put(&m_B20M.wand);
            if (m_B20M.wand_frame == 16) {
                m_B20M.wand_frame = 0;
                return true;
            }
        }
    }
    return false;
}
static void M_Shield(const B20M_ASSETS *const assets)
{
    if (m_B20M.status.frame % 10 != 0) {
        return;
    }
    m_B20M.shield.image = (m_B20M.status.frame % 40) / 10;
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        m_B20M.shield.lock_frame = 0;
        BossEntity_LockedMovePut(&m_B20M.shield, 0, 0, 1, false);
    }
}
static void M_Dress(const B20M_ASSETS *const assets)
{
    if (m_B20M.status.frame % 15 != 0) {
        return;
    }
    m_B20M.dress.image = (m_B20M.status.frame % 60) / 15;
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        BossAnimation_Put(&m_B20M.dress);
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
        ((index % 10) + 10) % 10, x, y, tx, ty, speed, color, age, width);
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
                g_Video->pages[g_Video->drawing_page][offset] = color & 15;
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
        m_B20M.ray_previous_x = m_B20M.ray_previous_y = 999;
        return;
    }
    if (m_B20M.ray_previous_x >= 0 && m_B20M.ray_previous_x < 640
        && m_B20M.ray_previous_y >= 0 && m_B20M.ray_previous_y < 400) {
        Video_LineErase(ox, oy, m_B20M.ray_previous_x, m_B20M.ray_previous_y);
    }
    if (x >= 0 && x < 640 && y >= 0 && y < 400) {
        Video_Line(ox, oy, x, y, color);
    }
    m_B20M.ray_previous_x = x;
    m_B20M.ray_previous_y = y;
}
static const PALETTE *M_TransitionTarget(const bool fade)
{
    return fade ? &g_Palette->resource : &g_Palette->saved;
}
static void M_BirdsReset(void)
{
    for (int32_t i = 0; i < 30; i++) {
        m_B20M.birds.alive[i] = false;
    }
}
static void M_BirdSpawn(
    const double x, const double y, const double vx, const double vy)
{
    if (x == 0) {
        return;
    }
    for (int32_t i = 0; i < 30; i++) {
        if (!m_B20M.birds.alive[i]) {
            m_B20M.birds.alive[i] = true;
            m_B20M.birds.left[i] = x;
            m_B20M.birds.top[i] = y;
            m_B20M.birds.velocity_x[i] = vx;
            m_B20M.birds.velocity_y[i] = vy;
            m_B20M.birds.reserved[i] = 1;
            m_B20M.birds.hatch_time[i] = 25;
            m_B20M.birds.hatch_frames[i] = 25;
            return;
        }
    }
}
static void M_BirdsFire(const B20M_ASSETS *const assets, const int16_t group)
{
    for (int32_t i = 0; i < 30; i++) {
        if (!m_B20M.birds.alive[i] || m_B20M.birds.hatch_time[i] != 0) {
            continue;
        }
        const int16_t x = M_PixelAdd(m_B20M.birds.left[i], 12),
                      y = M_PixelAdd(m_B20M.birds.top[i], 12);
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
    B20M_BIRDS *const birds = &m_B20M.birds;
    for (int32_t i = 0; i < 30; i++) {
        if (!birds->alive[i]) {
            continue;
        }
        const int16_t x = M_Pixel(birds->left[i]);
        const int16_t y = M_Pixel(birds->top[i]);
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
        const int16_t x = M_Pixel(birds->left[i]);
        const int16_t y = M_Pixel(birds->top[i]);
        if (birds->left[i] < 0 || birds->left[i] > 639 || birds->top[i] < 0
            || birds->top[i] > 399) {
            M_Erase(assets, x, y);
            birds->alive[i] = false;
            continue;
        }
        M_Grc(assets, x, y, 1, 6 + (m_B20M.status.frame / 5) % 2, 15);
        if (!g_Player->invincible && birds->left[i] > g_Player->left - 8
            && birds->left[i] < g_Player->left + 8 && birds->top[i] > 344
            && birds->top[i] < 384) {
            Player_Hit();
            Wait_Ticks(&m_B20M.wait, tick, 6);
            m_B20M.resume_stage = B20M_RESUME_BIRD_HIT;
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
            if (g_Video->pages[1][offset * 8 + bit] == 12) {
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
                g_Video->pages[0][offset * 8 + bit] = color & 15;
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
    B20M_PARTICLES *const p = &m_B20M.vertical;
    if (m_B20M.status.frame % 7 == 0) {
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
    if (m_B20M.status.frame % 2 != 0) {
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
    B20M_WAVES *const p = &m_B20M.waves;
    if (m_B20M.status.frame % 7 == 0) {
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
    if (m_B20M.status.frame % 2 != 0) {
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
static void M_B20mHorizontal(const B20M_ASSETS *const assets)
{
    B20M_PARTICLES *const p = &m_B20M.horizontal;
    if (m_B20M.status.frame % 7 == 0) {
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
    if (m_B20M.status.frame % 2 != 0) {
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
    m_B20M.stage_palette = g_Palette->saved;
    Video_SelectDrawingPage(0);
    M_Hud(assets);
    HudHp_Rerender(m_B20M.status.hp);
    m_B20M.status.frame = 0;
}
static bool M_Gust(const B20M_ASSETS *const assets)
{
    if (m_B20M.status.frame == 20) {
        m_B20M.gust.id = 0;
    }
    if (m_B20M.status.frame < 50) {
        return true;
    }
    if (m_B20M.status.frame == 50) {
        m_B20M.gust.x = 0;
        m_B20M.gust.y = 0;
        m_B20M.gust.offset = 0;
        m_B20M.gust.color = 2;
        Sound_Effect(SOUND_EFFECT_GUST);
    }
    for (int32_t row = 0; row < 100; row++) {
        const int16_t right = M_Random(assets) % 8;
        uint16_t masks[10] = {};
        const int16_t first = Math16_Wrap(m_B20M.gust.offset + right);
        for (int32_t stripe = 0; stripe < 10; stripe++) {
            const int16_t offset = Math16_Wrap(first - stripe * 2);
            if (first / 80 != offset / 80) {
                break;
            }
            if (offset >= 0 && offset / 80 == m_B20M.gust.offset / 80) {
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
            if (offset >= 0 && offset / 80 == m_B20M.gust.offset / 80) {
                M_MaskedByte(
                    assets, offset, (uint8_t)masks[stripe],
                    (uint8_t)(m_B20M.gust.color
                              + (stripe > 2 ? 4 - stripe : stripe)));
            }
        }
        for (int32_t stripe = 0; stripe < m_B20M.gust.id + 2; stripe++) {
            const int16_t offset = Math16_Wrap(first - 10 - stripe * 2);
            if (offset / 80 != first / 80) {
                break;
            }
            if (offset >= 0 && offset / 80 == m_B20M.gust.offset / 80) {
                M_MaskedByte(assets, offset, (uint8_t)masks[5 + stripe], 12);
            }
        }
        m_B20M.gust.y = Math16_Wrap(m_B20M.gust.y + 4);
        m_B20M.gust.offset = Math16_Wrap(
            m_B20M.gust.y * 80 + m_B20M.gust.x / 8 - (m_B20M.gust.x % 8 < 0));
        if (m_B20M.gust.offset >= 32000) {
            m_B20M.gust.y = 0;
            m_B20M.gust.x = Math16_Wrap(m_B20M.gust.x + 32);
            m_B20M.gust.offset = m_B20M.gust.x / 8;
        }
    }
    Video_SelectDrawingPage(0);
    if (m_B20M.gust.x > 608) {
        m_B20M.gust.id = Math16_Wrap(m_B20M.gust.id + 1);
        m_B20M.status.frame = 49;
        if (m_B20M.gust.id >= 3) {
            m_B20M.resume_stage = B20M_RESUME_GUST_END;
            return Wait_Scroll(&m_B20M.wait, 0);
        }
    }
    return true;
}
static void M_VortexDraw(
    const B20M_ASSETS *const assets, const int32_t i, const bool spread)
{
    M_Erase(
        assets, m_B20M.vortices.previous_left[i],
        m_B20M.vortices.previous_top[i]);
    M_Grc(
        assets, m_B20M.vortices.left[i], m_B20M.vortices.top[i], 0,
        m_B20M.status.frame % 3, 7);
    if (spread && m_B20M.status.frame % 4 == 0) {
        const uint8_t angle = M_Angle(
            388 - m_B20M.vortices.top[i],
            m_B20M.vortices.left[1 - i] - m_B20M.vortices.left[i]);
        M_Single(
            assets, Math16_Wrap(m_B20M.vortices.left[i] + 12),
            Math16_Wrap(m_B20M.vortices.top[i] + 12), 64, 112);
        M_Single(
            assets, Math16_Wrap(m_B20M.vortices.left[i] + 12),
            Math16_Wrap(m_B20M.vortices.top[i] + 12), angle, 112);
        M_Single(
            assets, Math16_Wrap(m_B20M.vortices.left[i] + 12),
            Math16_Wrap(m_B20M.vortices.top[i] + 12), (uint8_t)(128 - angle),
            112);
    }
}
static void M_Vortices(const B20M_ASSETS *const assets)
{
    if (!m_B20M.vortex_wand_done) {
        m_B20M.vortex_wand_done = M_WandRaise(assets, false);
    }
    if (m_B20M.status.frame < 50) {
        return;
    }
    if (m_B20M.status.frame == 50) {
        m_B20M.pattern_state = Boss_Rank(140, 145, 150, 155);
    }
    if (m_B20M.status.frame < 100) {
        for (int32_t i = 0; i < 2; i++) {
            m_B20M.vortices.left[i] = (int16_t)(i * 608);
            m_B20M.vortices.top[i] = 200;
            M_VortexDraw(assets, i, false);
            if (m_B20M.status.frame % 4 == 0) {
                M_Group(
                    assets, Math16_Wrap(m_B20M.vortices.left[i] + 12),
                    Math16_Wrap(m_B20M.vortices.top[i] + 12),
                    PELLET_GROUP_SINGLE, 112);
            }
            m_B20M.vortices.previous_left[i] = m_B20M.vortices.left[i];
            m_B20M.vortices.previous_top[i] = m_B20M.vortices.top[i];
        }
    } else if (m_B20M.status.frame < m_B20M.pattern_state) {
        for (int32_t i = 0; i < 2; i++) {
            m_B20M.vortices.left[i] =
                Math16_Wrap(m_B20M.vortices.left[i] + (i == 0 ? 5 : -5));
            m_B20M.vortices.top[i] = Math16_Wrap(m_B20M.vortices.top[i] - 2);
            M_VortexDraw(assets, i, false);
            if (m_B20M.status.frame % 4 == 0) {
                M_Group(
                    assets, Math16_Wrap(m_B20M.vortices.left[i] + 12),
                    Math16_Wrap(m_B20M.vortices.top[i] + 12),
                    PELLET_GROUP_SINGLE, 112);
                M_Single(
                    assets, Math16_Wrap(m_B20M.vortices.left[i] + 12),
                    Math16_Wrap(m_B20M.vortices.top[i] + 12),
                    M_Angle(412 - m_B20M.vortices.top[i], i * 160 - 68), 112);
            }
            m_B20M.vortices.previous_left[i] = m_B20M.vortices.left[i];
            m_B20M.vortices.previous_top[i] = m_B20M.vortices.top[i];
        }
    } else if (m_B20M.status.frame < 200) {
        for (int32_t i = 0; i < 2; i++) {
            M_VortexDraw(assets, i, true);
        }
    } else if (m_B20M.status.frame < 240) {
        if (m_B20M.status.frame == 200) {
            m_B20M.direction_first = M_Random(assets) % 2 != 0;
            if (m_B20M.direction_first) {
                M_WandLower(assets);
            }
        }
        for (int32_t i = 0; i < 2; i++) {
            m_B20M.vortices.left[i] = Math16_Wrap(
                m_B20M.vortices.left[i] + 2 - m_B20M.direction_first * 4);
            M_VortexDraw(assets, i, true);
            m_B20M.vortices.previous_left[i] = m_B20M.vortices.left[i];
        }
    } else if (m_B20M.status.frame < 320) {
        if (m_B20M.status.frame == 240) {
            m_B20M.direction_second = M_Random(assets) % 2 != 0;
            if (!m_B20M.direction_second
                && m_B20M.direction_second == m_B20M.direction_first) {
                M_WandLower(assets);
            } else if (
                m_B20M.direction_second
                && m_B20M.direction_second == m_B20M.direction_first) {
                m_B20M.vortex_wand_done = false;
            }
        }
        for (int32_t i = 0; i < 2; i++) {
            m_B20M.vortices.left[i] = Math16_Wrap(
                m_B20M.vortices.left[i]
                - (m_B20M.direction_second != m_B20M.direction_first
                       ? 2 - m_B20M.direction_second * 4
                       : 4 - m_B20M.direction_second * 8));
            M_VortexDraw(assets, i, true);
            m_B20M.vortices.previous_left[i] = m_B20M.vortices.left[i];
        }
    } else {
        for (int32_t i = 0; i < 2; i++) {
            M_Erase(assets, m_B20M.vortices.left[i], m_B20M.vortices.top[i]);
        }
        M_WandLower(assets);
        m_B20M.status.frame = 0;
        m_B20M.vortex_wand_done = false;
    }
}
static void M_PurpleLasers(const B20M_ASSETS *const assets)
{
    if (m_B20M.status.frame < 50) {
        return;
    }
    if (m_B20M.status.frame == 50) {
        m_B20M.pattern_state = Boss_Rank(68, 72, 76, 80);
        for (int32_t i = 0; i < 10; i++) {
            m_B20M.laser.x[i] = 120 + M_Random(assets) % 400;
            m_B20M.laser.y[i] = 100 + M_Random(assets) % 100;
        }
    }
    if (m_B20M.status.frame < 150) {
        const int32_t i = (m_B20M.status.frame - 50) / 10;
        if (m_B20M.status.frame % 10 != 9) {
            if (m_B20M.status.frame % 10 != 0) {
                const int16_t r =
                    (int16_t)(64 - ((m_B20M.status.frame - 1) % 10) * 8);
                M_EraseEllipse(
                    assets, m_B20M.laser.x[i], m_B20M.laser.y[i], r, r, 1);
            }
            if (m_B20M.status.frame % 10 != 8) {
                const int16_t r =
                    (int16_t)(64 - (m_B20M.status.frame % 10) * 8);
                M_Ellipse(
                    assets, m_B20M.laser.x[i], m_B20M.laser.y[i], r, r, 4, 1);
            }
        } else {
            Sound_Effect(SOUND_EFFECT_LASER);
            M_Laser(
                assets, i, m_B20M.laser.x[i], m_B20M.laser.y[i],
                M_Random(assets) % 640, 400, m_B20M.pattern_state, 4, 25, 5);
        }
    } else if (m_B20M.status.frame > 200) {
        m_B20M.status.frame = 0;
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
    if (m_B20M.status.frame < 40) {
        return;
    }
    if (!m_B20M.bird_wand_done) {
        m_B20M.bird_wand_done = M_WandRaise(assets, false);
    }
    if (m_B20M.status.frame < 50) {
        return;
    }
    if (m_B20M.status.frame == 50) {
        m_B20M.eggs.alive = 0;
        m_B20M.spawner.left = 5440;
        m_B20M.spawner.top = 1024;
        m_B20M.spawner.velocity_x =
            (int16_t)(64 - (M_Random(assets) % 2) * 128);
        m_B20M.spawner.velocity_y = (int16_t)(32 - (M_Random(assets) % 2) * 64);
        m_B20M.pattern_state = Boss_Rank(20, 15, 10, 8);
        Sound_Effect(SOUND_EFFECT_TELEGRAPH);
    } else if (m_B20M.status.frame < 200) {
        m_B20M.spawner.velocity_x = Math16_Wrap(
            m_B20M.spawner.velocity_x + (m_B20M.spawner.left < 5120 ? 1 : -1));
        m_B20M.spawner.velocity_y = Math16_Wrap(
            m_B20M.spawner.velocity_y + (m_B20M.spawner.top < 2624 ? 1 : -1));
        M_Erase(
            assets, Math16_ToPixel(m_B20M.spawner.left),
            Math16_ToPixel(m_B20M.spawner.top));
        for (int32_t i = m_B20M.eggs.alive - 1; i >= 0; i--) {
            M_Grc(assets, m_B20M.eggs.left[i], m_B20M.eggs.top[i], 1, 0, 15);
        }
        m_B20M.spawner.left =
            Math16_Wrap(m_B20M.spawner.left + m_B20M.spawner.velocity_x);
        m_B20M.spawner.top =
            Math16_Wrap(m_B20M.spawner.top + m_B20M.spawner.velocity_y);
        if (m_B20M.status.frame % m_B20M.pattern_state == 0
            && M_In(
                m_B20M.spawner.left, m_B20M.spawner.top, 0, 0, 9728, 5888)) {
            if (m_B20M.eggs.alive < 30) {
                m_B20M.eggs.left[m_B20M.eggs.alive] =
                    Math16_ToPixel(m_B20M.spawner.left);
                m_B20M.eggs.top[m_B20M.eggs.alive] =
                    Math16_ToPixel(m_B20M.spawner.top);
                m_B20M.eggs.alive++;
            }
            M_Grc(
                assets, Math16_ToPixel(m_B20M.spawner.left),
                Math16_ToPixel(m_B20M.spawner.top), 1, 0, 15);
        }
        M_Grc(
            assets, Math16_ToPixel(m_B20M.spawner.left),
            Math16_ToPixel(m_B20M.spawner.top), 2, m_B20M.status.frame % 2,
            (uint8_t)(m_B20M.status.frame % 4 + 2));
    } else if (m_B20M.status.frame < 400) {
        if (m_B20M.status.frame == 200) {
            if (m_B20M.spawner.velocity_y > -4
                && m_B20M.spawner.velocity_y < 4) {
                m_B20M.spawner.velocity_y =
                    m_B20M.spawner.velocity_y < 0 ? -4 : 4;
            }
            if (m_B20M.spawner.velocity_x > -4
                && m_B20M.spawner.velocity_x < 4) {
                m_B20M.spawner.velocity_x =
                    m_B20M.spawner.velocity_x < 0 ? -4 : 4;
            }
            for (int32_t i = m_B20M.eggs.alive - 1; i >= 0; i--) {
                const MATH16_VECTOR v =
                    Math16_Vector(3, (M_Random(assets) & 127) + 128);
                M_BirdSpawn(m_B20M.eggs.left[i], m_B20M.eggs.top[i], v.x, v.y);
            }
            M_WandLower(assets);
            m_B20M.bird_pellet_group = M_Random(assets) % 2;
        }
        if (M_In(
                m_B20M.spawner.left, m_B20M.spawner.top, -512, -512, 10240,
                6400)) {
            M_Erase(
                assets, Math16_ToPixel(m_B20M.spawner.left),
                Math16_ToPixel(m_B20M.spawner.top));
            m_B20M.spawner.left =
                Math16_Wrap(m_B20M.spawner.left + m_B20M.spawner.velocity_x);
            m_B20M.spawner.top =
                Math16_Wrap(m_B20M.spawner.top + m_B20M.spawner.velocity_y);
            if (M_In(m_B20M.spawner.left, m_B20M.spawner.top, 0, 0, 608, 368)) {
                M_Grc(
                    assets, m_B20M.spawner.left, m_B20M.spawner.top, 2,
                    m_B20M.status.frame % 2,
                    (uint8_t)(m_B20M.status.frame % 4 + 2));
            }
        }
        if (m_B20M.status.frame % 16 == 0 || m_B20M.status.frame % 16 == 2) {
            M_BirdsFire(assets, m_B20M.bird_pellet_group);
        }
    } else if (m_B20M.status.frame == 400) {
        m_B20M.status.frame = 0;
        m_B20M.bird_wand_done = false;
    }
}
static void M_FlakeEllipse(
    const B20M_ASSETS *const assets, const int16_t rx, const int16_t ry,
    const uint8_t color, const uint8_t step, const bool erase)
{
    const int16_t x = Math16_ToPixel(m_B20M.flake.left);
    if (erase) {
        M_EraseEllipse(assets, x, 384, rx, ry, step);
    } else {
        M_Ellipse(assets, x, 384, rx, ry, color, step);
    }
}
static void M_Flake(const B20M_ASSETS *const assets)
{
    if (m_B20M.status.frame < 10) {
        m_B20M.flake.state = -1;
        m_B20M.pattern_state = 160;
    }
    if (m_B20M.status.frame % m_B20M.pattern_state == 0) {
        m_B20M.flake.state = 0;
    }
    if (m_B20M.flake.state == 0) {
        m_B20M.flake.state = M_WandRaise(assets, false);
    }
    if (m_B20M.flake.state == 1) {
        Sound_Effect(SOUND_EFFECT_LASER);
        m_B20M.flake.left = 5440;
        m_B20M.flake.top = 1024;
        const MATH16_VECTOR v = Math16_Aim(
            (MATH16_VECTOR) { 340, 64 },
            (MATH16_VECTOR) { Math16_Wrap(g_Player->left + 12), 380 }, 112, 0);
        m_B20M.flake.velocity_x = v.x;
        m_B20M.flake.velocity_y = v.y;
        m_B20M.flake.state = 2;
    }
    if (m_B20M.flake.state == 2) {
        Sprite_Restore16(
            Math16_ToPixel(m_B20M.flake.left), Math16_ToPixel(m_B20M.flake.top),
            16, 8);
        m_B20M.flake.top =
            Math16_Wrap(m_B20M.flake.top + m_B20M.flake.velocity_y);
        if (m_B20M.flake.top >= 6336) {
            m_B20M.flake.state = 3;
            M_WandLower(assets);
            return;
        }
        m_B20M.flake.left =
            Math16_Wrap(m_B20M.flake.left + m_B20M.flake.velocity_x);
        M_Mask(
            assets, Math16_ToPixel(m_B20M.flake.left),
            Math16_ToPixel(m_B20M.flake.top), assets->flake, 7);
    }
    if (m_B20M.flake.state == 3) {
        M_FlakeEllipse(assets, 48, 16, 6, 8, false);
        M_FlakeEllipse(assets, 16, 48, 6, 8, false);
        M_FlakeEllipse(assets, 24, 24, 7, 6, false);
        m_B20M.star.radius_outer_1 = 48;
        m_B20M.star.radius_outer_2 = 16;
        m_B20M.star.radius_inner = 24;
        m_B20M.star.left = m_B20M.flake.left;
        m_B20M.star.top = 400 - M_Random(assets) % 48;
        Sound_Effect(SOUND_EFFECT_GUST);
    }
    if (m_B20M.flake.state >= 3) {
        m_B20M.flake.state = Math16_Wrap(m_B20M.flake.state + 1);
    }
    if (m_B20M.flake.state >= 5 && m_B20M.flake.state % 8 == 0) {
        Sprite_Restore16(m_B20M.star.left, m_B20M.star.top, 16, 8);
        M_FlakeEllipse(
            assets, m_B20M.star.radius_outer_1, m_B20M.star.radius_outer_2, 0,
            8, true);
        M_FlakeEllipse(
            assets, m_B20M.star.radius_outer_2, m_B20M.star.radius_outer_1, 0,
            8, true);
        M_FlakeEllipse(
            assets, m_B20M.star.radius_inner, m_B20M.star.radius_inner, 0, 6,
            true);
        m_B20M.star.radius_outer_1 =
            Math16_Wrap(m_B20M.star.radius_outer_1 - 6);
        m_B20M.star.radius_outer_2 =
            Math16_Wrap(m_B20M.star.radius_outer_2 + 6);
        m_B20M.star.radius_inner = Math16_Wrap(m_B20M.star.radius_inner + 4);
        m_B20M.star.left = Math16_Wrap(
            M_Random(assets) % 64 + Math16_ToPixel(m_B20M.flake.left) - 28);
        m_B20M.star.top = 400 - M_Random(assets) % 48;
        if (m_B20M.star.radius_outer_1 <= 8) {
            m_B20M.flake.state = -1;
            return;
        }
        M_FlakeEllipse(
            assets, m_B20M.star.radius_outer_1, m_B20M.star.radius_outer_2, 5,
            8, false);
        M_FlakeEllipse(
            assets, m_B20M.star.radius_outer_2, m_B20M.star.radius_outer_1, 5,
            8, false);
        M_FlakeEllipse(
            assets, m_B20M.star.radius_inner, m_B20M.star.radius_inner, 7, 6,
            false);
        M_Mask(assets, m_B20M.star.left, m_B20M.star.top, assets->star, 7);
    }
    const int16_t x = Math16_ToPixel(m_B20M.flake.left);
    if (!g_Player->invincible && x + 32 > g_Player->left
        && x - 64 < g_Player->left && m_B20M.flake.state >= 3) {
        Player_Hit();
    }
}
static void M_Rings(const B20M_ASSETS *const assets)
{
    if (m_B20M.status.frame < 5) {
        m_B20M.rings_angle = 0;
        m_B20M.rings_interval = Boss_Rank(18, 12, 10, 8);
    }
    if (m_B20M.status.frame % m_B20M.rings_interval == 0) {
        Pellets_SpawnWithCloud(true);
        M_Single(assets, 568, 160, (uint8_t)(0 - m_B20M.rings_angle), 56);
        M_Single(assets, 568, 160, (uint8_t)(128 - m_B20M.rings_angle), 56);
        M_Single(assets, 64, 160, m_B20M.rings_angle, 56);
        M_Single(assets, 64, 160, (uint8_t)(128 + m_B20M.rings_angle), 56);
        m_B20M.rings_angle =
            (uint8_t)(m_B20M.rings_angle + m_B20M.rings_interval);
    }
    if (m_B20M.status.frame > 479) {
        m_B20M.status.frame = 0;
    }
}
static void M_Sling(const B20M_ASSETS *const assets)
{
    if (m_B20M.status.frame == 10) {
        m_B20M.pattern_state = Boss_Rank(4, 3, 2, 2);
    }
    if (m_B20M.status.frame % 200 >= 150
        && m_B20M.status.frame % m_B20M.pattern_state == 0) {
        const uint8_t angle = (uint8_t)M_Random(assets);
        Pellets_AddSingle(
            g_Session.settings.difficulty, Math16_PolarY(320, 40, angle),
            Math16_PolarX(180, 40, angle), 0, 0, PELLET_SLING_AIMED, 72, 0, 0);
    }
    if (m_B20M.status.frame >= 299) {
        m_B20M.status.frame = 0;
    }
}
static void M_FourLasers(const B20M_ASSETS *const assets)
{
    if (m_B20M.status.frame < 5) {
        M_Ray(assets, 0, 0, 0, 0, 99);
    }
    if (m_B20M.status.frame < 50) {
        return;
    }
    int16_t x = 0;
    int16_t y = 0;
    int16_t target = 0;
    int32_t laser = -1;
    if (m_B20M.status.frame == 50) {
        x = 460;
        y = 120;
        target = g_Player->left;
        laser = 0;
        m_B20M.laser.target = target;
        m_B20M.pattern_state = Boss_Rank(60, 64, 68, 72);
    } else if (m_B20M.status.frame == 100) {
        x = 180;
        y = 120;
        target = Math16_Wrap(639 - m_B20M.laser.target);
        laser = 1;
    } else if (m_B20M.status.frame == 150) {
        x = 608;
        y = 164;
        target = g_Player->left;
        laser = 2;
    } else if (m_B20M.status.frame == 200) {
        x = 24;
        y = 164;
        target = g_Player->left;
        laser = 3;
    }
    if (laser >= 0) {
        m_B20M.laser.origin_x = x;
        m_B20M.laser.origin_y = y;
        const MATH16_VECTOR v = Math16_Aim(
            (MATH16_VECTOR) { x, y }, (MATH16_VECTOR) { target, 400 }, 16, 0);
        m_B20M.laser.velocity_x = v.x;
        m_B20M.laser.velocity_y = v.y;
        M_Laser(
            assets, laser, x, y, target, 400, m_B20M.pattern_state, 7, 50, 8);
        Sound_Effect(SOUND_EFFECT_LASER);
    } else if (m_B20M.status.frame < 250) {
        M_Ray(
            assets, 320, 120, m_B20M.laser.origin_x, m_B20M.laser.origin_y, 7);
        m_B20M.laser.origin_x =
            Math16_Wrap(m_B20M.laser.origin_x + m_B20M.laser.velocity_x);
        m_B20M.laser.origin_y =
            Math16_Wrap(m_B20M.laser.origin_y + m_B20M.laser.velocity_y);
    } else if (m_B20M.status.frame > 300) {
        m_B20M.status.frame = 0;
    }
}
static bool M_Shake(const B20M_ASSETS *const assets)
{
    if (m_B20M.status.frame < 50) {
        if (m_B20M.status.frame % 8 == 0 || m_B20M.status.frame % 8 == 4) {
            m_B20M.resume_stage = B20M_RESUME_SHAKE;
            return Wait_Scroll(
                &m_B20M.wait, m_B20M.status.frame % 8 == 0 ? 396 : 404);
        }
        if (m_B20M.status.frame % 4 == 0) {
            Sound_Effect(SOUND_EFFECT_PELLET);
        }
    } else if (m_B20M.status.frame == 50) {
        m_B20M.resume_stage = B20M_RESUME_SHAKE_END;
        return Wait_Scroll(&m_B20M.wait, 0);
    }
    return true;
}
static void M_TopRain(const B20M_ASSETS *const assets)
{
    if (m_B20M.status.frame % 8 == 0) {
        M_Group(
            assets, M_Random(assets) % 640, 64,
            g_Session.settings.difficulty == 3 ? PELLET_GROUP_RANDOM_WIDE
                                               : PELLET_GROUP_SINGLE,
            44);
    }
}
static void M_Radial(const B20M_ASSETS *const assets)
{
    if (m_B20M.status.frame < 5) {
        M_Ray(assets, 0, 0, 0, 0, 99);
    }
    if (m_B20M.status.frame < 150) {
        return;
    }
    if (m_B20M.status.frame == 150) {
        m_B20M.radial_angle = 0;
        m_B20M.radial_velocity = 1;
        m_B20M.pattern_state = Boss_Rank(60, 65, 70, 75);
        Sound_Effect(SOUND_EFFECT_LASER);
    } else if (m_B20M.status.frame < 200) {
        M_Ray(
            assets, 320, 160, Math16_PolarX(320, 140, m_B20M.radial_angle),
            Math16_PolarY(160, 140, m_B20M.radial_angle), 7);
        m_B20M.radial_angle =
            (uint8_t)(m_B20M.radial_angle + m_B20M.radial_velocity);
        m_B20M.radial_velocity++;
        if (m_B20M.status.frame % 10 == 0) {
            Sound_Effect(SOUND_EFFECT_LASER);
        }
    } else if (m_B20M.status.frame == 200) {
        M_Ray(assets, 320, 160, 999, 999, 7);
        m_B20M.radial_angle = 0;
    } else if (m_B20M.status.frame <= 400 && m_B20M.status.frame % 5 == 0) {
        Sound_Effect(SOUND_EFFECT_PELLET);
        if (m_B20M.status.frame % 15 == 0) {
            M_Laser(
                assets, (m_B20M.status.frame - 215) / 15, 320, 160,
                Math16_PolarX(320, 600, m_B20M.radial_angle),
                Math16_PolarY(160, 600, m_B20M.radial_angle),
                m_B20M.pattern_state, 7, 20, 4);
        } else {
            M_Single(assets, 320, 160, m_B20M.radial_angle, 100);
        }
        M_Single(assets, 320, 160, (uint8_t)(m_B20M.radial_angle - 4), 96);
        M_Single(assets, 320, 160, (uint8_t)(m_B20M.radial_angle + 4), 96);
        if (g_Session.settings.difficulty >= 2) {
            M_Single(assets, 320, 160, (uint8_t)(m_B20M.radial_angle - 4), 64);
            M_Single(assets, 320, 160, (uint8_t)(m_B20M.radial_angle + 4), 64);
            if (g_Session.settings.difficulty == 3) {
                M_Single(assets, 320, 160, m_B20M.radial_angle, 64);
                M_Single(assets, 320, 160, m_B20M.radial_angle, 48);
            }
        }
        m_B20M.radial_angle = (uint8_t)(m_B20M.radial_angle + 12);
    } else if (m_B20M.status.frame == 450) {
        m_B20M.status.frame = 0;
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
    B20M_RAYS *const r = &m_B20M.bird_rays;
    if (m_B20M.status.frame < 50) {
        return;
    }
    if (m_B20M.status.frame == 50) {
        m_B20M.pattern_state = Boss_Rank(6, 7, 8, 9);
        r->x = 2;
        r->y = 399;
        r->velocity_x = 2;
        r->velocity_y = -2;
        r->image = 3;
        return;
    }
    if (m_B20M.status.frame < 100) {
        M_RaysDraw(
            assets, r, m_B20M.status.frame % 2 == 0, 320, 120, 8, 2, true);
        if (m_B20M.status.frame % 2 != 0) {
            r->image = M_DebrisNext(r->image);
        }
        r->x = Math16_Wrap(r->x + 2);
        if (m_B20M.status.frame % 10 == 0) {
            Sound_Effect(SOUND_EFFECT_LASER);
            const int16_t target =
                Math16_Wrap((m_B20M.status.frame - 60) * 10 + r->x);
            const MATH16_VECTOR v = Math16_Aim(
                (MATH16_VECTOR) { r->x, Math16_Wrap(r->y - 32) },
                (MATH16_VECTOR) { target, 0 }, 4, 0);
            r->velocity_x = v.x;
            r->velocity_y = v.y;
            M_BirdSpawn(r->x, Math16_Wrap(r->y - 32), v.x, v.y);
            M_BirdSpawn(M_RayX(r, false), Math16_Wrap(r->y - 32), -v.x, v.y);
        }
        if (m_B20M.status.frame % 6 == 0) {
            M_BirdsFire(assets, 2);
        }
        return;
    }
    if (m_B20M.status.frame == 100) {
        Video_LineErase(320, 120, Math16_Wrap(M_RayX(r, false) + 2), r->y);
        return;
    }
    if (m_B20M.status.frame % 10 == 0) {
        M_BirdsFire(assets, 2);
    }
    if (m_B20M.status.frame >= 300) {
        m_B20M.status.frame = 0;
    }
}
static void M_Semicircles(const B20M_ASSETS *const assets)
{
    int32_t count = 0;
    int32_t angle = 0;
    int32_t step = 0;
    int32_t speed = 0;
    if (m_B20M.status.frame == 50) {
        count = 20;
        step = 7;
        speed = 32;
    } else if (m_B20M.status.frame == 60) {
        count = 10;
        angle = 4;
        step = 12;
        speed = 48;
    } else if (m_B20M.status.frame == 70) {
        count = 8;
        angle = 4;
        step = 14;
        speed = 64;
    } else if (m_B20M.status.frame == 80) {
        count = 10;
        angle = 14;
        step = 10;
        speed = 48;
    } else if (m_B20M.status.frame > 150) {
        m_B20M.status.frame = 0;
    }
    for (int32_t i = 0; i < count; i++) {
        M_Single(assets, 320, 160, (uint8_t)angle, (int16_t)speed);
        angle += step;
    }
}
static void M_Stacks(const B20M_ASSETS *const assets)
{
    B20M_RAYS *const r = &m_B20M.stack_rays;
    if (m_B20M.status.frame < 50) {
        return;
    }
    if (m_B20M.status.frame == 50) {
        m_B20M.pattern_state = Boss_Rank(16, 13, 10, 8);
        r->x = 2;
        r->y = 399;
    } else if (m_B20M.status.frame < 130) {
        M_RaysDraw(
            assets, r, m_B20M.status.frame % 2 == 0, 320, 120, 8, 8, true);
        if (m_B20M.status.frame % 2 != 0) {
            r->image = M_DebrisNext(r->image);
        }
        r->x = Math16_Wrap(r->x + 8);
    } else if (m_B20M.status.frame == 130) {
        Video_LineErase(320, 120, Math16_Wrap(M_RayX(r, false) + 8), r->y);
        r->x = 0;
    } else if (m_B20M.status.frame > 150 && m_B20M.status.frame < 350) {
        if (m_B20M.status.frame % 10 == 0) {
            for (int16_t i = 0; i < 64; i += 16) {
                if (g_Player->left + 16 - r->x > 32
                    || g_Player->left + 16 - r->x < -32) {
                    M_Single(
                        assets, r->x, Math16_Wrap(r->y - 8), 192,
                        Math16_Wrap(i + 48));
                }
                const int16_t rx = M_RayX(r, false);
                if (g_Player->left + 16 - rx > 32
                    || g_Player->left + 16 - rx < -32) {
                    M_Single(
                        assets, rx, Math16_Wrap(r->y - 8), 192,
                        Math16_Wrap(i + 48));
                }
            }
            r->x = Math16_Wrap(r->x + 32);
            Sound_Effect(SOUND_EFFECT_PELLET);
        }
    } else if (m_B20M.status.frame == 350) {
        r->x = 0;
        r->y = 64;
    } else if (m_B20M.status.frame < 550) {
        if (m_B20M.status.frame % m_B20M.pattern_state == 0) {
            for (int32_t i = 0; i < 2; i++) {
                uint8_t angle =
                    M_Angle(400 - r->y, (M_Random(assets) % 640) - r->x);
                M_Single(assets, r->x, r->y, angle, 48);
                angle = M_Angle(400 - r->y, (M_Random(assets) % 640) - r->x);
                M_Single(assets, M_RayX(r, false), r->y, angle, 48);
            }
            r->x = Math16_Wrap(r->x + 32);
        }
    } else if (m_B20M.status.frame > 600) {
        m_B20M.status.frame = 0;
    }
}
static void M_Dotted(
    const B20M_ASSETS *const assets, const int16_t frame,
    const int16_t interval, const int16_t step, const int16_t initial,
    const int16_t frames)
{
    const int16_t radius = Math16_Wrap((frame / interval) * step + initial);
    if (frame == 1) {
        m_B20M.dotted_active = true;
    } else {
        if (!m_B20M.dotted_active || frame % interval != 0) {
            return;
        }
        M_EraseEllipse(
            assets, 320, 185, m_B20M.dotted_radius, m_B20M.dotted_radius, 1);
        if (frame >= frames) {
            m_B20M.dotted_active = false;
            return;
        }
        M_Ellipse(assets, 320, 185, radius, radius, 7, 1);
    }
    m_B20M.dotted_radius = radius;
}
static void M_SprayInit(B20M_SPRAY *const s, const B20M_ASSETS *const assets)
{
    s->x = 0;
    s->y = 400;
    s->speed = 112;
    s->subpattern = 0;
    m_B20M.pattern_state = Boss_Rank(32, 24, 18, 16);
}
static void M_SprayFire(
    B20M_SPRAY *const s, const B20M_ASSETS *const assets, const bool right)
{
    s->angle = M_Angle(s->y - 185, s->x - 320);
    M_Single(assets, 320, 185, s->angle, s->speed);
    s->speed = Math16_Wrap(s->speed - 4);
    s->x = Math16_Wrap(
        s->x + (right ? m_B20M.pattern_state : -m_B20M.pattern_state));
}
static void M_SprayOnce(const B20M_ASSETS *const assets)
{
    B20M_SPRAY *const s = &m_B20M.sprays[0];
    if (m_B20M.status.frame < 50) {
        return;
    }
    if (m_B20M.status.frame == 50) {
        M_Dotted(assets, 1, 4, 16, 32, 160);
    }
    M_Dotted(assets, Math16_Wrap(m_B20M.status.frame - 49), 4, 16, 32, 90);
    if (m_B20M.status.frame < 100) {
        return;
    }
    if (m_B20M.status.frame == 100) {
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
            m_B20M.status.frame = 0;
        }
    }
}
static void M_SealRain(const B20M_ASSETS *const assets)
{
    B20M_RAYS *const r = &m_B20M.seal_rays;
    if (m_B20M.status.frame < 50) {
        return;
    }
    if (m_B20M.status.frame == 50) {
        r->x = 0;
        r->y = 399;
        r->image = 3;
        r->previous_image = 3;
        m_B20M.pattern_state = Boss_Rank(30, 35, 40, 45);
    }
    if (r->y > 185) {
        M_RaysDraw(
            assets, r, m_B20M.status.frame % 2 == 0, 320, 185, 32, 0, false);
        if (m_B20M.status.frame % 2 != 0) {
            r->previous_image = r->image;
            r->image = M_DebrisNext(r->image);
            r->y = Math16_Wrap(r->y - 8);
        }
        if (m_B20M.status.frame % 10 == 0) {
            Sound_Effect(SOUND_EFFECT_LASER);
        }
        return;
    }
    Sound_Effect(SOUND_EFFECT_BOSS_HIT);
    Sprite_Restore16(
        M_DebrisX(r, false, 32, 0), Math16_Wrap(r->y - 16), 32, 40);
    Sprite_Restore16(M_DebrisX(r, true, 32, 0), Math16_Wrap(r->y - 16), 32, 50);
    Video_LineErase(320, 185, M_RayX(r, true), Math16_Wrap(r->y + 4));
    Video_LineErase(320, 185, M_RayX(r, false), Math16_Wrap(r->y + 4));
    for (int32_t i = 0; i < m_B20M.pattern_state; i++) {
        M_Rain(
            assets, (int16_t)((640 / m_B20M.pattern_state) * i), 185,
            (uint8_t)M_Random(assets), 16);
    }
    m_B20M.status.frame = 0;
}
static void M_SprayTwice(const B20M_ASSETS *const assets)
{
    B20M_SPRAY *const s = &m_B20M.sprays[1];
    if (m_B20M.status.frame < 80) {
        return;
    }
    if (m_B20M.status.frame == 80) {
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
                m_B20M.status.frame = 0;
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
    B20M_LEAVES *const p = &m_B20M.leaves;
    if (interval == 999) {
        for (int32_t i = 0; i < 30; i++) {
            p->flag[i] = 0;
        }
        return;
    }
    if (m_B20M.status.frame % interval == 0) {
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
    if (m_B20M.status.frame % 2 != 0) {
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
            if (m_B20M.status.frame % 4 == 0) {
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
                && p->left[i] / 10 > g_Player->left
                && p->left[i] / 10 < g_Player->left + 24
                && !g_Player->invincible) {
                Player_Hit();
            }
        }
    }
}
static void M_Hud(const B20M_ASSETS *const assets)
{
    Stage_SetFirstStage(true);
    StageTimer_Redraw(true, g_Stage->timer_initialized, 0);
    Hud_ScoreAndCombo();
}
static void M_BeginEntrance(const void *const data)
{
    const B20M_ASSETS *const assets = data;
    ASSERT(
        !m_B20M.entrance_active && m_B20M.sequence == B20M_SEQUENCE_NONE
        && Wait_Idle(&m_B20M.wait));
    Pellets_Interlace();
    Tram_Fill((GFX_RECT) { 0, 0, 640, 400 }, TRAM_BLACK);
    M_Pi(assets, &assets->lower, true);
    Video_SelectDrawingPage(1);
    M_Pi(assets, &assets->upper, true);
    Video_SelectDrawingPage(0);
    StageObjects_Init(nullptr, 19, false, nullptr, nullptr);
    Sound_Load(13);
    Sound_Play();
    Tram_Clear();
    m_B20M.scroll_line = 399;
    m_B20M.sequence_stage = B20M_ENTRANCE_SCROLL;
    m_B20M.entrance_active = true;
}
static bool M_ResumeEntrance(
    const void *const data, const BOSS_CONTEXT *const ctx)
{
    const B20M_ASSETS *const assets = data;
    const uint64_t tick = ctx->host_tick;
    if (!m_B20M.entrance_active) {
        return true;
    }
    if (!Wait_Ready(&m_B20M.wait, tick)) {
        return false;
    }
    for (;;) {
        switch (m_B20M.sequence_stage) {
        case B20M_ENTRANCE_SCROLL:
            if (m_B20M.scroll_line < 0) {
                m_B20M.sequence_stage = B20M_ENTRANCE_DIAGONAL;
                return Wait_Scroll(&m_B20M.wait, 0);
            }
            m_B20M.sequence_stage = B20M_ENTRANCE_SCROLL_STEP;
            return Wait_Scroll(&m_B20M.wait, m_B20M.scroll_line);
        case B20M_ENTRANCE_SCROLL_STEP:
            M_CopyRows(assets, m_B20M.scroll_line, 1);
            m_B20M.scroll_line--;
            m_B20M.sequence_stage = B20M_ENTRANCE_SCROLL;
            if (!Wait_Delay(&m_B20M.wait, 1)) {
                return false;
            }
            continue;
        case B20M_ENTRANCE_DIAGONAL:
            Video_SelectDrawingPage(1);
            M_Pi(assets, &assets->backgrounds[0], true);
            Video_SelectDrawingPage(0);
            m_B20M.sequence_stage = B20M_ENTRANCE_DIAGONAL_STEP;
            if (!Palette_BeginGlobalDiagonalTransition(
                    M_TransitionTarget(false), assets->interleave, 40, tick)) {
                return false;
            }
            continue;
        case B20M_ENTRANCE_DIAGONAL_STEP:
            if (!Palette_AdvanceGlobalDiagonalTransition(
                    M_TransitionTarget(false), assets->interleave, tick)) {
                return false;
            }
            Rng_Seed(g_Session.frame_rand);
            m_B20M.entrance_active = false;
            Palette_SetStage(&m_B20M.stage_palette);
            m_B20M.sequence_stage = B20M_ENTRANCE_DONE;
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
    if (!Wait_Ready(&m_B20M.wait, tick)) {
        return false;
    }
    for (;;) {
        m_B20M.status.frame = Math16_Wrap(m_B20M.status.frame + 1);
        bool finish = false;
        if (m_B20M.status.frame == 1) {
            m_B20M.entrance_radius = 16;
        } else if (m_B20M.status.frame == 2) {
            M_InvertRings(assets, m_B20M.entrance_radius);
        } else if (m_B20M.status.frame > 2 && m_B20M.status.frame % 4 == 0) {
            M_InvertRings(assets, m_B20M.entrance_radius);
            m_B20M.entrance_radius = Math16_Wrap(m_B20M.entrance_radius + 16);
            finish = M_InvertRings(assets, m_B20M.entrance_radius) >= 128;
        }
        if (finish) {
            m_B20M.status.phase = B20M_PHASE_BIRDS;
            m_B20M.pattern = 0;
            m_B20M.patterns_done = 0;
            m_B20M.patterns_until_next = 1 + M_Random(assets) % 6;
            m_B20M.status.frame = 0;
            m_B20M.hp_bar_filled = false;
            M_Palette(assets, &m_B20M.boss_palette);
            m_B20M.shield.left = 304;
            m_B20M.shield.top = 144;
            M_Snap(assets);
            M_WandRaise(assets, true);
            M_BirdsReset();
            m_B20M.sequence = B20M_SEQUENCE_NONE;
            return true;
        }
        if (m_B20M.status.frame % 2 != 0 && !Wait_Delay(&m_B20M.wait, 1)) {
            return false;
        }
    }
}
static void M_BeginTransform(const B20M_ASSETS *const assets)
{
    m_B20M.status.frame = 0;
    m_B20M.hit.frame = 399;
    Shots_EraseAndReset();
    Pellets_EraseAndResetMoving();
    ShootoutLasers_EraseAndReset(10);
    Orb_Relaunch();
    Player_Center();
    Player_StopActions();
    Player_Reset(g_Session.settings.difficulty);
    m_B20M.hit.invincible = false;
    Rng_Seed(g_Session.frame_rand);
    Sound_Fade();
    Video_SelectDrawingPage(1);
    // The absent BOSS6.GRP leaves the previous image and palette intact.
    M_Color(assets, 15, 0, 0, 0);
    m_B20M.boss_palette = g_Palette->saved;
    M_CopyOther(assets);
    Video_SelectDrawingPage(1);
    M_Pi(assets, &assets->backgrounds[4], false);
    Video_SelectDrawingPage(0);
    m_B20M.sequence = B20M_SEQUENCE_TRANSFORM;
    m_B20M.sequence_stage = B20M_TRANSFORM_SHAKE;
}
static bool M_Transform(const B20M_ASSETS *const assets, const uint64_t tick)
{
    if (!Wait_Ready(&m_B20M.wait, tick)) {
        return false;
    }
    for (;;) {
        switch (m_B20M.sequence_stage) {
        case B20M_TRANSFORM_SHAKE:
            m_B20M.status.frame = Math16_Wrap(m_B20M.status.frame + 1);
            if (m_B20M.status.frame < 200) {
                if (m_B20M.status.frame % 2 == 0) {
                    M_Color(assets, 15, 15, 15, 15);
                } else {
                    M_Palette(assets, &m_B20M.boss_palette);
                }
                m_B20M.sequence_stage = B20M_TRANSFORM_SHAKE_STEP;
                return Wait_Scroll(
                    &m_B20M.wait, m_B20M.status.frame % 2 == 0 ? 416 : 386);
            }
            if (m_B20M.status.frame == 200) {
                M_Palette(assets, &m_B20M.boss_palette);
                m_B20M.sequence_stage = B20M_TRANSFORM_SHAKE_STEP;
                return Wait_Scroll(&m_B20M.wait, 0);
            }
            for (int32_t i = 0; i < 3; i++) {
                Palette_SetGlobalResourceComponent(15, i, 0);
            }
            ASSERT(Palette_BeginGlobalDiagonalTransition(
                M_TransitionTarget(true), assets->interleave, 0, tick));
            Video_SelectDrawingPage(1);
            Sound_Load(14);
            Sound_Play();
            M_Pi(assets, &assets->backgrounds[5], true);
            M_Color(assets, 6, 0, 0, 0);
            M_CopyOther(assets);
            M_Hud(assets);
            m_B20M.sequence_stage = B20M_TRANSFORM_SECOND_FORM;
            return Wait_Scroll(&m_B20M.wait, 0);
        case B20M_TRANSFORM_SHAKE_STEP:
            if (m_B20M.status.frame < 200 && m_B20M.status.frame % 4 == 0) {
                Sound_Effect(SOUND_EFFECT_SHAKE);
            }
            m_B20M.sequence_stage = B20M_TRANSFORM_SHAKE;
            if (!Wait_Delay(&m_B20M.wait, 1)) {
                return false;
            }
            continue;
        case B20M_TRANSFORM_SECOND_FORM:
            m_B20M.status.frame = 0;
            m_B20M.hit.frame = 0;
            m_B20M.pattern = 0;
            m_B20M.patterns_done = 0;
            m_B20M.status.phase = B20M_PHASE_SECOND_FORM;
            Player_EndInvincibility();
            m_B20M.boss_palette = g_Palette->saved;
            m_B20M.stage_palette = m_B20M.boss_palette;
            m_B20M.status.hp = 6;
            HudHp_Begin(3, 10);
            m_B20M.hp_bar_filled = false;
            M_Leaves(assets, 999);
            m_B20M.sequence = B20M_SEQUENCE_NONE;
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
    m_B20M.boss_palette = g_Palette->saved;
    M_CopyOther(assets);
    Video_SelectDrawingPage(0);
    m_B20M.sequence = B20M_SEQUENCE_DEFEAT;
    m_B20M.sequence_stage = B20M_DEFEAT_SHAKE;
}
static bool M_Defeat(const B20M_ASSETS *const assets, const uint64_t tick)
{
    if (!Wait_Ready(&m_B20M.wait, tick)) {
        return false;
    }
    for (;;) {
        switch (m_B20M.sequence_stage) {
        case B20M_DEFEAT_SHAKE:
            m_B20M.status.frame = Math16_Wrap(m_B20M.status.frame + 1);
            if (m_B20M.status.frame < 230) {
                m_B20M.sequence_stage = B20M_DEFEAT_SHAKE_STEP;
                return Wait_Scroll(
                    &m_B20M.wait, m_B20M.status.frame % 2 == 0 ? 416 : 386);
            }
            m_B20M.sequence_stage = B20M_DEFEAT_SHAKE_STEP;
            continue;
        case B20M_DEFEAT_SHAKE_STEP:
            if (m_B20M.status.frame < 200 && m_B20M.status.frame % 4 == 0) {
                Sound_Effect(SOUND_EFFECT_SHAKE);
            }
            if (m_B20M.status.frame == 170) {
                Sound_Fade();
            }
            if (m_B20M.status.frame > 190) {
                if (m_B20M.status.frame % 2 == 0) {
                    for (int32_t i = 0; i < 16; i++) {
                        for (int32_t j = 0; j < 3; j++) {
                            Palette_SetGlobalSavedComponent(
                                i, j, g_Palette->saved.colors[i][j] - 1);
                        }
                    }
                    M_Palette(assets, &g_Palette->saved);
                }
                if (m_B20M.status.frame > 230) {
                    m_B20M.sequence_stage = B20M_DEFEAT_BONUS;
                    return Wait_Scroll(&m_B20M.wait, 0);
                }
            }
            m_B20M.sequence_stage = B20M_DEFEAT_SHAKE;
            if (!Wait_Delay(&m_B20M.wait, 2)) {
                return false;
            }
            continue;
        case B20M_DEFEAT_BONUS:
            for (int32_t i = 0; i < 5; i++) {
                Stage_AddScore(10000);
            }
            Stage_SetGameCleared();
            m_B20M.finished = true;
            m_B20M.sequence = B20M_SEQUENCE_NONE;
            return true;
        default:
            ASSERT(false);
            return false;
        }
    }
}
static void M_Hit(const B20M_ASSETS *const assets)
{
    const ORB *const orb = g_Orb;
    const bool collision = (orb->left >= 314 && orb->left <= 346
                            && orb->top >= 104 && orb->top <= 136)
        || BossEntity_HitTestOrb(&m_B20M.shield);
    const uint8_t colors[] = { 3, 4, 5 };
    BossHit_UpdateAndRender(
        &m_B20M.hit, &m_B20M.status.hp, &m_B20M.boss_palette, colors, 3, 10000,
        collision, (BOSS_SHOT_HITBOX) { 314, 104, 64, 32 });
}
static void M_PhaseNext(const B20M_ASSETS *const assets, const int8_t phase)
{
    m_B20M.status.phase = phase;
    m_B20M.status.frame = 0;
    M_Snap(assets);
}
static void M_PatternNext(const int16_t first, const int16_t last)
{
    m_B20M.pattern = last == 1   ? Math16_Wrap(1 - m_B20M.pattern)
        : m_B20M.pattern == last ? first
                                 : Math16_Wrap(m_B20M.pattern + 1);
    m_B20M.patterns_done = Math16_Wrap(m_B20M.patterns_done + 1);
}
static void M_Pulse(const B20M_ASSETS *const assets)
{
    if (m_B20M.status.frame % 10 != 0) {
        return;
    }
    M_Palette(assets, &m_B20M.boss_palette);
    const int8_t *const color = g_Palette->saved.colors[6];
    if (m_B20M.patterns_done == 0) {
        if (color[0] < 10) {
            Palette_SetGlobalSavedComponent(6, 0, color[0] + 1);
        } else {
            m_B20M.patterns_done = 1;
        }
        if (color[1] < 10) {
            Palette_SetGlobalSavedComponent(6, 1, color[1] + 1);
        }
        if (color[2] < 6) {
            Palette_SetGlobalSavedComponent(6, 2, color[2] + 1);
        }
    } else {
        if (color[0] > 0) {
            Palette_SetGlobalSavedComponent(6, 0, color[0] - 1);
        } else {
            m_B20M.patterns_done = 0;
        }
        if (color[1] > 0) {
            Palette_SetGlobalSavedComponent(6, 1, color[1] - 1);
        }
        if (color[2] > 0) {
            Palette_SetGlobalSavedComponent(6, 2, color[2] - 1);
        }
    }
    M_Palette(assets, &g_Palette->saved);
    m_B20M.boss_palette = g_Palette->saved;
}
static bool M_Pattern(const B20M_ASSETS *const assets)
{
    switch (m_B20M.call_phase) {
    case B20M_PHASE_BIRDS:
        if (m_B20M.pattern == 0) {
            M_PurpleLasers(assets);
        } else if (m_B20M.pattern == 1) {
            M_Vortices(assets);
        } else if (m_B20M.pattern == 2) {
            M_Eggs(assets);
        }
        break;
    case B20M_PHASE_VERTICAL:
        if (m_B20M.pattern == 0) {
            M_Flake(assets);
            M_Rings(assets);
        } else if (m_B20M.pattern == 1) {
            M_Sling(assets);
            M_Rings(assets);
        } else if (m_B20M.status.frame < 60) {
            m_B20M.status.frame = 0;
        }
        break;
    case B20M_PHASE_WAVY:
        if (m_B20M.pattern == 0) {
            M_FourLasers(assets);
        } else if (m_B20M.pattern == 1 || m_B20M.pattern == 4) {
            return M_Shake(assets);
        } else if (m_B20M.pattern == 2 || m_B20M.pattern == 5) {
            M_FourLasers(assets);
            M_TopRain(assets);
        } else if (m_B20M.pattern == 3) {
            M_Radial(assets);
        }
        break;
    case B20M_PHASE_VERTICAL_BIRDS:
        if (m_B20M.pattern == 0) {
            M_SymmetricBirds(assets);
        } else if (m_B20M.pattern == 1) {
            M_Semicircles(assets);
        } else if (m_B20M.pattern == 2) {
            M_Stacks(assets);
        }
        break;
    case B20M_PHASE_SECOND_FORM:
        switch (m_B20M.pattern) {
        case 0:
            m_B20M.pattern_state = Boss_Rank(56, 32, 24, 20);
            if (m_B20M.status.frame > 100) {
                m_B20M.status.frame = 0;
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
            M_Leaves(assets, m_B20M.pattern_state);
            if (m_B20M.status.frame > 300) {
                m_B20M.status.frame = 0;
            }
            break;
        case 5:
            M_Leaves(assets, 500);
            if (m_B20M.status.frame > 200) {
                m_B20M.status.frame = 0;
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
    const int8_t phase = m_B20M.call_phase;
    if (m_B20M.status.frame == 0) {
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
        if (m_B20M.status.hp <= 0) {
            Sound_Stop();
            m_B20M.status.phase = B20M_PHASE_DEFEAT;
            m_B20M.status.frame = 0;
            m_B20M.hit.frame = 0;
        }
    } else {
        if (m_B20M.status.hp <= 0) {
            m_B20M.status.phase = B20M_PHASE_TRANSFORM;
        }
        if (m_B20M.patterns_done >= m_B20M.patterns_until_next
            && !m_B20M.hit.invincible) {
            m_B20M.patterns_done = 0;
            m_B20M.status.phase = phase + 1;
            m_B20M.pattern = 0;
            m_B20M.status.frame = 0;
        }
    }
    return true;
}
static void M_AfterGust(const B20M_ASSETS *const assets)
{
    const int8_t phase = m_B20M.call_phase;
    M_Dress(assets);
    if (m_B20M.status.frame != 0) {
        return;
    }
    M_PhaseNext(
        assets, phase == B20M_PHASE_GUST_4 ? B20M_PHASE_BIRDS : phase + 1);
    if (phase != B20M_PHASE_GUST_1) {
        m_B20M.boss_palette = g_Palette->saved;
    }
    m_B20M.patterns_until_next = phase == B20M_PHASE_GUST_1
        ? 1 + M_Random(assets) % 5
        : phase == B20M_PHASE_GUST_2 ? 3 + M_Random(assets) % 4
        : phase == B20M_PHASE_GUST_3 ? 2 + M_Random(assets) % 5
                                     : 1 + M_Random(assets) % 6;
}
static bool M_Update(const void *const data, const BOSS_CONTEXT *const ctx)
{
    const B20M_ASSETS *const assets = data;
    const uint64_t tick = ctx->host_tick;
    if (m_B20M.finished) {
        return true;
    }
    if (m_B20M.sequence == B20M_SEQUENCE_RINGS) {
        return M_RingEntrance(assets, tick);
    }
    if (m_B20M.sequence == B20M_SEQUENCE_TRANSFORM) {
        return M_Transform(assets, tick);
    }
    if (m_B20M.sequence == B20M_SEQUENCE_DEFEAT) {
        return M_Defeat(assets, tick);
    }
    if (m_B20M.resume_stage != B20M_RESUME_NONE) {
        if (!Wait_Ready(&m_B20M.wait, tick)) {
            return false;
        }
        const uint8_t resume = m_B20M.resume_stage;
        m_B20M.resume_stage = B20M_RESUME_NONE;
        if (resume == B20M_RESUME_SHAKE) {
            Sound_Effect(SOUND_EFFECT_PELLET);
            return M_Tail(assets);
        }
        if (resume == B20M_RESUME_SHAKE_END) {
            m_B20M.status.frame = 0;
            return M_Tail(assets);
        }
        if (resume == B20M_RESUME_GUST_END) {
            const int16_t image = m_B20M.call_phase == B20M_PHASE_GUST_1 ? 1
                : m_B20M.call_phase == B20M_PHASE_GUST_2                 ? 2
                : m_B20M.call_phase == B20M_PHASE_GUST_3                 ? 3
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
    if (m_B20M.status.phase == B20M_PHASE_START) {
        m_B20M.status.frame = -1;
        m_B20M.hit.frame = 0;
        m_B20M.hit.invincible = false;
        m_B20M.boss_palette = g_Palette->saved;
        m_B20M.stage_palette = m_B20M.boss_palette;
        Rng_Seed(g_Session.frame_rand);
        m_B20M.sequence = B20M_SEQUENCE_RINGS;
        return M_RingEntrance(assets, tick);
    }
    if (m_B20M.status.phase == B20M_PHASE_TRANSFORM) {
        M_BeginTransform(assets);
        return M_Transform(assets, tick);
    }
    if (m_B20M.status.phase == B20M_PHASE_DEFEAT) {
        M_BeginDefeat(assets);
        return M_Defeat(assets, tick);
    }
    m_B20M.call_phase = m_B20M.status.phase;
    if (m_B20M.status.phase >= B20M_PHASE_GUST_1
        && m_B20M.status.phase <= B20M_PHASE_GUST_4
        && m_B20M.status.phase % 2 == 0) {
        m_B20M.status.frame = Math16_Wrap(m_B20M.status.frame + 1);
        if (!M_Gust(assets)) {
            return false;
        }
        M_AfterGust(assets);
        return true;
    }
    if (m_B20M.status.phase != B20M_PHASE_BIRDS
        && m_B20M.status.phase != B20M_PHASE_VERTICAL
        && m_B20M.status.phase != B20M_PHASE_WAVY
        && m_B20M.status.phase != B20M_PHASE_VERTICAL_BIRDS
        && m_B20M.status.phase != B20M_PHASE_SECOND_FORM) {
        return true;
    }
    if ((m_B20M.status.phase == B20M_PHASE_BIRDS
         || m_B20M.status.phase == B20M_PHASE_SECOND_FORM)
        && !m_B20M.hp_bar_filled) {
        m_B20M.hp_bar_filled =
            HudHp_Increment(m_B20M.status.hp, m_B20M.status.frame);
    }
    m_B20M.status.frame = Math16_Wrap(m_B20M.status.frame + 1);
    m_B20M.hit.frame = Math16_Wrap(m_B20M.hit.frame + 1);
    if (m_B20M.status.phase != B20M_PHASE_SECOND_FORM) {
        M_Shield(assets);
        M_Dress(assets);
    }
    if (m_B20M.status.phase == B20M_PHASE_VERTICAL
        || m_B20M.status.phase == B20M_PHASE_VERTICAL_BIRDS) {
        M_Vertical(assets, m_B20M.status.phase == B20M_PHASE_VERTICAL_BIRDS);
    } else if (m_B20M.status.phase == B20M_PHASE_WAVY) {
        M_Wavy(assets);
    } else if (m_B20M.status.phase == B20M_PHASE_SECOND_FORM) {
        M_B20mHorizontal(assets);
    }
    if ((m_B20M.status.phase == B20M_PHASE_BIRDS
         || m_B20M.status.phase == B20M_PHASE_VERTICAL_BIRDS)
        && !M_BirdsUpdate(assets, tick)) {
        return false;
    }
    if (!M_Pattern(assets)) {
        return false;
    }
    return M_Tail(assets);
}

const BOSS_STRATEGY g_BossSariel = {
    .assets_size = sizeof(B20M_ASSETS),
    .load_assets = M_LoadAssets,
    .init = M_Init,
    .setup = M_Setup,
    .begin_entrance = M_BeginEntrance,
    .resume_entrance = M_ResumeEntrance,
    .update = M_Update,
    .song = -1,
};
