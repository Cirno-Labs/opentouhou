#include "th01/stage_objects.h"
#include "th01/rng.h"

#include "pc98/zun/master_rng.h"
#include "th01/sprite.h"

#include <string.h>

typedef struct {
    STAGE_CARD *cards;
    STAGE_OBSTACLE *obstacles;
    uint8_t *backgrounds;
} STAGE_OBJECT_STORAGE;

STAGE_OBJECTS g_StageObjects;
static STAGE_OBJECT_STORAGE m_Storage;

static const uint8_t m_CardAnimation[5][5] = {
    { 17, 18, 19, 20, 21 }, { 14, 15, 16, 18, 17 }, { 11, 12, 13, 15, 14 },
    { 8, 9, 10, 12, 11 },   { 8, 9, 10, 9, 8 },
};

static int32_t M_Offset(const int16_t left, const int16_t top)
{
    return top * VIDEO_WIDTH + left - (left % 8 + 8) % 8;
}

static int32_t M_ObstacleImage(const STAGE_DATA_OBSTACLE type)
{
    switch (type) {
    case STAGE_DATA_BUMPER:
        return 22;
    case STAGE_DATA_PORTAL:
        return 29;
    case STAGE_DATA_BAR_TOP:
        return 25;
    case STAGE_DATA_BAR_BOTTOM:
        return 27;
    case STAGE_DATA_BAR_LEFT:
        return 26;
    case STAGE_DATA_BAR_RIGHT:
        return 28;
    default:
        return 23;
    }
}

static void M_Snapshot(
    const int16_t left, const int16_t top, const int32_t slot)
{
    Video_SelectDrawingPage(1);
    const int32_t offset = M_Offset(left, top);
    for (int32_t y = 0; y < 32; y++) {
        for (int32_t x = 0; x < 32; x++) {
            const int32_t at = offset + y * VIDEO_WIDTH + x;
            if (at >= 0 && at < VIDEO_PAGE_SIZE) {
                g_StageObjects.backgrounds[slot * 1024 + y * 32 + x] =
                    g_Video.pages[1][at];
            }
        }
    }
    Video_SelectDrawingPage(0);
}

static void M_Copy(const int16_t left, const int16_t top)
{
    const int32_t offset = M_Offset(left, top);
    for (int32_t y = 0; y < 32; y++) {
        for (int32_t x = 0; x < 32; x++) {
            const int32_t at = offset + y * VIDEO_WIDTH + x;
            if (at >= 0 && at < VIDEO_PAGE_SIZE) {
                g_Video.pages[1][at] = g_Video.pages[0][at];
            }
        }
    }
}

RESULT StageObjects_ValidateSprites(
    const PTN *const sprites, const int16_t stage_id)
{
    if (stage_id < 0 || sprites == nullptr || sprites->pixels == nullptr
        || sprites->count < (stage_id % 5 == 4 ? 7 : 32)) {
        return FAIL("Invalid stage object sprites or stage ID");
    }
    return OK;
}

