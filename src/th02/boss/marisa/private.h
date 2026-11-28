#pragma once

#include "th02/boss/marisa.h"
#include "th02/boss/marisa/assets.h"
#include "th02/entity.h"

#include "core/base/types.h"

#define MARISA_BIT_COUNT 4
#define MARISA_DOT_COUNT 30

// One of Marisa's trailing star-dust dots, drawn behind her sprite.
typedef struct {
    uint8_t flag;
    uint8_t angle;
    ENTITY_POINT position[2];
    uint8_t size;
    uint8_t speed;
} MARISA_DOT;

// Stage 4 boss Marisa's own state: her dust trail, orbiting bits and
// which fire pattern the current combat cycle runs, plus the stage 4
// midboss she shares a sprite bank and fields with.
typedef struct {
    MARISA_DOT dots[MARISA_DOT_COUNT];
    ENTITY_POINT position;
    int16_t phase;
    int16_t direction;
    int16_t midboss_bullet_top;
    int16_t midboss_kill_frame;
    uint8_t midboss_pattern;
    uint8_t midboss_patterns_done;
    uint8_t midboss_page;
    bool midboss_active;
    ENTITY_POINT bit_position[2][MARISA_BIT_COUNT];
    ENTITY_POINT orbit_center;
    int16_t bit_hits[MARISA_BIT_COUNT];
    int16_t bit_state[MARISA_BIT_COUNT];
    int16_t bit_hit[MARISA_BIT_COUNT];
    int16_t bit_flash[MARISA_BIT_COUNT];
    int16_t bit_radius[MARISA_BIT_COUNT];
    int16_t bit_speed[MARISA_BIT_COUNT];
    int16_t bit_state_sum;
    int16_t defeat_frame;
    int16_t move_x;
    int16_t move_y;
    uint8_t bit_angle[MARISA_BIT_COUNT];
    uint8_t bit_spin[MARISA_BIT_COUNT];
    uint8_t damage_enable;
    uint8_t drift_toggle;
    uint8_t double_fire;
    uint8_t double_angle;
    uint8_t drift_angle;
    uint8_t orbit_angle, expand_angle, spin_angle, ring_toggle,
        background_color;
    uint8_t fan, second_fan, unused_fan, edge_count, pattern_rounds,
        bit_patterns_seen, bomb_mask;
    int8_t edge_direction;
    int8_t orbit_direction;
    int8_t color_index;
    int8_t pattern;
    int8_t pattern_cycle;
} MARISA;

// Writable view of the module's private state, for tests.
extern MARISA *const p_Marisa;

// Places Marisa at her entrance, before the first Marisa_Begin.
void Marisa_Place(BOSS_STATE *boss);

// Starts Marisa's fight: resets damage, bits and her fire pattern cycle
// for rank.
void Marisa_Begin(BOSS_STATE *boss, uint8_t rank);

// Advances Marisa one frame: moves, fires and checks for defeat.
BOSS_RESULT Marisa_Update(
    BOSS_STATE *boss, const PATTERN_BANK *patterns, uint8_t rank,
    bool reduce_effects);

// Restores the scrolled-out tiles Marisa's sprite and dust trail marked
// dirty.
void Marisa_Invalidate(BOSS_STATE *boss, bool reduce_effects);

// Advances the stage 4 midboss one frame.
void Marisa_MidbossUpdate(
    BOSS_STATE *boss, const PATTERN_BANK *patterns, uint8_t rank);

// Restores the midboss's scrolled-out tiles; reports whether it is still
// active.
bool Marisa_MidbossInvalidate(BOSS_STATE *boss);

// Draws Marisa's own two-sprite body, or its hit flash.
void Marisa_DrawBody(BOSS_STATE *boss, const PATTERN_BANK *patterns);

// Fills patterns with the combat or summon set from assets, recoloring
// them if assets names a palette.
void Marisa_SelectPatterns(
    PATTERN_BANK *patterns, const MARISA_ASSETS *assets, bool combat);
