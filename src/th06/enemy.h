#pragma once

#include "th06/anm.h"
#include "th06/bullet.h"
#include "th06/ecl.h"
#include "th06/effect.h"
#include "th06/timer.h"

#include "core/base/result.h"
#include "core/math/vec.h"

#include <stdint.h>

// Runs the enemies: walks the timeline of the stage (ecl.h) to spawn them,
// runs their scripts, moves them, damages them with the shots and the bomb
// of the player, kills them, drops their items and keeps the state of the
// spell card. The sprite of an enemy is part of the simulation: its size
// decides when the enemy is on screen. Drawing comes with graphics.

#define ENEMY_COUNT 0x100
#define ENEMY_SPRITE_COUNT 8
#define ENEMY_STACK_DEPTH 8
#define ENEMY_INTERRUPT_COUNT 8
#define ENEMY_LASER_COUNT 0x20
#define ENEMY_ORBIT_COUNT 12
#define ENEMY_BOSS_COUNT 8

typedef enum {
    ENEMY_MOVE_VELOCITY = 0, // opcode 44
    ENEMY_MOVE_ANGLE = 1, // by angle and speed, opcodes 45 to 51
    ENEMY_MOVE_TO = 2, // from move.start by move.delta, over move.duration
} ENEMY_MOVE;

// Selects how ENEMY_MOVE_TO eases its progress.
typedef enum {
    ENEMY_EASE_LINEAR = 0,
    ENEMY_EASE_OUT = 1, // slows down to the end
    ENEMY_EASE_OUT4 = 2,
    ENEMY_EASE_IN = 3, // speeds up from the start
    ENEMY_EASE_IN4 = 4,
} ENEMY_EASE;

// Selects what happens when the life of an enemy that can be shot falls
// to 0.
typedef enum {
    // Scores, drops the item and frees the enemy.
    ENEMY_DEATH_REMOVE = 0,
    // Scores, drops the item and keeps the enemy, unable to be shot.
    ENEMY_DEATH_KEEP = 1,
    // Drops the item and keeps the enemy as it is.
    ENEMY_DEATH_DROP = 2,
    // Leaves the enemy 1 life, stops its damage and hides the boss bar.
    // The death mode becomes ENEMY_DEATH_REMOVE.
    ENEMY_DEATH_SURVIVE = 3,
} ENEMY_DEATH;

// Selects the turning script that the sprite shows last.
typedef enum {
    ENEMY_FACING_STILL = 0,
    ENEMY_FACING_LEFT = 1,
    ENEMY_FACING_RIGHT = 2,
    ENEMY_FACING_UNSET = 0xff, // set by opcode 98
} ENEMY_FACING;

// Holds the flags of an enemy. Scripts set each bool from the lowest bit
// of an int argument, and death from its low 3 bits.
typedef struct {
    ENEMY_MOVE move;
    ENEMY_EASE ease;
    bool hold_fire; // the fire opcodes only set the shooter up
    bool mirrored; // mirrors the horizontal movement
    bool active;
    bool shootable;
    bool contact_damage; // touching the player costs the enemy 10 life
    bool on_screen; // was inside the play area; leaving it frees the enemy
    bool boss; // shown in the boss bar
    bool takes_damage;
    ENEMY_DEATH death;
    bool clamp_pos; // keeps pos within move.bounds_min and bounds_max
    bool rotate_sprite; // turns vm with move.angle
    bool no_call_stack; // calls and interrupts do not save the state
    bool hidden; // not drawn, not damaged and not killed
    // Keeps the spell card running and the bullets in place when the
    // timer callback starts.
    bool keep_spell_card;
    uint8_t facing; // ENEMY_FACING
} ENEMY_FLAGS;