static void M_Init(
    const STAGE_DATA *const data, const int16_t stage_id,
    const bool first_in_scene, const ALLOCATOR *const allocator,
    const PTN *const sprites)
{
    const STAGE_OBJECTS previous = g_StageObjects;
    memset(&g_StageObjects, 0, sizeof(g_StageObjects));
    g_StageObjects.entrance_done = true;
    g_StageObjects.vertical_bar_active = previous.vertical_bar_active;
    g_StageObjects.portal_active = previous.portal_active;
    g_StageObjects.entered_portal = previous.entered_portal;
    g_StageObjects.destination_left = previous.destination_left;
    g_StageObjects.destination_top = previous.destination_top;
    if (stage_id % 5 == 4) {
        return;
    }
    const STAGE_DATA_STAGE *const layout = &data->stages[stage_id % 5];
    for (int32_t y = 0; y < 10; y++) {
        for (int32_t x = 0; x < 20; x++) {
            g_StageObjects.card_count += layout->cells[y][x].card;
            g_StageObjects.obstacle_count +=
                layout->cells[y][x].obstacle != STAGE_DATA_OBSTACLE_NONE;
        }
    }
    if (allocator == nullptr) {
        g_StageObjects.cards = m_Storage.cards;
        g_StageObjects.obstacles = m_Storage.obstacles;
        g_StageObjects.backgrounds = m_Storage.backgrounds;
        memset(
            g_StageObjects.cards, 0,
            (size_t)g_StageObjects.card_count * sizeof(STAGE_CARD));
        memset(
            g_StageObjects.obstacles, 0,
            (size_t)g_StageObjects.obstacle_count * sizeof(STAGE_OBSTACLE));
        memset(
            g_StageObjects.backgrounds, 0,
            (size_t)(g_StageObjects.card_count + g_StageObjects.obstacle_count)
                * 1024);
    } else {
        if (g_StageObjects.card_count + g_StageObjects.obstacle_count != 0) {
            g_StageObjects.backgrounds = Allocator_AllocZ(
                allocator,
                (size_t)(g_StageObjects.card_count
                         + g_StageObjects.obstacle_count)
                    * 1024);
        }
        if (g_StageObjects.card_count != 0) {
            g_StageObjects.cards = Allocator_AllocZ(
                allocator,
                (size_t)g_StageObjects.card_count * sizeof(STAGE_CARD));
        }
        if (g_StageObjects.obstacle_count != 0) {
            g_StageObjects.obstacles = Allocator_AllocZ(
                allocator,
                (size_t)g_StageObjects.obstacle_count * sizeof(STAGE_OBSTACLE));
        }
    }
    MasterRng_Next(&g_Rng);
    int32_t card = 0;
    for (int32_t y = 0; y < 10; y++) {
        for (int32_t x = 0; x < 20; x++) {
            if (layout->cells[y][x].card) {
                g_StageObjects.cards[card] = (STAGE_CARD) {
                    .left = (int16_t)(x * 32),
                    .top = (int16_t)(y * 32 + 64),
                    .hp = layout->cells[y][x].card_extra_hp,
                };
                M_Snapshot(
                    g_StageObjects.cards[card].left,
                    g_StageObjects.cards[card].top, card);
                card++;
            }
        }
    }
    int32_t obstacle = 0;
    for (int32_t y = 0; y < 10; y++) {
        for (int32_t x = 0; x < 20; x++) {
            const STAGE_DATA_OBSTACLE type = layout->cells[y][x].obstacle;
            if (type != STAGE_DATA_OBSTACLE_NONE) {
                const int16_t left = (int16_t)(x * 32);
                const int16_t top = (int16_t)(y * 32 + 64);
                g_StageObjects.obstacles[obstacle] =
                    (STAGE_OBSTACLE) { .left = left, .top = top, .type = type };
                M_Snapshot(left, top, obstacle + g_StageObjects.card_count);
                Sprite_PtnPut(sprites, M_ObstacleImage(type), -1, left, top);
                obstacle++;
            }
        }
    }
    g_StageObjects.entrance_done = first_in_scene;
    if (first_in_scene) {
        for (int32_t i = 0; i < g_StageObjects.card_count; i++) {
            const STAGE_CARD *const c = &g_StageObjects.cards[i];
            Sprite_PtnPut(
                sprites, StageObjects_CardImage(c->hp, 0), -1, c->left, c->top);
        }
    }
}

void StageObjects_Prepare(const ALLOCATOR *const allocator)
{
    const size_t cells = STAGE_DATA_ROWS * STAGE_DATA_COLUMNS;
    m_Storage.cards = Allocator_Alloc(allocator, cells * sizeof(STAGE_CARD));
    m_Storage.obstacles =
        Allocator_Alloc(allocator, cells * sizeof(STAGE_OBSTACLE));
    m_Storage.backgrounds = Allocator_Alloc(allocator, cells * 2 * 1024);
}

void StageObjects_Init(
    const STAGE_DATA *const data, const int16_t stage_id,
    const bool first_in_scene, const ALLOCATOR *const allocator,
    const PTN *const sprites)
{
    M_Init(data, stage_id, first_in_scene, allocator, sprites);
}

