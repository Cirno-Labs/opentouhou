#pragma once

#include "th01/boss_hit.h"
#include "th01/boss_render.h"
#include "th01/grc.h"
#include "pc98/mdrv_data.h"
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
    MDRV_MUSIC music, second_music;
    MDRV_EFFECTS effects;
    const INTERLEAVE_ASSETS *interleave;
    const uint8_t *star, *flake, *spark, *leaf_left, *leaf_right;
} B20M_ASSETS;

typedef enum {
    B20M_AUDIO_LOAD_MUSIC,
    B20M_AUDIO_LOAD_SECOND_MUSIC,
    B20M_AUDIO_LOAD_EFFECTS,
    B20M_AUDIO_PLAY,
    B20M_AUDIO_STOP,
    B20M_AUDIO_FADE,
} B20M_AUDIO_ACTION;

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
    BOSS_ENTITY shield;
    BOSS_ANIMATION dress, wand;
    BOSS_HIT hit;
    PALETTE boss_palette, stage_palette;
    uint8_t *wand_background;
    B20M_BIRDS birds;
    B20M_PARTICLES vertical, horizontal;
    B20M_WAVES waves;
    B20M_LEAVES leaves;
    B20M_RAYS bird_rays, stack_rays, seal_rays;
    B20M_SPRAY sprays[2];
    int16_t vortex_left[2], vortex_top[2], vortex_previous_left[2],
        vortex_previous_top[2];
    int16_t laser_x[10], laser_y[10], egg_left[30], egg_top[30];
    int16_t spawner_left, spawner_top, spawner_velocity_x, spawner_velocity_y;
    int16_t flake_state, flake_left, flake_top, flake_velocity_x,
        flake_velocity_y;
    int16_t star_left, star_top, radius_outer_1, radius_outer_2, radius_inner;
    int16_t hp, frame, pattern, pattern_state, patterns_done,
        patterns_until_next;
    int16_t entrance_radius, wand_frame, eggs_alive, bird_pellet_group;
    int16_t ray_previous_x, ray_previous_y;
    int16_t laser_origin_x, laser_origin_y, laser_velocity_x, laser_velocity_y,
        laser_target;
    int16_t gust_x, gust_y, gust_offset, gust_color, gust_id;
    int16_t dotted_radius, rings_interval, raw_scroll, scroll_line;
    PAGE_TRANSITION transition;
    FRAME_WAIT wait;
    uint64_t wait_tick;
    uint16_t wait_frames;
    uint8_t rings_angle, radial_angle, radial_velocity, wait_kind, sequence,
        sequence_stage, resume_stage;
    int8_t phase, call_phase;
    bool vortex_wand_done, bird_wand_done, direction_first, direction_second;
    bool hp_bar_filled, dotted_active, entrance_active, finished;
} B20M;

typedef struct {
    const B20M_ASSETS *assets;
    BOSS_SPRITES *sprites;
    const BOSS_HIT_CONTEXT *hit;
    PELLETS *pellets;
    PELLET_SHARED *pellet_shared;
    SHOOTOUT_LASERS *lasers;
    STAGE_OBJECTS *objects;
    TRAM *tram;
    GAME_INPUT *input;
    STAGE_TIMER *timer;
    bool *first_stage;
    const bool *timer_initialized;
    uint32_t *rng;
    bool *game_cleared;
    void (*audio)(
        void *user, B20M_AUDIO_ACTION action, const B20M_ASSETS *assets);
} B20M_CONTEXT;

// Loads both forms, backgrounds, music, effects and executable masks into
// the caller's immutable asset group. Scratch is needed only while loading.
RESULT B20M_LoadAssets(
    VFS *vfs, uint8_t *scratch, size_t scratch_size, const ALLOCATOR *allocator,
    B20M_ASSETS *out);
RESULT B20M_ValidateAssets(const B20M_ASSETS *assets);

// Initializes one program and allocates its 128x96 wand snapshot from the
// caller's group. Retain the group while this boss is active.
void B20M_Init(B20M *boss, const ALLOCATOR *allocator);

// Binds actor images, resets the first form and retains pattern history.
// Run before the separate cached entrance. Performs no allocation or IO.
void B20M_Setup(B20M *boss, const B20M_CONTEXT *context);
RESULT B20M_ValidateContext(const B20M_CONTEXT *context);

// Starts the cached load/scroll/diagonal entrance. Advance reference VSyncs
// and a monotonic 60 Hz host serial while polling; pending polls stay inert.
void B20M_BeginEntrance(B20M *boss, const B20M_CONTEXT *context);
bool B20M_ResumeEntrance(
    B20M *boss, const B20M_CONTEXT *context, FRAME_VSYNC *vsync,
    uint64_t host_tick);

// Runs one battle call or resumes its nested waits. False suspends the world.
// Implements both forms and the final 50,000-point bonus and game clear.
// Quantizes the bird safety delay to six 60 Hz host ticks. Audio/effect
// consumers may post events but must not mutate borrowed simulation state.
// Borrows stable context bindings and allocates nothing and performs no IO.
bool B20M_UpdateAndRender(
    B20M *boss, const B20M_CONTEXT *context, FRAME_VSYNC *vsync,
    uint64_t host_tick);
