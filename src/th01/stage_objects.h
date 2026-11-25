#pragma once

#include "th01/vsync_irq.h"
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

extern STAGE_OBJECTS g_StageObjects;

// Prepares storage for every cell of a normalized layout.
// Keep the allocator's group alive across all stages.
void StageObjects_Prepare(const ALLOCATOR *allocator);

// Instantiates as Init does, in storage from Prepare, without allocating.
void StageObjects_InitPrepared(
    const STAGE_DATA *data, int16_t stage_id, bool first_in_scene,
    const PTN *sprites);

// Validates immutable sprites before initialization. Ordinary layouts need
// 32 images; boss slot 4 accepts the reduced seven-image stage set.
RESULT StageObjects_ValidateSprites(const PTN *sprites, int16_t stage_id);

// Instantiates the stage's cards and obstacles, drawing them at once for
// first_in_scene and otherwise starting the one-card-per-frame entrance.
// Allocates from a stage-lifetime group; release it between stages.
void StageObjects_Init(
    const STAGE_DATA *data, int16_t stage_id, bool first_in_scene,
    const ALLOCATOR *allocator, const PTN *sprites);

// Runs the entrance until the next frame delay; returns true when complete.
bool StageObjects_ResumeEntrance(const PTN *sprites);

// Redraws a 32x32 square as its background snapshot plus the object's
// opaque pixels; NONE restores only the snapshot.
void StageObjects_PutBackgroundAndObject(
    const PTN *sprites, int16_t left, int16_t top, int32_t image, int32_t slot);
void StageObjects_PutAllBackgrounds(void);

// Copies obstacles, then cards, from page 0 to page 1. Leaves drawing
// on page 0 after any copy. Zero-object/boss calls preserve selectors.
void StageObjects_CopyToBackground(void);
int32_t StageObjects_CardImage(uint8_t hp, int16_t frame);
