#pragma once

#include "th01/bonus/bonus.h"

typedef enum {
    BONUS_DONE,
    BONUS_STAGE_BOX,
    BONUS_CONFIRM,
} BONUS_PHASE;

typedef struct {
    BONUS_PHASE phase;
    int16_t stage_number;
    int16_t row;
    bool waiting;
} BONUS_RUN;

typedef struct {
    BONUS_RUN run;
    // Accumulates across the stage bonuses leading up to a total; Bonus_Resume
    // and Bonus_ResumeTotal clear it on confirmation. Bonus_BeginStage resets
    // only run, so this survives between stage bonus runs.
    uint32_t score_bonus;
} BONUS;

typedef enum {
    BONUS_TOTAL_DONE,
    BONUS_TOTAL_INITIAL_WAIT,
    BONUS_TOTAL_TRANSITION,
    BONUS_TOTAL_STAGE_WAIT,
    BONUS_TOTAL_PREPARE_METRIC,
    BONUS_TOTAL_DIGIT,
    BONUS_TOTAL_DIGIT_WAIT,
    BONUS_TOTAL_METRIC_WAIT,
    BONUS_TOTAL_CONFIRM,
} BONUS_TOTAL_PHASE;

typedef struct {
    BONUS_TOTAL_PHASE phase;
    uint64_t last_tick;
    uint32_t credit;
    int16_t stage_number;
    int16_t digits[6];
    int16_t top;
    int16_t metric;
    int16_t place;
    int16_t digit_count;
    int16_t spin;
} BONUS_TOTAL;

typedef struct {
    uint16_t transferred[VIDEO_HEIGHT];
    bool active[VIDEO_HEIGHT];
    uint16_t activated;
    uint16_t done;
    uint16_t previous_done;
    bool pending;
    bool waiting;
} BONUS_ROW_TRANSITION;

// Writable view of the bonus run/total state for tests.
extern BONUS *const p_Bonus;
extern BONUS_TOTAL *const p_Total;
// Writable view of the bonus row transition for tests.
extern BONUS_ROW_TRANSITION *const p_RowTransition;
