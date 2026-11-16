#pragma once

#include "th01/frame.h"
#include "th01/orb.h"
#include "th01/stage_data.h"

#define STAGE_OBJECT_NONE (-1)

typedef enum {
    STAGE_CARD_ALIVE,
    STAGE_CARD_FLIPPING,
    STAGE_CARD_REMOVED,
} STAGE_CARD_FLAG;

typedef struct {
    int16_t left, top, flip_frame, entrance_frame;
    uint8_t flag, hp;
    uint32_t score;
} STAGE_CARD;

typedef struct {
    int16_t left, top, frame;
    STAGE_DATA_OBSTACLE type;
    uint8_t turret_flag;
} STAGE_OBSTACLE;

typedef struct {
    STAGE_CARD *cards;
    STAGE_OBSTACLE *obstacles;
    uint8_t *backgrounds;
    int32_t card_count, obstacle_count, entrance_total;
    bool entrance_waiting, entrance_done;
    bool vertical_bar_active, portal_active;
    int32_t entered_portal;
    int16_t destination_left, destination_top;
} STAGE_OBJECTS;

// Validates immutable sprites before initialization. Ordinary layouts need
// 32 images; boss slot 4 accepts the reduced seven-image stage set.
RESULT StageObjects_ValidateSprites(const PTN *sprites, int16_t stage_id);

// Zero-initialize objects once and reuse it across stage changes. Preserve
// obstacle static history, including stale portal destinations after reset
// and across boss loads. Instantiates the normalized layout in source order.
// Requires nonnegative stage_id and validated sprites. All
// mutable objects, entrance timers and raw background snapshots belong to
// the supplied stage-lifetime allocator group. Release that group between
// stages; there are no individual frees. Snapshot cards before obstacles,
// from page 1 before drawing, selecting page 1 then page 0 per snapshot.
// Draw obstacles on page 0, then cards immediately for first_in_scene.
// Otherwise begin the cooperative one-card-per-frame entrance animation.
// Boss slot 4 returns empty before allocation or RNG. An ordinary empty
// card layout consumes the unused random draw without dividing by zero.
// Uses count-sized timers and snapshots, extending the original 50-card
// entrance and 102-background DOS limits safely for debug layouts.
void StageObjects_Init(
    STAGE_OBJECTS *objects, const STAGE_DATA *data, int16_t stage_id,
    bool first_in_scene, const ALLOCATOR *allocator, VIDEO *video,
    const PTN *sprites, uint32_t *rng);

// Runs until the next frame_delay(1), or returns true when complete.
// The first call runs before waiting; subsequent calls require count1 >= 1
// as unsigned 16 bits and reset count1 on success. Repeated polls without
// readiness do nothing. No update allocates. Preserve objects until done.
bool StageObjects_ResumeEntrance(
    STAGE_OBJECTS *objects, VIDEO *video, const PTN *sprites,
    FRAME_VSYNC *vsync);

// Replaces the full 32x32 square with the raw snapshot plus opaque sprite
// pixels. NONE restores only the snapshot. Slot order is cards, obstacles.
// Uses the drawing page and eight-pixel X alignment, with linear row wrap
// and outside-page writes discarded, as in the other native PTN helpers.
void StageObjects_PutBackgroundAndObject(
    const STAGE_OBJECTS *objects, VIDEO *video, const PTN *sprites,
    int16_t left, int16_t top, int32_t image, int32_t slot);
void StageObjects_PutAllBackgrounds(const STAGE_OBJECTS *objects, VIDEO *video);

// Copies obstacles, then cards, from page 0 to page 1. Leaves drawing
// on page 0 after any copy. Zero-object/boss calls preserve selectors.
void StageObjects_CopyToBackground(const STAGE_OBJECTS *objects, VIDEO *video);
int32_t StageObjects_CardImage(uint8_t hp, int16_t frame);
