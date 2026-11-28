#include "th01/boss/private.h"
#include "th01/gameplay.h"
#include "th01/grz.h"
#include "th01/pellets.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "th01/shootout_lasers.h"
#include "th01/sound.h"
#include "th01/sprite.h"
#include "th01/stage/objects.h"
#include "th01/tram.h"
#include "th01/wait.h"

#include "core/base/assert.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/pi.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

typedef struct {
    BOS head;
    BOS face;
    BOS aim;
    GRZ body;
    PI_IMAGE scroll;
    PI_IMAGE upper;
    PI_IMAGE layers[4];
    const uint8_t *diamond;
    const uint8_t *flake;
} B20J_ASSETS;

typedef struct {
    int16_t left[4][5];
    int16_t top[4][5];
    MATH16_VECTOR velocity[4];
    int16_t target_left[4];
    bool target_locked[4];
} B20J_SNAKES;

typedef enum {
    B20J_PHASE_START,
    B20J_PHASE_PATTERNS_1,
    B20J_PHASE_FLASH_1,
    B20J_PHASE_PATTERNS_2,
    B20J_PHASE_FLASH_2,
    B20J_PHASE_PATTERNS_3,
    B20J_PHASE_FLASH_3,
    B20J_PHASE_FINAL,
} B20J_PHASE;

typedef enum {
    B20J_ENTRANCE_SCROLL = 1,
    B20J_ENTRANCE_SCROLL_STEP,
    B20J_ENTRANCE_SCROLL_END,
    B20J_ENTRANCE_SHAKE_DELAY,
    B20J_ENTRANCE_SHAKE,
    B20J_ENTRANCE_SHAKE_STEP,
    B20J_ENTRANCE_PAN,
    B20J_ENTRANCE_PAN_STEP,
    B20J_ENTRANCE_TREMBLE,
    B20J_ENTRANCE_TREMBLE_STEP,
    B20J_ENTRANCE_FLASH_DELAY,
    B20J_ENTRANCE_FLASH,
    B20J_ENTRANCE_FADE_IN,
} B20J_ENTRANCE;

typedef enum {
    B20J_DEFEAT_KUJI = 1,
    B20J_DEFEAT_SHAKE,
    B20J_DEFEAT_SHAKE_STEP,
    B20J_DEFEAT_SCROLL,
    B20J_DEFEAT_SCROLL_STEP,
    B20J_DEFEAT_CLEAR,
} B20J_DEFEAT;

typedef struct {
    BOSS_ENTITY head;
    BOSS_ENTITY face;
    BOSS_ENTITY aim;
    BOSS_HIT hit;
    PALETTE boss_palette;
    PALETTE stage_palette;
    B20J_SNAKES two_snakes;
    B20J_SNAKES four_snakes;
    MATH16_VECTOR diamond_velocity[2];
    int16_t diamond_left[4];
    int16_t diamond_top[4];
    int16_t diamond_edges_frame;
    MATH16_VECTOR row_velocity;
    int16_t row_left;
    int16_t row_top;
    int16_t row_direction;
    int16_t row_speed;
    int16_t spray_delta;
    int16_t spray_fired;
    int16_t ray_x;
    int16_t ray_y;
    int16_t ray_previous_x;
    int16_t ray_previous_y;
    int16_t slash_left[3];
    int16_t slash_top[3];
    BOSS_STATE status;
    int16_t pattern_state;
    int16_t pattern;
    int16_t pattern_previous;
    int16_t patterns_done;
    int16_t face_direction;
    int16_t face_expression;
    int16_t scroll_line;
    int16_t scroll_speed;
    int16_t scroll_quarter;
    int16_t scroll_frame;
    int16_t animation_frame;
    WAIT wait;
    uint8_t cup_angle;
    uint8_t spray_offset;
    uint8_t spray_angle;
    uint8_t entrance_stage;
    uint8_t defeat_stage;
    bool direction_can_change;
    bool laser_right_to_left;
    bool hp_bar_filled;
    bool entrance_active;
    bool defeating;
    bool finished;
} B20J;

static B20J m_B20J;

static RESULT M_LoadBody(
    VFS *const vfs, const ALLOCATOR *const a, GRZ *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS8.GRZ", VFS_IN_MEMORY, &reader));
    const RESULT result = Grz_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}
static RESULT M_LoadMasks(
    VFS *const vfs, const ALLOCATOR *const a, B20J_ASSETS *const out)
{
    uint8_t *shapes;
    size_t size;
    MUST(SpriteSheet_Load(vfs, "shape8x8.bmp", 8, 8, a, &shapes, &size));
    FAIL_IF(size != 7 * 8, "Invalid shape8x8.bmp size");
    out->diamond = shapes;
    out->flake = shapes + 2 * 8;
    return OK;
}
static RESULT M_ValidateAssets(const B20J_ASSETS *const a)
{
    FAIL_IF(a == nullptr, "Missing Konngara assets");
    const BOS *const banks[] = { &a->head, &a->face, &a->aim };
    const int32_t counts[] = { 3, 6, 3 };
    const int32_t heights[] = { 80, 32, 32 };
    for (int32_t i = 0; i < 3; i++) {
        FAIL_IF(
            banks[i]->pixels == nullptr || banks[i]->coverage == nullptr
                || banks[i]->count != counts[i] || banks[i]->width != 80
                || banks[i]->height != heights[i],
            "Invalid Konngara sprite bank");
    }
    FAIL_IF(a->body.count != 16, "Missing Konngara body images");
    for (int32_t i = 0; i < 16; i++) {
        const GRX *const image = &a->body.images[i];
        FAIL_IF(
            image->stream_count < 1 || image->pixels == nullptr
                || image->coverage == nullptr,
            "Invalid Konngara body image");
    }
    const PI_IMAGE *const images[] = { &a->scroll,    &a->upper,
                                       &a->layers[0], &a->layers[1],
                                       &a->layers[2], &a->layers[3] };
    for (int32_t i = 0; i < 6; i++) {
        FAIL_IF(
            images[i]->pixels == nullptr || images[i]->width <= 0
                || images[i]->height <= 0 || !images[i]->has_palette,
            "Invalid Konngara background");
    }
    FAIL_IF(
        a->diamond == nullptr || a->flake == nullptr,
        "Missing Konngara shape masks");
    return OK;
}
static RESULT M_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const a, void *const data)
{
    B20J_ASSETS *const out = data;
    *out = (B20J_ASSETS) {};
    B20J_ASSETS loaded = {};
    MUST(Boss_LoadBos(vfs, "BOSS8_1.BOS", a, &loaded.head));
    MUST(Boss_LoadBos(vfs, "BOSS8_E1.BOS", a, &loaded.face));
    MUST(Boss_LoadBos(vfs, "BOSS8_E2.BOS", a, &loaded.aim));
    MUST(M_LoadBody(vfs, a, &loaded.body));
    MUST(Boss_LoadBackground(vfs, "BOSS7_D1.GRP", a, &loaded.scroll));
    MUST(Boss_LoadBackground(vfs, "BOSS8_A1.GRP", a, &loaded.upper));
    const char *const names[] = { "BOSS8_D1.GRP", "BOSS8_D2.GRP",
                                  "BOSS8_D3.GRP", "BOSS8_D4.GRP" };
    for (int32_t i = 0; i < 4; i++) {
        MUST(Boss_LoadBackground(vfs, names[i], a, &loaded.layers[i]));
    }
    MUST(M_LoadMasks(vfs, a, &loaded));
    MUST(M_ValidateAssets(&loaded));
    *out = loaded;
    return OK;
}
static void M_Init(void)
{
    memset(&m_B20J, 0, sizeof(m_B20J));
    m_B20J.ray_previous_x = 999;
    m_B20J.ray_previous_y = 999;
}
static void M_Setup(const void *const data, const BOSS_CONTEXT *const ctx)
{
    const B20J_ASSETS *const assets = data;
    Boss_Select(&m_B20J.status);
    ASSERT(
        !m_B20J.entrance_active && !m_B20J.defeating
        && Wait_Idle(&m_B20J.wait));
    ASSERT(Result_Ignore(M_ValidateAssets(assets)));
    m_B20J.boss_palette = g_Palette.saved;
    m_B20J.status.hp = 18;
    m_B20J.status.phase = B20J_PHASE_START;
    m_B20J.status.frame = 0;
    m_B20J.direction_can_change = true;
    m_B20J.face_expression = 0;
    m_B20J.face_direction = 2;
    m_B20J.finished = false;
    HudHp_Begin(10, 16);
}

