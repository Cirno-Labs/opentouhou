#pragma once

#include "th01/boss_hit.h"
#include "th01/boss_entity.h"
#include "th01/grc.h"
#include "pc98/zun/pi.h"
#include "th01/pellets.h"
#include "th01/shootout_lasers.h"
#include "th01/stage_objects.h"
#include "th01/tram.h"
#include "th01/timer.h"
#include "th01/transition.h"

typedef struct {
    BOS shield, wand, dress;
    GRC vortex, bird, cross, splash;
    PI_IMAGE lower, upper, backgrounds[6];
    const INTERLEAVE_ASSETS *interleave;
    const uint8_t *star, *flake, *spark, *leaf_left, *leaf_right;
} B20M_ASSETS;

typedef struct {
    double left[30], top[30], velocity_x[30], velocity_y[30];
    int8_t reserved[30], hatch_time[30], hatch_frames[30];
    bool alive[30];
} B20M_BIRDS;
typedef struct {
    double left[30], top[30], velocity[30];
    int8_t color[30];
} B20M_PARTICLES;
typedef struct {
    int16_t left[30], top[30], velocity[30], age[30];
    int8_t color[30];
} B20M_WAVES;
typedef struct {
    int16_t left[30], top[30], velocity_x[30], velocity_y[30], flag[30];
} B20M_LEAVES;
typedef struct {
    int16_t x, y, velocity_x, velocity_y, image, previous_image;
} B20M_RAYS;
typedef struct {
    int16_t x, y, speed, subpattern;
    uint8_t angle;
} B20M_SPRAY;
typedef struct {
    int16_t left[2], top[2], previous_left[2], previous_top[2];
} B20M_VORTICES;
typedef struct {
    int16_t x[10], y[10];
    int16_t origin_x, origin_y, velocity_x, velocity_y, target;
} B20M_LASER;
typedef struct {
    int16_t left[30], top[30], alive;
} B20M_EGGS;
typedef struct {
    int16_t left, top, velocity_x, velocity_y;
} B20M_SPAWNER;
typedef struct {
    int16_t state, left, top, velocity_x, velocity_y;
} B20M_FLAKE;
typedef struct {
    int16_t left, top, radius_outer_1, radius_outer_2, radius_inner;
} B20M_STAR;
typedef struct {
    int16_t x, y, offset, color, id;
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
    BOSS_ANIMATION dress, wand;
    BOSS_HIT hit;
    PALETTE boss_palette, stage_palette;
    uint8_t wand_background[128 * 96];
    B20M_BIRDS birds;
    B20M_PARTICLES vertical, horizontal;
    B20M_WAVES waves;
    B20M_LEAVES leaves;
    B20M_RAYS bird_rays, stack_rays, seal_rays;
    B20M_SPRAY sprays[2];
    B20M_VORTICES vortices;
    B20M_LASER laser;
    B20M_EGGS eggs;
    B20M_SPAWNER spawner;
    B20M_FLAKE flake;
    B20M_STAR star;
    B20M_GUST gust;
    int16_t hp, frame, pattern, pattern_state, patterns_done,
        patterns_until_next;
    int16_t entrance_radius, wand_frame, bird_pellet_group;
    int16_t ray_previous_x, ray_previous_y;
    int16_t dotted_radius, rings_interval, raw_scroll, scroll_line;
    FRAME_WAIT wait;
    uint64_t wait_tick;
    uint16_t wait_frames;
    uint8_t rings_angle, radial_angle, radial_velocity, wait_kind, sequence,
        sequence_stage, resume_stage;
    int8_t phase, call_phase;
    bool vortex_wand_done, bird_wand_done, direction_first, direction_second;
    bool hp_bar_filled, dotted_active, entrance_active, finished;
} B20M;

extern B20M g_B20M;

// Loads both forms, backgrounds and the masks of ileave_m.bmp,
// shape8x8.bmp, leaf_s.bmp, leaf_l.bmp and leaf_r.bmp into the caller's
// immutable asset group.
RESULT B20M_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, B20M_ASSETS *out);
RESULT B20M_ValidateAssets(const B20M_ASSETS *assets);

// Resets the boss state, before the first fight of a game.
void B20M_Init(void);

// Binds actor images, resets the first form and retains pattern history.
// Run before the separate cached entrance. Performs no allocation or IO.
void B20M_Setup(const B20M_ASSETS *assets);

// Starts the cached load/scroll/diagonal entrance. Advance reference VSyncs
// and a monotonic 60 Hz host serial while polling; pending polls stay inert.
void B20M_BeginEntrance(const B20M_ASSETS *assets);
bool B20M_ResumeEntrance(const B20M_ASSETS *assets, uint64_t host_tick);

// Runs one battle call or resumes its nested waits. False suspends the world.
// Implements both forms and the final 50,000-point bonus and game clear.
// Quantizes the bird safety delay to six 60 Hz host ticks.
// Borrows stable context bindings and allocates nothing and performs no IO.
bool B20M_UpdateAndRender(const B20M_ASSETS *assets, uint64_t host_tick);
