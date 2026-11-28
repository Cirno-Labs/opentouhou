#pragma once

#include "th01/shootout_lasers.h"

typedef struct {
    int32_t origin_left;
    int32_t origin_y;
    int32_t start_left;
    int32_t start_y;
    int32_t cursor_left;
    int32_t cursor_y;
    int32_t step_x;
    int32_t step_y;
    int16_t length;
    int16_t tail_speed;
    int16_t extend_speed;
    int16_t age;
    int16_t detach_age;
    uint8_t color;
    uint8_t width_mask;
    bool alive;
    bool damaging;
} SHOOTOUT_LASER;

typedef struct {
    SHOOTOUT_LASER slots[SHOOTOUT_LASERS_COUNT];
} SHOOTOUT_LASERS;

// Writable view of the shootout lasers for tests.
extern SHOOTOUT_LASERS *const p_Lasers;