typedef struct ENEMY {
    ANM_VM vm;
    // Hold the extra sprites that scripts start. A sprite is unused while
    // its script_index is negative, and becomes unused when its script
    // ends.
    ANM_VM vms[ENEMY_SPRITE_COUNT];
    // Holds the color of vm between updates. The update puts it back into
    // vm before each step of the script and saves it again after, so a
    // color that the enemy script gives vm before the step is lost.
    uint32_t color;

    struct {
        ECL_STATE state;
        ECL_STATE stack[ENEMY_STACK_DEPTH];
        int32_t depth;
        int32_t death_sub; // runs when the enemy dies; negative for none
        int32_t interrupts[ENEMY_INTERRUPT_COUNT]; // their subroutines
        int32_t pending_interrupt; // runs next frame; negative for none
    } script;

    struct {
        VEC3 pos;
        VEC3 velocity;
        float angle;
        float angular_velocity;
        float speed;
        float acceleration;
        VEC3 start, delta;
        TIMER timer; // counts down move.duration
        int32_t duration;
        VEC2 bounds_min, bounds_max;
    } move;

    VEC3 hitbox_size;
    int32_t life, max_life;
    int32_t score;

    struct {
        BULLET_SHOOTER shooter;
        VEC3 offset; // from move.pos
        int32_t interval; // fires shooter every interval frames, when above 0
        TIMER timer;
        // Hold how the speeds and counts of the shots grow with the rank:
        // what is added at rank 0 and at rank 32.
        float rank_speed_low, rank_speed_high;
        int16_t rank_columns_low, rank_columns_high;
        int16_t rank_rows_low, rank_rows_high;
        BULLET_SHOOTER laser_shooter;
        LASER *lasers[ENEMY_LASER_COUNT];
        int32_t laser_index; // where the next laser goes in lasers
    } shots;

    // Start subroutines when the life falls below life or when timer
    // reaches time. Negative life or time means none. The boss timer
    // shows the seconds to time.
    struct {
        int32_t life;
        int32_t life_sub;
        int32_t time;
        int32_t time_sub;
        TIMER timer; // stops while the game is frozen
    } callbacks;

    struct {
        uint8_t effect; // an EFFECT_TYPE spawned on death
        // Selects the color of the EFFECT_PARTICLE bursts on death and on
        // an item drop.
        uint8_t particle_color;
        // Holds the item dropped on death: an ITEM_TYPE, -1 for the drop
        // table on every third such enemy, or below -1 for none.
        int8_t item;
    } death;

    uint8_t boss_index; // the slot in g_Enemies.bosses
    uint8_t hit_flash_frames; // counts down the flash of the sprite after a hit
    // Counts down the frames after a bomb in which
    // ECL_FUNC_SPARKS_AND_BOMB_SHIELD keeps the enemy unable to be shot.
    TIMER shield_timer;
    ENEMY_FLAGS flags;

    // Hold the scripts, from 0x100, that the sprite turns with: to still
    // at first, to still from left and from right, to left and to right.
    // With left negative, the sprite never turns.
    struct {
        int16_t still, still_from_left, still_from_right;
        int16_t left, right;
    } turns;

    // Hold the orbit effects that follow the enemy and widen to radius.
    struct {
        EFFECT *effects[ENEMY_ORBIT_COUNT];
        int32_t count;
        float radius;
    } orbits;

    // Hold the circle of ECL_FUNC_SPARK_CIRCLE.
    struct {
        float angle;
        TIMER timer;
    } sparks;
} ENEMY;

typedef enum {
    ENEMY_SPELL_NONE = 0,
    ENEMY_SPELL_RUNNING = 1,
    ENEMY_SPELL_TIMED_OUT = 2, // the timer callback started during it
} ENEMY_SPELL;

typedef struct {
    ENEMY enemies[ENEMY_COUNT];
    // Holds the enemies that the timeline names, set by opcode 101.
    ENEMY *bosses[ENEMY_BOSS_COUNT];

    // Count the enemies that drop from the drop table, and select the next
    // item of the table.
    struct {
        uint16_t counter;
        uint16_t index;
    } drops;

    struct {
        ENEMY_SPELL state;
        // Is cleared when the player dies or bombs, and when the timer
        // callback starts.
        bool capturable;
        // Is set, at each bomb, to whether a spell card runs. It stays set
        // after the spell card ends, until the next bomb. During a spell
        // card, the bomb deals a third of its damage while this is set and
        // none otherwise.
        bool bombed;
        int32_t index;
        int32_t bonus;
    } spell_card;

    struct {
        int32_t index; // the next entry
        TIMER time;
    } timeline;
} ENEMY_MANAGER;

extern ENEMY_MANAGER g_Enemies;

// Adds the update element (priority 9) and the draw element (priority 6),
// and resets g_Enemies. Adding loads the enemy sprites of
// g_GameManager.run.stage and draws the first drop counter and drop index
// from g_Rng.
RESULT Enemy_Register(void);
// Removes both elements and releases the enemy sprites.
void Enemy_Remove(void);

// Spawns an enemy at pos in the first free slot, starts subroutine sub
// and runs its first frame. A life or score of 0 or more replaces the
// script's. Item is as death.item, truncated to a byte. Returns the
// enemy, or a spare enemy that never runs when no slot is free.
ENEMY *Enemy_Spawn(
    int32_t sub, VEC3 pos, int16_t life, int16_t item, int32_t score);
// Keeps the position of enemy within its bounds, while flags.clamp_pos is
// set.
void Enemy_ClampPos(ENEMY *enemy);
// Renews sprite presentation identities after an instant position change.
void Enemy_RenewPresentation(ENEMY *enemy);
// Makes the orbit effects of enemy fade out and forgets them.
void Enemy_ReleaseOrbits(ENEMY *enemy);
// Kills every active enemy except the bosses: sets its life to 0, and
// starts the death subroutine of one that cannot be shot.
void Enemy_KillAll(void);