static uint16_t M_Random(const B20J_ASSETS *const assets)
{
    return MasterRng_Next(&g_Rng);
}
static void M_Pi(
    const B20J_ASSETS *const assets, const PI_IMAGE *const image,
    const bool colorkey)
{
    const VIDEO_IMAGE view = { .pixels = image->pixels,
                               .width = image->width,
                               .height = image->height,
                               .stride = image->width,
                               .key = colorkey ? 15 : -1 };
    Video_Blit(&view, (GFX_RECT) { 0, 0, image->width, image->height }, 0, 0);
    Palette_Import(&g_Palette.resource, image->palette);
    if (!colorkey) {
        Palette_Set(&g_Palette, &g_Palette.resource);
    }
}
static void M_Grx(const B20J_ASSETS *const assets, const int16_t image)
{
    const GRX *const grx = &assets->body.images[image];
    const VIDEO_IMAGE view = { .pixels = grx->pixels,
                               .coverage = grx->coverage,
                               .width = 640,
                               .height = 400,
                               .stride = 640,
                               .coverage_stride = 640,
                               .key = -1 };
    Video_Blit(&view, (GFX_RECT) { 0, 0, 640, 400 }, 0, 0);
}
static void M_Head(const B20J_ASSETS *const assets, const int16_t direction)
{
    BossEntity_Put(&m_B20J.head, 280, 80, direction);
}
static void M_Face(
    const B20J_ASSETS *const assets, const int16_t expression,
    const int16_t direction)
{
    if (expression == 3) {
        BossEntity_Put(&m_B20J.aim, 280, 128, direction);
    } else if (expression != 0) {
        BossEntity_Put(
            &m_B20J.face, 280, 128, (expression - 1) * 3 + direction);
    }
}
static void M_Direction(
    const B20J_ASSETS *const assets, const int16_t direction)
{
    if (!m_B20J.direction_can_change || m_B20J.face_direction == direction) {
        return;
    }
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        M_Head(assets, direction);
    }
    if (m_B20J.face_expression != 0) {
        for (int32_t page = 1; page >= 0; page--) {
            Video_SelectDrawingPage(page);
            M_Face(assets, m_B20J.face_expression, direction);
        }
    }
    m_B20J.face_direction = direction;
}
static void M_Expression(
    const B20J_ASSETS *const assets, const int16_t expression)
{
    if (m_B20J.face_expression == expression) {
        return;
    }
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        if (expression == 0) {
            M_Head(assets, m_B20J.face_direction);
        } else {
            M_Face(assets, expression, m_B20J.face_direction);
        }
    }
    m_B20J.face_expression = expression;
}
static void M_SlashPut(const B20J_ASSETS *const assets, const int16_t image)
{
    Video_SelectDrawingPage(1);
    M_Grx(assets, image);
    Video_SelectDrawingPage(0);
    M_Grx(assets, image);
}
static void M_White(const B20J_ASSETS *const assets)
{
    Palette_SetColor(&g_Palette, 9, 15, 15, 15);
}
static void M_WhiteIn(const B20J_ASSETS *const assets)
{
    int8_t *const col = g_Palette.saved.colors[9];
    if (col[0] > 0) {
        col[0]--;
    }
    if (col[1] > 9) {
        col[1]--;
    }
    if (col[2] > 10) {
        col[2]--;
    }
    Palette_Set(&g_Palette, &g_Palette.saved);
}
static void M_BlackOut(const B20J_ASSETS *const assets)
{
    PALETTE *const colors = &g_Palette.saved;
    for (int32_t i = 0; i < 16; i++) {
        for (int32_t j = 0; j < 3; j++) {
            if (colors->colors[i][j] > 0) {
                colors->colors[i][j]--;
            }
        }
    }
    Palette_Set(&g_Palette, colors);
}
static void M_BeginEntrance(const void *const data)
{
    const B20J_ASSETS *const assets = data;
    ASSERT(
        !m_B20J.entrance_active && !m_B20J.defeating
        && Wait_Idle(&m_B20J.wait));
    Pellets_Interlace();
    Tram_Fill((GFX_RECT) { 0, 0, 640, 400 }, TRAM_BLACK);
    M_Pi(assets, &assets->scroll, false);
    m_B20J.stage_palette = g_Palette.saved;
    StageObjects_Init(nullptr, 19, false, nullptr, nullptr);
    Video_SelectDrawingPage(1);
    M_Pi(assets, &assets->upper, false);
    Video_SelectDrawingPage(0);
    Sound_Load(12);
    ASSERT(Result_Ignore(BossEntity_Bind(&m_B20J.head, 0, &assets->head)));
    ASSERT(Result_Ignore(BossEntity_Bind(&m_B20J.face, 1, &assets->face)));
    ASSERT(Result_Ignore(BossEntity_Bind(&m_B20J.aim, 2, &assets->aim)));
    Sound_Play();
    Palette_SetGray(&g_Palette, 0);
    Tram_Clear();
    m_B20J.scroll_line = 0;
    m_B20J.scroll_speed = 32;
    m_B20J.scroll_quarter = 0;
    m_B20J.scroll_frame = 0;
    m_B20J.entrance_stage = B20J_ENTRANCE_SCROLL;
    m_B20J.entrance_active = true;
}
static bool M_ResumeEntrance(
    const void *const data, const BOSS_CONTEXT *const ctx)
{
    const B20J_ASSETS *const assets = data;
    if (!m_B20J.entrance_active) {
        return true;
    }
    if (!Wait_Ready(&m_B20J.wait, 0)) {
        return false;
    }
    for (;;) {
        switch (m_B20J.entrance_stage) {
        case B20J_ENTRANCE_SCROLL:
            m_B20J.entrance_stage = B20J_ENTRANCE_SCROLL_STEP;
            return Wait_Scroll(&m_B20J.wait, m_B20J.scroll_line);
        case B20J_ENTRANCE_SCROLL_STEP:
            m_B20J.scroll_line += m_B20J.scroll_speed;
            for (int32_t quarter = 0; quarter < 4; quarter++) {
                if (m_B20J.scroll_quarter == quarter
                    && m_B20J.scroll_line > (quarter + 1) * 100) {
                    m_B20J.scroll_quarter++;
                    m_B20J.scroll_speed--;
                    if (quarter == 3) {
                        m_B20J.scroll_quarter = 0;
                        m_B20J.scroll_line -= 400;
                    }
                }
            }
            if (m_B20J.scroll_speed <= 0) {
                m_B20J.entrance_stage = B20J_ENTRANCE_SCROLL_END;
                continue;
            }
            if (m_B20J.scroll_frame % 8 == 0) {
                Palette_StepTo(&g_Palette, &g_Palette.resource, true);
            }
            m_B20J.scroll_frame++;
            m_B20J.entrance_stage = B20J_ENTRANCE_SCROLL;
            if (!Wait_Delay(&m_B20J.wait, 1)) {
                return false;
            }
            continue;
        case B20J_ENTRANCE_SCROLL_END:
            m_B20J.entrance_stage = B20J_ENTRANCE_SHAKE_DELAY;
            return Wait_Scroll(&m_B20J.wait, 0);
        case B20J_ENTRANCE_SHAKE_DELAY:
            m_B20J.animation_frame = 0;
            m_B20J.entrance_stage = B20J_ENTRANCE_SHAKE;
            if (!Wait_Delay(&m_B20J.wait, 40)) {
                return false;
            }
            continue;
        case B20J_ENTRANCE_SHAKE:
            if (m_B20J.animation_frame >= 32) {
                m_B20J.scroll_line = 400;
                m_B20J.entrance_stage = B20J_ENTRANCE_PAN;
                continue;
            }
            m_B20J.entrance_stage = B20J_ENTRANCE_SHAKE_STEP;
            return Wait_Scroll(
                &m_B20J.wait, 416 - m_B20J.animation_frame % 2 * 32);
        case B20J_ENTRANCE_SHAKE_STEP:
            if (m_B20J.animation_frame % 8 == 0) {
                Sound_Effect(SOUND_EFFECT_SHAKE);
            }
            m_B20J.animation_frame++;
            m_B20J.entrance_stage = B20J_ENTRANCE_SHAKE;
            if (!Wait_Delay(&m_B20J.wait, 1)) {
                return false;
            }
            continue;
        case B20J_ENTRANCE_PAN:
            if (m_B20J.scroll_line < 0) {
                m_B20J.animation_frame = 0;
                m_B20J.entrance_stage = B20J_ENTRANCE_TREMBLE;
                continue;
            }
            m_B20J.entrance_stage = B20J_ENTRANCE_PAN_STEP;
            return Wait_Scroll(&m_B20J.wait, m_B20J.scroll_line);
        case B20J_ENTRANCE_PAN_STEP:
            for (int32_t i = 0; i < 32 * 80; i++) {
                const uint16_t offset =
                    (uint16_t)(m_B20J.scroll_line * 80 + i) & 0x7fff;
                if (offset < 32000) {
                    memcpy(
                        g_Video.pages[0] + offset * 8,
                        g_Video.pages[1] + offset * 8, 8);
                }
            }
            Video_SelectDrawingPage(0);
            m_B20J.scroll_line -= 32;
            m_B20J.entrance_stage = B20J_ENTRANCE_PAN;
            if (!Wait_Delay(&m_B20J.wait, 1)) {
                return false;
            }
            continue;
        case B20J_ENTRANCE_TREMBLE:
            if (m_B20J.animation_frame >= 32) {
                m_B20J.entrance_stage = B20J_ENTRANCE_FLASH_DELAY;
                continue;
            }
            m_B20J.entrance_stage = B20J_ENTRANCE_TREMBLE_STEP;
            return Wait_Scroll(
                &m_B20J.wait, 416 - m_B20J.animation_frame % 2 * 16);
        case B20J_ENTRANCE_TREMBLE_STEP:
            m_B20J.animation_frame++;
            m_B20J.entrance_stage = B20J_ENTRANCE_TREMBLE;
            if (!Wait_Delay(&m_B20J.wait, 1)) {
                return false;
            }
            continue;
        case B20J_ENTRANCE_FLASH_DELAY:
            m_B20J.entrance_stage = B20J_ENTRANCE_FLASH;
            if (!Wait_Delay(&m_B20J.wait, 30)) {
                return false;
            }
            continue;
        case B20J_ENTRANCE_FLASH:
            M_White(assets);
            for (int32_t i = 0; i < 4; i++) {
                M_Pi(assets, &assets->layers[i], true);
            }
            m_B20J.animation_frame = 0;
            m_B20J.entrance_stage = B20J_ENTRANCE_FADE_IN;
            continue;
        case B20J_ENTRANCE_FADE_IN:
            if (m_B20J.animation_frame >= 16) {
                Video_Copy(
                    g_Video.drawing_page, (GFX_RECT) { 0, 0, 640, 400 },
                    1 - g_Video.drawing_page, 0, 0);
                m_B20J.entrance_active = false;
                Palette_SetStage(&m_B20J.stage_palette);
                return true;
            }
            M_WhiteIn(assets);
            m_B20J.animation_frame++;
            if (!Wait_Delay(&m_B20J.wait, 10)) {
                return false;
            }
            continue;
        default:
            ASSERT(false);
            return false;
        }
    }
}

