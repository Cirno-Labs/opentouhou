#pragma once

#include "th02/input.h"
#include "th02/stage_bonus/assets.h"

typedef enum {
    BONUS_RANK,
    BONUS_PERFORMANCE,
    BONUS_BOMBS,
    BONUS_MISSES,
    BONUS_START_BOMBS,
    BONUS_START_LIVES,
    BONUS_EXTRA_CLEAR,
    BONUS_EXTRA_MISSES,
    BONUS_EXTRA_BOMBS,
    BONUS_EXTRA_TIME,
} STAGE_BONUS_LABEL;

typedef struct {
    int8_t rank;
    int16_t performance;
    int16_t point_items;
    uint8_t bombs_used;
    uint8_t misses;
    uint8_t start_bombs;
    uint8_t start_lives;
    uint32_t sigma_frames;
} STAGE_BONUS_RUN;

// Draws the bonus screen; a null view draws nothing.
typedef struct {
    void (*begin)(void *context, bool extra);
    void (*row)(void *context, STAGE_BONUS_LABEL label, int16_t value);
    void (*multiply)(
        void *context, int16_t point_items, int32_t sum_x10, int32_t total_x10,
        uint8_t digits);
    void *context;
} STAGE_BONUS_VIEW;

typedef struct {
    int16_t sum;
    int32_t amount;
    uint16_t keys;
} STAGE_BONUS_RESULT;

// Clears the bonus wait state, once at startup.
void StageBonus_Init(void);

// Awards the stage bonus with retail's signed wrap; keys holds Up, the
// mask the release/press wait senses. Then starts the given number of
// release/press waits.
STAGE_BONUS_RESULT StageBonus_Grant(
    const STAGE_BONUS_RUN *run, bool extra, const STAGE_BONUS_VIEW *view);

// Awards the bonus, then starts the given number of release/press waits;
// returns the amount and the sensed keys.
STAGE_BONUS_RESULT StageBonus_Begin(
    const STAGE_BONUS_ASSETS *assets, const STAGE_BONUS_RUN *run, bool extra,
    uint8_t waits, const TH02_INPUT *input);

// Runs one wait frame, ignoring movement and Space; returns true when done.
bool StageBonus_Update(const TH02_INPUT *input);
