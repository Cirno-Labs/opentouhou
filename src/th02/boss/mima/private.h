#pragma once

#include "th02/boss.h"
#include "th02/boss/mima/assets.h"
#include "th02/boss/talk.h"
#include "th02/dialogue/box.h"
#include "th02/dialogue/sequence.h"
#include "th02/dialogue/summon.h"
#include "th02/entity.h"
#include "th02/wait.h"

#include "core/base/types.h"

// Shared between Mima's combat (logic.c) and her form-change script
// (transition.c); nothing outside the mima/ directory sees these.

typedef enum {
    MIMA_TRANSITION_WAIT,
    MIMA_TRANSITION_FORM,
    MIMA_TRANSITION_POST,
    MIMA_TRANSITION_ENDED,
} MIMA_TRANSITION;

#define MIMA_SPHERE_COUNT 8

// Stage 5 boss Mima's attack phase, through her four forms.
typedef enum {
    MIMA_APPEAR,
    MIMA_ATTACK_ONE,
    MIMA_CHANGE_TWO,
    MIMA_ATTACK_TWO,
    MIMA_CHANGE_THREE,
    MIMA_ATTACK_THREE,
    MIMA_CHANGE_FOUR,
    MIMA_ATTACK_FOUR,
    MIMA_FORM,
    MIMA_ATTACK_FINAL,
    MIMA_POST,
} MIMA_PHASE;

// Stage 5 boss Mima's own state: her current form, orbiting spheres and
// which fire pattern the current attack cycle runs.
typedef struct {
    ENTITY_POINT shot_origin;
    ENTITY_POINT pellet_origin;
    ENTITY_POINT lower_origin;
    ENTITY_POINT ball_origin;
    int16_t damage_enable;
    int16_t phase;
    int16_t pattern;
    int16_t inactive_spheres;
    int16_t pattern_cycles;
    ENTITY_POINT sphere_position[2][MIMA_SPHERE_COUNT];
    int16_t sphere_active[MIMA_SPHERE_COUNT];
    uint8_t form, background_radius, disc_radius, ring_color, trail_color,
        disc_color, flash_cycle, beam_angle, beam_spread, beam_tone,
        orbit_burst_mode;
    ENTITY_POINT orbit_center;
    ENTITY_POINT aimed_velocity;
    ENTITY_POINT twist_velocity;
    ENTITY_POINT expand_center;
    int16_t orbit_radius;
    int16_t expand_radius;
    uint8_t orbit_angle, orbit_spinning, spiral_angle, spiral_speed, fan_angle,
        star_angle, cross_angle, expand_angle, ball_angle, cue_angle,
        cue_radius;
    int8_t orbit_direction;
    int8_t star_direction;
    int16_t damage_threshold;
    int16_t max_patterns;
    int16_t pattern_count;
    int16_t min_patterns;
    uint8_t effect_radius;
    uint8_t disc_base_radius;
    uint8_t disc_pulse;
    uint8_t background_angle;
    int16_t move_y;
    uint8_t ring;
    uint8_t groups[2];
    uint8_t ball_group;
    uint8_t angle_step;
    bool transition_started;
    bool ended;
} MIMA;

// Mima's own talk sequence phase, through the pre-fight reveal and her
// two form-change scripts.
typedef enum {
    MIMA_TALK_ENTER,
    MIMA_TALK_INTRO,
    MIMA_TALK_APPEARANCE_BEGIN,
    MIMA_TALK_APPEARANCE,
    MIMA_TALK_COMMON,
    MIMA_TALK_BRANCH_BEGIN,
    MIMA_TALK_BRANCH,
    MIMA_TALK_WINGED_BEGIN,
    MIMA_TALK_WINGED,
    MIMA_TALK_LEAVE,
    MIMA_TALK_FORM_BEGIN,
    MIMA_TALK_FORM_BOXES,
    MIMA_TALK_FORM_END,
    MIMA_TALK_POST_RUN,
    MIMA_TALK_FLASH_FORCE,
    MIMA_TALK_FADE,
    MIMA_TALK_DELAY,
    MIMA_TALK_DONE,
} MIMA_TALK_PHASE;

// transition.c's talk sequence state: the boxes, summon and wait it steps
// through between forms.
typedef struct {
    DIALOGUE_SEQUENCE sequence;
    DIALOGUE_SUMMON summon;
    DIALOGUE_RUNTIME runtime;
    WAIT wait;
    MIMA_TALK_PHASE phase;
    MIMA_TALK_PHASE after_wait;
    uint16_t continues;
    uint8_t faces[2];
    uint8_t operation;
    bool box_active;
} MIMA_TALK;

// Writable view of logic.c's private state, for tests.
extern MIMA *const p_Mima;

// Writable view of transition.c's private state, for tests.
extern MIMA_TALK *const p_Talk;

// Copies Mima's boss slots into patterns and imports their palette without
// showing it; missing banks keep the current patterns.
void Mima_SelectPatterns(
    PATTERN_BANK *patterns, const MIMA_ASSETS *assets, MIMA_BANK bank);

// Places Mima at (144,64) with sprite 128.
void Mima_Place(BOSS_STATE *boss);

// Draws Mima's body, flashing green on every fourth pending hit_flash.
void Mima_DrawBody(BOSS_STATE *boss, const PATTERN_BANK *patterns);

// Starts combat: resets damage and the first form's state.
void Mima_BeginScene(BOSS_STATE *boss, uint8_t rank);

// Advances Mima one frame: moves, fires and checks for defeat or a form
// change.
BOSS_RESULT Mima_Update(
    BOSS_STATE *boss, const PATTERN_BANK *patterns, uint8_t rank,
    bool reduce_effects);

// Restores the scrolled-out tiles Mima's sprite and effects marked dirty.
void Mima_Invalidate(BOSS_STATE *boss, bool reduce_effects);

// Starts a form change once; returns the script to run, WAIT while it runs
// and ENDED after the ending handoff.
MIMA_TRANSITION Mima_BeginTransition(
    BOSS_STATE *boss, const PATTERN_BANK *patterns);

// Enters the final form only without continues; otherwise awards the ending
// score and returns BOSS_FINISHED.
BOSS_RESULT Mima_EndTransition(BOSS_STATE *boss, bool continued);

// The stage 5 pre-fight dialogue: reveals Mima through a summon and her
// first form.
void Mima_TalkBegin(
    BOSS_STATE *boss, PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
    const BOSS_MUSIC *music, const TH02_SESSION *session, bool post);
bool Mima_TalkUpdate(
    BOSS_STATE *boss, PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
    const BOSS_MUSIC *music, const TH02_SESSION *session, bool post,
    const TH02_INPUT *input);

// Runs Mima's form change; RUNNING when combat resumes, FINISHED at the
// ending.
BOSS_RESULT Mima_Transition(
    BOSS_STATE *boss, PATTERN_BANK *patterns, const BOSS_ASSETS *assets,
    const BOSS_MUSIC *music, const TH02_SESSION *session,
    const TH02_INPUT *input);