static void M_Single(
    const B20J_ASSETS *const assets, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed)
{
    Pellets_AddSingle(
        g_Session.settings.difficulty, x, y, angle, speed, PELLET_REGULAR, 0, 0,
        0);
}
static void M_Rain(
    const B20J_ASSETS *const assets, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed)
{
    Pellets_AddSingle(
        g_Session.settings.difficulty, x, y, angle, speed, PELLET_GRAVITY, 1, 0,
        0);
}
static void M_Group(
    const B20J_ASSETS *const assets, const int16_t x, const int16_t y,
    const int16_t group, const int16_t speed)
{
    Pellets_AddGroup(g_Session.settings.difficulty, x, y, group, speed);
}
static uint8_t M_Angle(const int32_t y, const int32_t x)
{
    return Math16_Angle(Math16_Wrap(y), Math16_Wrap(x));
}
static void M_Diamond(
    const B20J_ASSETS *const assets, const int16_t left, const int16_t top,
    const uint8_t color)
{
    if (left < 0 || left >= 640 || top < 0 || top >= 400) {
        return;
    }
    for (int32_t y = 0; y < 8; y++) {
        for (int32_t x = 0; x < 8; x++) {
            const int32_t offset = (top + y) * 640 + left + x;
            if (offset < VIDEO_PAGE_SIZE
                && (assets->diamond[y] & (0x80u >> x)) != 0) {
                g_Video.pages[g_Video.drawing_page][offset] = color;
            }
        }
    }
}
static void M_Erase(
    const B20J_ASSETS *const assets, const int16_t left, const int16_t top)
{
    Sprite_Restore16(left, top, 16, 8);
}
static void M_Cross(const B20J_ASSETS *const assets)
{
    if (m_B20J.status.frame == 10) {
        M_Expression(assets, 0);
    }
    if (m_B20J.status.frame < 100) {
        return;
    }
    if (m_B20J.status.frame == 100) {
        m_B20J.pattern_state = Boss_Rank(
            PELLET_GROUP_TWO_NARROW_AIMED, PELLET_GROUP_THREE_NARROW_AIMED,
            PELLET_GROUP_FIVE_WIDE_AIMED, PELLET_GROUP_FIVE_NARROW_AIMED);
        m_B20J.diamond_velocity[0] = Math16_Aim(
            (MATH16_VECTOR) { 316, 236 }, (MATH16_VECTOR) { 0, 384 }, 7, 0);
        m_B20J.diamond_velocity[1] = Math16_Aim(
            (MATH16_VECTOR) { 316, 236 }, (MATH16_VECTOR) { 0, 64 }, 7, 0);
        for (int32_t i = 0; i < 4; i++) {
            m_B20J.diamond_left[i] = 316;
            m_B20J.diamond_top[i] = 236;
        }
        M_Group(assets, 312, 188, m_B20J.pattern_state, 48);
        m_B20J.pattern_state = Boss_Rank(18, 16, 14, 12);
        Sound_Effect(SOUND_EFFECT_SPAWN);
    } else if (m_B20J.diamond_left[0] > 0) {
        for (int32_t i = 0; i < 4; i++) {
            M_Erase(assets, m_B20J.diamond_left[i], m_B20J.diamond_top[i]);
        }
        for (int32_t i = 0; i < 4; i++) {
            m_B20J.diamond_left[i] = Math16_Wrap(
                m_B20J.diamond_left[i]
                + (i % 2 == 0 ? 1 : -1) * m_B20J.diamond_velocity[i / 2].x);
            m_B20J.diamond_top[i] = Math16_Wrap(
                m_B20J.diamond_top[i] + m_B20J.diamond_velocity[i / 2].y);
        }
        if (m_B20J.diamond_left[0] <= 0) {
            m_B20J.diamond_left[0] = 0;
            m_B20J.diamond_left[2] = 0;
            m_B20J.diamond_left[1] = 632;
            m_B20J.diamond_left[3] = 632;
        } else {
            for (int32_t i = 0; i < 4; i++) {
                M_Diamond(
                    assets, m_B20J.diamond_left[i], m_B20J.diamond_top[i], 9);
            }
        }
        return;
    } else if (m_B20J.diamond_top[0] > 64) {
        for (int32_t i = 0; i < 4; i++) {
            M_Erase(assets, m_B20J.diamond_left[i], m_B20J.diamond_top[i]);
        }
        m_B20J.diamond_top[0] = Math16_Wrap(m_B20J.diamond_top[0] - 3);
        m_B20J.diamond_top[1] = Math16_Wrap(m_B20J.diamond_top[1] - 3);
        m_B20J.diamond_left[2] = Math16_Wrap(m_B20J.diamond_left[2] + 6);
        m_B20J.diamond_left[3] = Math16_Wrap(m_B20J.diamond_left[3] - 6);
        if (m_B20J.diamond_top[0] <= 64) {
            m_B20J.diamond_top[0] = 64;
        } else {
            for (int32_t i = 0; i < 4; i++) {
                M_Diamond(
                    assets, m_B20J.diamond_left[i], m_B20J.diamond_top[i], 9);
            }
        }
        return;
    } else if (m_B20J.diamond_edges_frame < 200) {
        m_B20J.diamond_edges_frame++;
        if (m_B20J.diamond_edges_frame % m_B20J.pattern_state == 0) {
            int16_t from_y = 64 + M_Random(assets) % 200;
            int16_t to_x = 240 + M_Random(assets) % 400;
            M_Single(assets, 0, from_y, M_Angle(400 - from_y, to_x), 40);
            from_y = 64 + M_Random(assets) % 200;
            to_x = M_Random(assets) % 400;
            M_Single(
                assets, 632, from_y, M_Angle(400 - from_y, to_x - 632), 40);
            for (int32_t i = 0; i < 2; i++) {
                const int16_t from_x = M_Random(assets) % 640;
                to_x = M_Random(assets) % 640;
                M_Single(assets, from_x, 64, M_Angle(336, to_x - from_x), 40);
            }
            const int16_t from_x = M_Random(assets) % 640;
            M_Group(assets, from_x, 64, PELLET_GROUP_SINGLE_AIMED, 40);
        }
        return;
    } else {
        m_B20J.status.frame = 0;
    }
    m_B20J.diamond_edges_frame = 0;
}
static void M_CupFire(const B20J_ASSETS *const assets, const uint8_t angle)
{
    M_Single(assets, 328, 188, angle, m_B20J.pattern_state);
    M_Single(assets, 296, 188, angle, m_B20J.pattern_state);
}
static void M_Cup(const B20J_ASSETS *const assets)
{
    if (m_B20J.status.frame == 10) {
        M_Expression(assets, 1);
    }
    if (m_B20J.status.frame < 100) {
        return;
    }
    if (m_B20J.status.frame == 100) {
        m_B20J.cup_angle = 64;
        m_B20J.pattern_state = Boss_Rank(80, 80, 96, 112);
    }
    if (m_B20J.status.frame < 140 && m_B20J.status.frame % 8 == 0) {
        M_CupFire(assets, 64);
        return;
    }
    if (m_B20J.status.frame < 220 && m_B20J.status.frame % 8 == 0) {
        m_B20J.cup_angle += 5;
        M_CupFire(assets, m_B20J.cup_angle);
        M_CupFire(assets, 128 - m_B20J.cup_angle);
        return;
    }
    if (m_B20J.status.frame < 300 && m_B20J.status.frame % 8 == 0) {
        m_B20J.cup_angle -= 12;
        M_CupFire(assets, m_B20J.cup_angle);
        M_CupFire(assets, 128 - m_B20J.cup_angle);
    }
    if (m_B20J.status.frame >= 300) {
        m_B20J.status.frame = 0;
    }
}
static void M_SnakeAim(
    B20J_SNAKES *const s, const B20J_ASSETS *const assets, const int32_t i,
    const int16_t target, const int16_t speed)
{
    uint8_t angle = M_Angle(384 - s->top[i][0], target + 12 - s->left[i][0]);
    angle += M_Random(assets) % 2 == 0 ? 40 : -40;
    s->velocity[i] = Math16_Vector(speed, angle);
}
static void M_SnakeSpawn(
    B20J_SNAKES *const s, const B20J_ASSETS *const assets, const int32_t i,
    const int16_t x, const int16_t y)
{
    for (int32_t j = 0; j < 5; j++) {
        s->left[i][j] = x;
        s->top[i][j] = y;
    }
    M_SnakeAim(s, assets, i, g_Player->left, 6);
}
static void M_SnakesUpdate(
    B20J_SNAKES *const s, const B20J_ASSETS *const assets, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        if (s->left[i][0] == 999) {
            continue;
        }
        M_Erase(assets, s->left[i][4], s->top[i][4]);
        for (int32_t j = 3; j >= 1; j--) {
            M_Diamond(assets, s->left[i][j], s->top[i][j], 9);
        }
        M_Diamond(assets, s->left[i][0], s->top[i][0], 7);
        if (!s->target_locked[i]) {
            s->target_left[i] = g_Player->left;
        }
        M_SnakeAim(s, assets, i, s->target_left[i], 7);
        if (s->top[i][0] > 300) {
            s->target_locked[i] = true;
        }
        for (int32_t j = 4; j >= 1; j--) {
            s->left[i][j] = s->left[i][j - 1];
            s->top[i][j] = s->top[i][j - 1];
        }
        s->left[i][0] = Math16_Wrap(s->left[i][0] + s->velocity[i].x);
        s->top[i][0] = Math16_Wrap(s->top[i][0] + s->velocity[i].y);
        const PLAYER *const player = g_Player;
        if (s->left[i][0] > player->left && s->left[i][0] < player->left + 24
            && s->top[i][0] > 368 + (player->sliding ? 10 : 0)
            && s->top[i][0] < 392 && !player->invincible) {
            Player_Hit();
        }
    }
}
static void M_Snakes(const B20J_ASSETS *const assets, const bool four)
{
    B20J_SNAKES *const s = four ? &m_B20J.four_snakes : &m_B20J.two_snakes;
    const int32_t count = four ? 4 : 2;
    if (m_B20J.status.frame == 10) {
        M_Expression(assets, 2);
    }
    if (m_B20J.status.frame < 100) {
        return;
    }
    if (m_B20J.status.frame == 100) {
        for (int32_t i = 0; i < count; i++) {
            s->target_locked[i] = false;
        }
        M_SnakeSpawn(s, assets, 0, 312, 188);
        for (int32_t i = 1; i < count; i++) {
            s->left[i][0] = 999;
        }
        if (four) {
            m_B20J.pattern_state = Boss_Rank(18, 16, 14, 12);
        }
        Sound_Effect(SOUND_EFFECT_SPAWN);
        return;
    }
    if (m_B20J.status.frame < (four ? 400 : 500)) {
        M_SnakesUpdate(s, assets, count);
        if (m_B20J.status.frame == 150) {
            M_SnakeSpawn(s, assets, 1, 312, 188);
            Sound_Effect(SOUND_EFFECT_SPAWN);
        }
        if (four
            && (m_B20J.status.frame == 200 || m_B20J.status.frame == 250)) {
            M_SnakeSpawn(
                s, assets, m_B20J.status.frame == 200 ? 2 : 3, 290, 200);
            Sound_Effect(SOUND_EFFECT_SPAWN);
        }
        if (!four
            && m_B20J.status.frame > 240 - g_Session.settings.difficulty * 40
            && m_B20J.status.frame % 40 == 0) {
            uint8_t angle = M_Random(assets) % 12;
            const int16_t x = m_B20J.status.frame % 120 == 0 ? 410
                : m_B20J.status.frame % 120 == 40            ? 316
                                                             : 312;
            const int16_t y = m_B20J.status.frame % 120 == 0 ? 70
                : m_B20J.status.frame % 120 == 40            ? 140
                                                             : 188;
            for (int32_t i = 0; i < 10; i++) {
                const int16_t speed = 40
                    + (i % 4 == 1       ? 16
                           : i % 4 == 3 ? 32
                                        : 0);
                M_Single(assets, x, y, 128 - angle, speed);
                M_Single(assets, x, y, angle, speed);
                angle += 12;
            }
            Sound_Effect(SOUND_EFFECT_PELLET);
        }
    } else {
        for (int32_t i = 0; i < count; i++) {
            for (int32_t j = 0; j < 5; j++) {
                M_Erase(assets, s->left[i][j], s->top[i][j]);
            }
        }
        m_B20J.status.frame = 0;
    }
}
static void M_Rows(const B20J_ASSETS *const assets)
{
    if (m_B20J.status.frame == 10) {
        M_Expression(assets, 0);
    }
    if (m_B20J.status.frame < 100) {
        return;
    }
    if (m_B20J.status.frame == 100) {
        m_B20J.row_velocity = Math16_Aim(
            (MATH16_VECTOR) { 290, 200 }, (MATH16_VECTOR) { 64, 64 }, 16, 0);
        m_B20J.row_left = 290;
        m_B20J.row_top = 200;
        m_B20J.row_speed = 48;
        m_B20J.row_direction = 99;
        Sound_Effect(SOUND_EFFECT_SPAWN);
        m_B20J.pattern_state = Boss_Rank(12, 10, 8, 6);
        return;
    }
    if (m_B20J.row_direction == 99) {
        M_Erase(assets, m_B20J.row_left, m_B20J.row_top);
        m_B20J.row_left = Math16_Wrap(m_B20J.row_left + m_B20J.row_velocity.x);
        m_B20J.row_top = Math16_Wrap(m_B20J.row_top + m_B20J.row_velocity.y);
        if (m_B20J.row_top <= 64) {
            m_B20J.row_left = 64;
            m_B20J.row_top = 64;
            m_B20J.row_direction = 0;
            return;
        }
        M_Diamond(assets, m_B20J.row_left, m_B20J.row_top, 9);
    } else if (m_B20J.row_direction < 99) {
        M_Erase(assets, m_B20J.row_left, m_B20J.row_top);
        m_B20J.row_left = Math16_Wrap(
            m_B20J.row_left
            + (m_B20J.row_direction == 0       ? 8
                   : m_B20J.row_direction == 1 ? -8
                                               : 0));
        if (m_B20J.row_direction >= 2) {
            m_B20J.row_top = Math16_Wrap(m_B20J.row_top + 8);
            m_B20J.row_direction++;
            if (m_B20J.row_direction >= 10) {
                m_B20J.row_direction = m_B20J.row_left > 320 ? 1 : 0;
            }
        }
        if (m_B20J.row_left > 576) {
            m_B20J.row_direction = 2;
            m_B20J.row_left = 576;
        } else if (m_B20J.row_left < 64) {
            m_B20J.row_direction = 2;
            m_B20J.row_left = 64;
        }
        if (m_B20J.row_top >= 256) {
            m_B20J.status.frame = 0;
            return;
        }
        if (m_B20J.row_direction < 2
            && m_B20J.status.frame % m_B20J.pattern_state == 0) {
            M_Group(
                assets, m_B20J.row_left, m_B20J.row_top,
                PELLET_GROUP_SINGLE_AIMED, m_B20J.row_speed);
            m_B20J.row_speed = Math16_Wrap(m_B20J.row_speed + 2);
        }
        M_Diamond(assets, m_B20J.row_left, m_B20J.row_top, 6);
    }
}
static void M_Spray(const B20J_ASSETS *const assets)
{
    if (m_B20J.status.frame == 10) {
        M_Expression(assets, 1);
    }
    if (m_B20J.status.frame < 100) {
        return;
    }
    if (m_B20J.status.frame == 100) {
        m_B20J.spray_offset = 32;
        m_B20J.spray_delta = -8;
        m_B20J.spray_fired = 0;
        m_B20J.pattern_state = Boss_Rank(5, 4, 3, 2);
    }
    if (m_B20J.status.frame % m_B20J.pattern_state == 0) {
        m_B20J.spray_angle =
            M_Angle(368 - 184, g_Player->left - 278) + m_B20J.spray_offset;
        m_B20J.spray_offset += m_B20J.spray_delta;
        m_B20J.spray_fired++;
        if (m_B20J.spray_fired > 8) {
            m_B20J.spray_delta *= -1;
            m_B20J.spray_fired = 0;
        }
        M_Single(assets, 312, 188, m_B20J.spray_angle, 48);
    }
    if (m_B20J.status.frame >= 700) {
        m_B20J.status.frame = 0;
    }
}
static void M_Ray(
    const B20J_ASSETS *const assets, const int16_t dx, const int16_t dy)
{
    if (m_B20J.ray_previous_x >= 0 && m_B20J.ray_previous_x < 640
        && m_B20J.ray_previous_y >= 0 && m_B20J.ray_previous_y < 400) {
        Video_LineErase(410, 70, m_B20J.ray_previous_x, m_B20J.ray_previous_y);
    }
    if (m_B20J.ray_x >= 0 && m_B20J.ray_x < 640 && m_B20J.ray_y >= 0
        && m_B20J.ray_y < 400) {
        Video_Line(410, 70, m_B20J.ray_x, m_B20J.ray_y, 6);
    }
    m_B20J.ray_previous_x = m_B20J.ray_x;
    m_B20J.ray_previous_y = m_B20J.ray_y;
    m_B20J.ray_x = Math16_Wrap(m_B20J.ray_x + dx);
    m_B20J.ray_y = Math16_Wrap(m_B20J.ray_y + dy);
}
static void M_Edges(const B20J_ASSETS *const assets)
{
    if (m_B20J.status.frame == 10) {
        M_Expression(assets, 3);
    }
    if (m_B20J.status.frame < 100) {
        return;
    }
    if (m_B20J.status.frame == 100) {
        m_B20J.ray_x = 0;
        m_B20J.ray_y = 264;
        m_B20J.pattern_state = Boss_Rank(5, 3, 2, 2);
    }
    if (m_B20J.status.frame < 125) {
        M_Ray(assets, 0, -8);
    } else if (m_B20J.status.frame <= 125) {
        m_B20J.ray_x = 0;
        m_B20J.ray_y = 64;
        M_Ray(assets, 8, 0);
    } else if (m_B20J.status.frame < 205) {
        M_Ray(assets, 8, 0);
    } else if (m_B20J.status.frame == 205) {
        m_B20J.ray_x = 639;
        m_B20J.ray_y = 64;
        M_Ray(assets, 0, 8);
    } else if (m_B20J.status.frame < 230) {
        M_Ray(assets, 0, 8);
    } else if (m_B20J.status.frame == 230) {
        m_B20J.ray_x = 639;
        m_B20J.ray_y = 264;
        M_Ray(assets, 0, -8);
    } else if (m_B20J.status.frame < 255) {
        M_Ray(assets, 0, -8);
    } else if (m_B20J.status.frame == 255) {
        m_B20J.ray_x = 639;
        m_B20J.ray_y = 64;
        M_Ray(assets, -8, 0);
    } else if (m_B20J.status.frame < 335) {
        M_Ray(assets, -8, 0);
    } else if (m_B20J.status.frame == 335) {
        m_B20J.ray_x = 0;
        m_B20J.ray_y = 64;
        M_Ray(assets, 0, 8);
    } else if (m_B20J.status.frame < 360) {
        M_Ray(assets, 0, 8);
    } else if (m_B20J.status.frame == 360) {
        m_B20J.ray_y = Math16_Wrap(m_B20J.ray_y - 8);
        Video_LineErase(410, 70, m_B20J.ray_x, m_B20J.ray_y);
        m_B20J.status.frame = 0;
    }
    if (m_B20J.status.frame % 10 == 0) {
        Sound_Effect(SOUND_EFFECT_LASER);
    }
    if (m_B20J.status.frame % m_B20J.pattern_state == 0) {
        M_Rain(assets, m_B20J.ray_x, m_B20J.ray_y, M_Random(assets) & 127, 32);
        M_Rain(assets, m_B20J.ray_x, m_B20J.ray_y, M_Random(assets) & 127, 32);
    }
}
static void M_SlashAnimate(const B20J_ASSETS *const assets)
{
    const int16_t frames[] = { 50, 60, 100, 120, 140, 160, 170 };
    for (int32_t i = 0; i < 7; i++) {
        if (m_B20J.status.frame < frames[i]) {
            return;
        }
        if (m_B20J.status.frame == frames[i]) {
            M_SlashPut(assets, i);
            if (i == 0) {
                Sound_Effect(SOUND_EFFECT_TELEGRAPH);
            }
        }
    }
    if (m_B20J.status.frame > 200) {
        m_B20J.status.frame = 0;
        m_B20J.direction_can_change = true;
    }
}
static void M_SlashFire(const B20J_ASSETS *const assets, const int32_t pattern)
{
    const int16_t x = m_B20J.slash_left[pattern];
    const int16_t y = m_B20J.slash_top[pattern];
    if (pattern == 0) {
        M_Rain(assets, x, y, M_Random(assets) % 127, 0);
        M_Rain(assets, x, y, M_Random(assets) % 127, 0);
    } else if (pattern == 1) {
        M_Single(assets, x, y, 32, m_B20J.pattern_state);
        M_Single(assets, x, y, 96, m_B20J.pattern_state);
    } else {
        M_Group(assets, x, y, PELLET_GROUP_SINGLE_AIMED, m_B20J.pattern_state);
    }
}
static void M_Slash(const B20J_ASSETS *const assets, const int32_t pattern)
{
    if (m_B20J.status.frame == 10) {
        M_Direction(assets, 2);
        M_Expression(assets, pattern == 0 ? 1 : 3);
        m_B20J.direction_can_change = false;
        m_B20J.slash_left[pattern] = 410;
        m_B20J.slash_top[pattern] = 70;
        m_B20J.pattern_state = pattern == 0 ? Boss_Rank(5, 3, 2, 1)
            : pattern == 1                  ? Boss_Rank(32, 48, 64, 72)
                                            : Boss_Rank(64, 80, 88, 96);
    }
    M_SlashAnimate(assets);
    if (m_B20J.status.frame < 100) {
        return;
    }
    const int16_t interval = pattern == 0 ? m_B20J.pattern_state : 3;
    if (m_B20J.status.frame < 140 && m_B20J.status.frame % interval == 0) {
        M_SlashFire(assets, pattern);
        m_B20J.slash_left[pattern] =
            Math16_Wrap(m_B20J.slash_left[pattern] + 22 / (40 / interval));
        m_B20J.slash_top[pattern] =
            Math16_Wrap(m_B20J.slash_top[pattern] + 162 / (40 / interval));
    }
    if (m_B20J.status.frame == 140) {
        m_B20J.slash_left[pattern] = 432;
        m_B20J.slash_top[pattern] = 232;
        if (pattern == 0) {
            m_B20J.pattern_state = Boss_Rank(3, 2, 2, 2);
        }
    }
    if (m_B20J.status.frame < 140) {
        return;
    }
    if (m_B20J.status.frame < 150) {
        M_SlashFire(assets, pattern);
        m_B20J.slash_left[pattern] =
            Math16_Wrap(m_B20J.slash_left[pattern] - (-234 / -10));
        m_B20J.slash_top[pattern] =
            Math16_Wrap(m_B20J.slash_top[pattern] - (-34 / -10));
    }
    if (m_B20J.status.frame == 150) {
        m_B20J.slash_left[pattern] = 198;
        m_B20J.slash_top[pattern] = 198;
        if (pattern == 0) {
            m_B20J.pattern_state = Boss_Rank(3, 2, 1, 1);
        }
    }
    if (m_B20J.status.frame < 150) {
        return;
    }
    const int16_t last_interval = pattern == 0 ? m_B20J.pattern_state : 2;
    if (m_B20J.status.frame < 170 && m_B20J.status.frame % last_interval == 0) {
        M_SlashFire(assets, pattern);
        m_B20J.slash_left[pattern] = Math16_Wrap(
            m_B20J.slash_left[pattern] - (-212 / (20 / last_interval)));
        m_B20J.slash_top[pattern] = Math16_Wrap(
            m_B20J.slash_top[pattern] - (128 / (20 / last_interval)));
    }
}
static void M_Lasers(const B20J_ASSETS *const assets)
{
    if (m_B20J.status.frame == 10) {
        M_Expression(assets, 3);
    }
    if (m_B20J.status.frame < 100) {
        return;
    }
    if (m_B20J.status.frame == 100) {
        m_B20J.laser_right_to_left = M_Random(assets) % 2 != 0;
        m_B20J.pattern_state = Boss_Rank(128, 96, 80, 64);
    }
    if (m_B20J.status.frame % 10 == 0) {
        const int16_t distance = Math16_Wrap(
            (m_B20J.status.frame - 100) / 10 * m_B20J.pattern_state);
        const int16_t target =
            Math16_Wrap(m_B20J.laser_right_to_left ? 640 - distance : distance);
        ShootoutLasers_Spawn(
            (m_B20J.status.frame / 10) % 10, 410, 70, target, 400, 68, 7, 30,
            5);
        Sound_Effect(SOUND_EFFECT_LASER);
        if ((!m_B20J.laser_right_to_left && target >= 640)
            || (m_B20J.laser_right_to_left && target <= 0)) {
            m_B20J.status.frame = 0;
        }
        M_Group(assets, 410, 70, PELLET_GROUP_THREE_WIDE_AIMED, 72);
    }
}
static void M_Sleeve(const B20J_ASSETS *const assets)
{
    if (m_B20J.status.frame == 10) {
        M_Expression(assets, 3);
    }
    if (m_B20J.status.frame < 100) {
        return;
    }
    if (m_B20J.status.frame % 20 == 0 || m_B20J.status.frame % 20 == 10) {
        uint8_t angle = 0;
        for (int32_t i = 0; i < 10; i++, angle -= 12) {
            if (m_B20J.status.frame % 20 == 0) {
                M_Rain(assets, 290, 200, angle, 32);
            } else {
                Pellets_AddSingle(
                    g_Session.settings.difficulty, 290, 200, angle, 32,
                    PELLET_FALL_STRAIGHT_FROM_TOP_THEN_REGULAR, 48, 0, 0);
            }
        }
        Sound_Effect(SOUND_EFFECT_PELLET);
    }
    if (m_B20J.status.frame >= 300) {
        m_B20J.status.frame = 0;
    }
}