void StageObjects_InitPrepared(
    const STAGE_DATA *const data, const int16_t stage_id,
    const bool first_in_scene, const PTN *const sprites)
{
    M_Init(data, stage_id, first_in_scene, nullptr, sprites);
}

bool StageObjects_ResumeEntrance(const PTN *const sprites)
{
    if (g_StageObjects.entrance_done) {
        return true;
    }
    if (g_StageObjects.entrance_waiting && !VSyncIrq_DelayReady(1)) {
        return false;
    }
    for (;;) {
        int32_t done = 0;
        for (int32_t i = 0; i < g_StageObjects.card_count; i++) {
            STAGE_CARD *const card = &g_StageObjects.cards[i];
            if (card->entrance_frame == -1) {
                done++;
            }
            if (card->entrance_frame < 12) {
                continue;
            }
            if (card->entrance_frame % 6 == 0) {
                Sprite_PtnPut(
                    sprites,
                    StageObjects_CardImage(card->hp + 1, card->entrance_frame),
                    -1, card->left, card->top);
            }
            card->entrance_frame++;
            if (card->entrance_frame > 24) {
                card->entrance_frame = -1;
            }
        }
        if (done >= g_StageObjects.card_count) {
            g_StageObjects.entrance_done = true;
            return true;
        }
        if (g_StageObjects.entrance_total < g_StageObjects.card_count) {
            g_StageObjects.cards[g_StageObjects.entrance_total].entrance_frame =
                12;
        }
        g_StageObjects.entrance_total++;
        g_StageObjects.entrance_waiting = true;
        if (!VSyncIrq_DelayReady(1)) {
            return false;
        }
    }
}

void StageObjects_PutBackgroundAndObject(
    const PTN *const sprites, const int16_t left, const int16_t top,
    const int32_t image, const int32_t slot)
{
    const int32_t offset = M_Offset(left, top);
    for (int32_t y = 0; y < 32; y++) {
        for (int32_t x = 0; x < 32; x++) {
            const int32_t at = offset + y * VIDEO_WIDTH + x;
            const int32_t pixel = y * 32 + x;
            if (at >= 0 && at < VIDEO_PAGE_SIZE) {
                const uint8_t fg = image == STAGE_OBJECT_NONE
                    ? PTN_TRANSPARENT
                    : sprites->pixels[image * 1024 + pixel];
                g_Video.pages[g_Video.drawing_page][at] = fg == PTN_TRANSPARENT
                    ? g_StageObjects.backgrounds[slot * 1024 + pixel]
                    : fg;
            }
        }
    }
}

void StageObjects_PutAllBackgrounds(void)
{
    for (int32_t i = 0; i < g_StageObjects.card_count; i++) {
        StageObjects_PutBackgroundAndObject(
            nullptr, g_StageObjects.cards[i].left, g_StageObjects.cards[i].top,
            STAGE_OBJECT_NONE, i);
    }
    for (int32_t i = 0; i < g_StageObjects.obstacle_count; i++) {
        StageObjects_PutBackgroundAndObject(
            nullptr, g_StageObjects.obstacles[i].left,
            g_StageObjects.obstacles[i].top, STAGE_OBJECT_NONE,
            i + g_StageObjects.card_count);
    }
}

void StageObjects_CopyToBackground(void)
{
    for (int32_t i = 0; i < g_StageObjects.obstacle_count; i++) {
        M_Copy(
            g_StageObjects.obstacles[i].left, g_StageObjects.obstacles[i].top);
        Video_SelectDrawingPage(0);
    }
    for (int32_t i = 0; i < g_StageObjects.card_count; i++) {
        M_Copy(g_StageObjects.cards[i].left, g_StageObjects.cards[i].top);
        Video_SelectDrawingPage(0);
    }
}

int32_t StageObjects_CardImage(const uint8_t hp, const int16_t frame)
{
    return m_CardAnimation[hp][frame / 6];
}
