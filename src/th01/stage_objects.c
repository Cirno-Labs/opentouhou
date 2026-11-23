#include "th01/stage_objects.h"

#include "pc98/zun/master_rng.h"
#include "th01/sprite.h"

#include <string.h>

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
    STAGE_OBJECTS *const objects, VIDEO *const video, const int16_t left,
    const int16_t top, const int32_t slot)
{
    Video_SelectDrawingPage(video, 1);
    const int32_t offset = M_Offset(left, top);
    for (int32_t y = 0; y < 32; y++) {
        for (int32_t x = 0; x < 32; x++) {
            const int32_t at = offset + y * VIDEO_WIDTH + x;
            if (at >= 0 && at < VIDEO_PAGE_SIZE) {
                objects->backgrounds[slot * 1024 + y * 32 + x] =
                    video->pages[1][at];
            }
        }
    }
    Video_SelectDrawingPage(video, 0);
}

static void M_Copy(VIDEO *const video, const int16_t left, const int16_t top)
{
    const int32_t offset = M_Offset(left, top);
    for (int32_t y = 0; y < 32; y++) {
        for (int32_t x = 0; x < 32; x++) {
            const int32_t at = offset + y * VIDEO_WIDTH + x;
            if (at >= 0 && at < VIDEO_PAGE_SIZE) {
                video->pages[1][at] = video->pages[0][at];
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
    STAGE_OBJECTS *const objects, const STAGE_DATA *const data,
    const int16_t stage_id, const bool first_in_scene,
    const ALLOCATOR *const allocator, STAGE_OBJECT_STORAGE *const storage,
    VIDEO *const video, const PTN *const sprites, uint32_t *const rng)
{
    *objects = (STAGE_OBJECTS) {
        .entrance_done = true,
        .vertical_bar_active = objects->vertical_bar_active,
        .portal_active = objects->portal_active,
        .entered_portal = objects->entered_portal,
        .destination_left = objects->destination_left,
        .destination_top = objects->destination_top,
    };
    if (stage_id % 5 == 4) {
        return;
    }
    const STAGE_DATA_STAGE *const layout = &data->stages[stage_id % 5];
    for (int32_t y = 0; y < 10; y++) {
        for (int32_t x = 0; x < 20; x++) {
            objects->card_count += layout->cells[y][x].card;
            objects->obstacle_count +=
                layout->cells[y][x].obstacle != STAGE_DATA_OBSTACLE_NONE;
        }
    }
    if (storage != nullptr) {
        objects->cards = storage->cards;
        objects->obstacles = storage->obstacles;
        objects->backgrounds = storage->backgrounds;
        memset(
            objects->cards, 0,
            (size_t)objects->card_count * sizeof(STAGE_CARD));
        memset(
            objects->obstacles, 0,
            (size_t)objects->obstacle_count * sizeof(STAGE_OBSTACLE));
        memset(
            objects->backgrounds, 0,
            (size_t)(objects->card_count + objects->obstacle_count) * 1024);
    } else {
        if (objects->card_count + objects->obstacle_count != 0) {
            objects->backgrounds = Allocator_AllocZ(
                allocator,
                (size_t)(objects->card_count + objects->obstacle_count) * 1024);
        }
        if (objects->card_count != 0) {
            objects->cards = Allocator_AllocZ(
                allocator, (size_t)objects->card_count * sizeof(STAGE_CARD));
        }
        if (objects->obstacle_count != 0) {
            objects->obstacles = Allocator_AllocZ(
                allocator,
                (size_t)objects->obstacle_count * sizeof(STAGE_OBSTACLE));
        }
    }
    MasterRng_Next(rng);
    int32_t card = 0;
    for (int32_t y = 0; y < 10; y++) {
        for (int32_t x = 0; x < 20; x++) {
            if (layout->cells[y][x].card) {
                objects->cards[card] = (STAGE_CARD) {
                    .left = (int16_t)(x * 32),
                    .top = (int16_t)(y * 32 + 64),
                    .hp = layout->cells[y][x].card_extra_hp,
                };
                M_Snapshot(
                    objects, video, objects->cards[card].left,
                    objects->cards[card].top, card);
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
                objects->obstacles[obstacle] =
                    (STAGE_OBSTACLE) { .left = left, .top = top, .type = type };
                M_Snapshot(
                    objects, video, left, top, obstacle + objects->card_count);
                Sprite_PtnPut(
                    video, sprites, M_ObstacleImage(type), -1, left, top);
                obstacle++;
            }
        }
    }
    objects->entrance_done = first_in_scene;
    if (first_in_scene) {
        for (int32_t i = 0; i < objects->card_count; i++) {
            const STAGE_CARD *const c = &objects->cards[i];
            Sprite_PtnPut(
                video, sprites, StageObjects_CardImage(c->hp, 0), -1, c->left,
                c->top);
        }
    }
}

void StageObjects_Prepare(
    STAGE_OBJECT_STORAGE *const storage, const ALLOCATOR *const allocator)
{
    const size_t cells = STAGE_DATA_ROWS * STAGE_DATA_COLUMNS;
    *storage = (STAGE_OBJECT_STORAGE) {
        .cards = Allocator_Alloc(allocator, cells * sizeof(STAGE_CARD)),
        .obstacles = Allocator_Alloc(allocator, cells * sizeof(STAGE_OBSTACLE)),
        .backgrounds = Allocator_Alloc(allocator, cells * 2 * 1024),
    };
}

void StageObjects_Init(
    STAGE_OBJECTS *const objects, const STAGE_DATA *const data,
    const int16_t stage_id, const bool first_in_scene,
    const ALLOCATOR *const allocator, VIDEO *const video,
    const PTN *const sprites, uint32_t *const rng)
{
    M_Init(
        objects, data, stage_id, first_in_scene, allocator, nullptr, video,
        sprites, rng);
}

void StageObjects_InitPrepared(
    STAGE_OBJECTS *const objects, const STAGE_DATA *const data,
    const int16_t stage_id, const bool first_in_scene,
    STAGE_OBJECT_STORAGE *const storage, VIDEO *const video,
    const PTN *const sprites, uint32_t *const rng)
{
    M_Init(
        objects, data, stage_id, first_in_scene, nullptr, storage, video,
        sprites, rng);
}

bool StageObjects_ResumeEntrance(
    STAGE_OBJECTS *const objects, VIDEO *const video, const PTN *const sprites,
    FRAME_VSYNC *const vsync)
{
    if (objects->entrance_done) {
        return true;
    }
    if (objects->entrance_waiting && !Frame_DelayReady(vsync, 1)) {
        return false;
    }
    for (;;) {
        int32_t done = 0;
        for (int32_t i = 0; i < objects->card_count; i++) {
            STAGE_CARD *const card = &objects->cards[i];
            if (card->entrance_frame == -1) {
                done++;
            }
            if (card->entrance_frame < 12) {
                continue;
            }
            if (card->entrance_frame % 6 == 0) {
                Sprite_PtnPut(
                    video, sprites,
                    StageObjects_CardImage(card->hp + 1, card->entrance_frame),
                    -1, card->left, card->top);
            }
            card->entrance_frame++;
            if (card->entrance_frame > 24) {
                card->entrance_frame = -1;
            }
        }
        if (done >= objects->card_count) {
            objects->entrance_done = true;
            return true;
        }
        if (objects->entrance_total < objects->card_count) {
            objects->cards[objects->entrance_total].entrance_frame = 12;
        }
        objects->entrance_total++;
        objects->entrance_waiting = true;
        if (!Frame_DelayReady(vsync, 1)) {
            return false;
        }
    }
}

void StageObjects_PutBackgroundAndObject(
    const STAGE_OBJECTS *const objects, VIDEO *const video,
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
                video->pages[video->drawing_page][at] = fg == PTN_TRANSPARENT
                    ? objects->backgrounds[slot * 1024 + pixel]
                    : fg;
            }
        }
    }
}