static void M_BeginDeath(const B20J_ASSETS *const assets)
{
    Tram_Clear();
    Video_Clear(0);
    Sound_Stop();
    m_B20J.defeating = true;
    m_B20J.defeat_stage = B20J_DEFEAT_KUJI;
    m_B20J.animation_frame = 7;
}
static bool M_Death(const B20J_ASSETS *const assets)
{
    if (!Wait_Ready(&m_B20J.wait, 0)) {
        return false;
    }
    for (;;) {
        switch (m_B20J.defeat_stage) {
        case B20J_DEFEAT_KUJI:
            if (m_B20J.animation_frame > 15) {
                m_B20J.status.frame = 0;
                m_B20J.defeat_stage = B20J_DEFEAT_SHAKE;
                continue;
            }
            if (m_B20J.animation_frame != 7) {
                Video_Clear(0);
            }
            const int16_t flash = m_B20J.animation_frame % 2 == 0 ? 15 : 0;
            Palette_SetColor(&g_Palette, 0, flash, flash, flash);
            M_Grx(assets, m_B20J.animation_frame);
            m_B20J.animation_frame++;
            if (!Wait_Delay(&m_B20J.wait, 10)) {
                return false;
            }
            continue;
        case B20J_DEFEAT_SHAKE:
            m_B20J.status.frame = Math16_Wrap(m_B20J.status.frame + 1);
            if (m_B20J.status.frame % 4 == 0) {
                M_BlackOut(assets);
                Sound_Effect(SOUND_EFFECT_PELLET);
            }
            m_B20J.defeat_stage = B20J_DEFEAT_SHAKE_STEP;
            return Wait_Scroll(
                &m_B20J.wait, 408 + m_B20J.status.frame % 2 * 16);
        case B20J_DEFEAT_SHAKE_STEP:
            if (m_B20J.status.frame > 64) {
                Tram_Fill((GFX_RECT) { 0, 0, 640, 400 }, TRAM_BLACK);
                M_Pi(assets, &assets->scroll, false);
                Palette_SetGray(&g_Palette, 0);
                Tram_Clear();
                m_B20J.scroll_line = 0;
                m_B20J.scroll_frame = 0;
                m_B20J.defeat_stage = B20J_DEFEAT_SCROLL;
                continue;
            }
            m_B20J.defeat_stage = B20J_DEFEAT_SHAKE;
            if (!Wait_Delay(&m_B20J.wait, 1)) {
                return false;
            }
            continue;
        case B20J_DEFEAT_SCROLL:
            m_B20J.defeat_stage = B20J_DEFEAT_SCROLL_STEP;
            return Wait_Scroll(&m_B20J.wait, m_B20J.scroll_line);
        case B20J_DEFEAT_SCROLL_STEP:
            m_B20J.scroll_line += 32;
            if (m_B20J.scroll_line > 400) {
                m_B20J.scroll_line -= 400;
            }
            if (m_B20J.scroll_frame < 150) {
                if (m_B20J.scroll_frame % 8 == 0) {
                    Palette_StepTo(&g_Palette, &g_Palette.resource, true);
                    Sound_Effect(SOUND_EFFECT_PELLET);
                }
            } else if (m_B20J.scroll_frame % 8 == 0) {
                if (m_B20J.scroll_frame == 192) {
                    Sound_Fade();
                }
                M_BlackOut(assets);
                if (g_Palette.saved.colors[7][0] == 0) {
                    m_B20J.defeat_stage = B20J_DEFEAT_CLEAR;
                    return Wait_Scroll(&m_B20J.wait, 0);
                }
                if (m_B20J.scroll_frame < 220) {
                    Sound_Effect(SOUND_EFFECT_PELLET);
                }
            }
            m_B20J.scroll_frame++;
            m_B20J.defeat_stage = B20J_DEFEAT_SCROLL;
            if (!Wait_Delay(&m_B20J.wait, 1)) {
                return false;
            }
            continue;
        case B20J_DEFEAT_CLEAR:
            for (int32_t i = 0; i < 5; i++) {
                Stage_AddScore(10000);
            }
            Stage_SetGameCleared();
            m_B20J.finished = true;
            m_B20J.defeating = false;
            return true;
        default:
            ASSERT(false);
            return false;
        }
    }
}
static void M_Hit(void)
{
    const uint8_t colors[] = { 3, 4, 5 };
    const ORB *const orb = g_Orb;
    const bool collision = orb->left >= 288 && orb->left <= 352
        && orb->top >= 120 && orb->top <= 160;
    BossHit_UpdateAndRender(
        &m_B20J.hit, &m_B20J.status.hp, &m_B20J.boss_palette, colors, 3, 10000,
        collision, (BOSS_SHOT_HITBOX) { 288, 120, 96, 40 });
}
static void M_Next(const B20J_ASSETS *const assets, const int8_t phase)
{
    m_B20J.status.phase = phase;
    m_B20J.status.frame = 0;
    m_B20J.hit.frame = 0;
    m_B20J.pattern = 99;
    m_B20J.patterns_done = 0;
    if ((phase & 1) != 0) {
        M_Expression(assets, 0);
    }
}
static void M_Pattern(const B20J_ASSETS *const assets, const int16_t pattern)
{
    switch (pattern) {
    case 0:
        M_Cross(assets);
        break;
    case 1:
        M_Cup(assets);
        break;
    case 2:
        M_Snakes(assets, false);
        break;
    case 3:
        M_Rows(assets);
        break;
    case 4:
        M_Spray(assets);
        break;
    case 5:
        M_Snakes(assets, true);
        break;
    case 6:
        M_Edges(assets);
        break;
    case 7:
        M_Slash(assets, 0);
        break;
    case 8:
        M_Lasers(assets);
        break;
    case 9:
        M_Slash(assets, 1);
        break;
    case 10:
        M_Sleeve(assets);
        break;
    case 11:
        M_Slash(assets, 2);
        break;
    default:
        break;
    }
}
static bool M_Update(const void *const data, const BOSS_CONTEXT *const ctx)
{
    const B20J_ASSETS *const assets = data;
    if (m_B20J.finished) {
        return true;
    }
    if (m_B20J.entrance_active) {
        return M_ResumeEntrance(assets, ctx);
    }
    if (m_B20J.defeating) {
        return M_Death(assets);
    }
    if (m_B20J.status.phase == B20J_PHASE_START) {
        m_B20J.status.phase = B20J_PHASE_PATTERNS_1;
        m_B20J.pattern_previous = 99;
        m_B20J.pattern = 99;
        m_B20J.patterns_done = 0;
        m_B20J.status.frame = 0;
        m_B20J.hit.frame = 0;
        m_B20J.hit.invincible = false;
        m_B20J.hp_bar_filled = false;
        m_B20J.boss_palette = g_Palette.saved;
        Rng_Seed(g_Session.frame_rand);
        return true;
    }
    if (m_B20J.status.phase < B20J_PHASE_PATTERNS_1
        || m_B20J.status.phase > B20J_PHASE_FINAL) {
        return true;
    }
    if (m_B20J.status.phase == B20J_PHASE_PATTERNS_1 && !m_B20J.hp_bar_filled) {
        m_B20J.hp_bar_filled =
            HudHp_Increment(m_B20J.status.hp, m_B20J.status.frame);
    }
    m_B20J.status.frame = Math16_Wrap(m_B20J.status.frame + 1);
    m_B20J.hit.frame = Math16_Wrap(m_B20J.hit.frame + 1);
    const int16_t direction = g_Player->left < 198 ? 1
        : g_Player->left > 396                     ? 0
                                                   : 2;
    M_Direction(assets, direction);
    if ((m_B20J.status.phase & 1) == 0) {
        if (m_B20J.status.frame == 50) {
            M_White(assets);
        }
        if (m_B20J.status.frame > 50 && m_B20J.status.frame % 4 == 0) {
            M_WhiteIn(assets);
        }
        M_Hit();
        if (!m_B20J.hit.invincible && m_B20J.status.frame > 120) {
            M_Next(assets, m_B20J.status.phase + 1);
        }
        return true;
    }
    if (m_B20J.pattern == 99) {
        if (m_B20J.status.frame
            > (m_B20J.status.phase == B20J_PHASE_FINAL ? 5 : 120)) {
            m_B20J.status.frame = 1;
            const int16_t count = m_B20J.status.phase == B20J_PHASE_PATTERNS_1
                ? 3
                : m_B20J.status.phase == B20J_PHASE_PATTERNS_2 ? 4
                : m_B20J.status.phase == B20J_PHASE_PATTERNS_3 ? 3
                                                               : 12;
            const int16_t retry =
                m_B20J.status.phase >= B20J_PHASE_PATTERNS_3 ? 2 : count;
            m_B20J.pattern = M_Random(assets) % count;
            if (m_B20J.pattern == m_B20J.pattern_previous) {
                m_B20J.pattern = M_Random(assets) % retry;
            }
            m_B20J.pattern_previous = m_B20J.pattern;
            m_B20J.patterns_done = Math16_Wrap(m_B20J.patterns_done + 1);
        }
    } else {
        const int16_t pattern = m_B20J.status.phase == B20J_PHASE_PATTERNS_2
            ? m_B20J.pattern + 3
            : m_B20J.status.phase == B20J_PHASE_PATTERNS_3 ? m_B20J.pattern + 7
                                                           : m_B20J.pattern;
        M_Pattern(assets, pattern);
    }
    if (m_B20J.status.frame == 0) {
        M_Expression(assets, 0);
        m_B20J.pattern = 99;
    }
    M_Hit();
    if (m_B20J.status.phase == B20J_PHASE_FINAL) {
        if (m_B20J.status.hp <= 0) {
            M_BeginDeath(assets);
            return M_Death(assets);
        }
    } else if (!m_B20J.hit.invincible && m_B20J.pattern == 99) {
        const int16_t patterns = m_B20J.status.phase == B20J_PHASE_PATTERNS_1
            ? 7
            : m_B20J.status.phase == B20J_PHASE_PATTERNS_2 ? 9
                                                           : 6;
        const int16_t threshold = m_B20J.status.phase == B20J_PHASE_PATTERNS_1
            ? 16
            : m_B20J.status.phase == B20J_PHASE_PATTERNS_2 ? 13
                                                           : 10;
        if (m_B20J.patterns_done >= patterns || m_B20J.status.hp < threshold) {
            M_Next(assets, m_B20J.status.phase + 1);
        }
    }
    return true;
}

const BOSS_STRATEGY g_BossKonngara = {
    .assets_size = sizeof(B20J_ASSETS),
    .load_assets = M_LoadAssets,
    .init = M_Init,
    .setup = M_Setup,
    .begin_entrance = M_BeginEntrance,
    .resume_entrance = M_ResumeEntrance,
    .update = M_Update,
    .song = -1,
};
