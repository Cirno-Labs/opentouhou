#pragma once

#include "th01/boss/private.h"
#include "th01/grc.h"
#include "th01/interleave.h"
#include "th01/wait.h"

#include "pc98/zun/pi.h"

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

// Writable view of the b20m state for tests.
extern B20M *const p_B20M;

// Sariel's lifecycle, for tests that look it up directly.
extern const BOSS_STRATEGY g_BossSariel;

// Checks that assets loaded or faked for Sariel are usable.
RESULT B20M_ValidateAssets(const B20M_ASSETS *a);