void StageObjects_PutAllBackgrounds(
    const STAGE_OBJECTS *const objects, VIDEO *const video)
{
    for (int32_t i = 0; i < objects->card_count; i++) {
        StageObjects_PutBackgroundAndObject(
            objects, video, nullptr, objects->cards[i].left,
            objects->cards[i].top, STAGE_OBJECT_NONE, i);
    }
    for (int32_t i = 0; i < objects->obstacle_count; i++) {
        StageObjects_PutBackgroundAndObject(
            objects, video, nullptr, objects->obstacles[i].left,
            objects->obstacles[i].top, STAGE_OBJECT_NONE,
            i + objects->card_count);
    }
}

void StageObjects_CopyToBackground(
    const STAGE_OBJECTS *const objects, VIDEO *const video)
{
    for (int32_t i = 0; i < objects->obstacle_count; i++) {
        M_Copy(video, objects->obstacles[i].left, objects->obstacles[i].top);
        Video_SelectDrawingPage(video, 0);
    }
    for (int32_t i = 0; i < objects->card_count; i++) {
        M_Copy(video, objects->cards[i].left, objects->cards[i].top);
        Video_SelectDrawingPage(video, 0);
    }
}

int32_t StageObjects_CardImage(const uint8_t hp, const int16_t frame)
{
    return m_CardAnimation[hp][frame / 6];
}
