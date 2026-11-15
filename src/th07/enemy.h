#pragma once

#include "th07/anm.h"
#include "th07/bullet.h"
#include "th07/ecl.h"
#include "th07/effect.h"

#include "core/base/result.h"
#include "core/math/vec.h"

#include "stg/timer.h"

#include <stdint.h>

// Runs the enemies: walks the timelines of the stage (ecl.h) to spawn
// them, runs their scripts, moves them, damages them with the shots and
// the bomb of the player, kills them, drops their items and keeps the
// state of the spell card. The sprite of an enemy is part of the
// simulation: its size decides when the enemy is on screen. Timers,
// movement and hit tests run at g_GameSpeed. Also runs the trails, the two
// draw layers, and the homing and aim targets of the player.

#define ENEMY_COUNT 0x1e0
#define ENEMY_SPRITE_COUNT 2
#define ENEMY_STACK_DEPTH 16
#define ENEMY_INTERRUPT_COUNT 0x20
#define ENEMY_LASER_COUNT 0x20
#define ENEMY_ORBIT_COUNT 25
#define ENEMY_SPELL_CIRCLE 24 // the orbit slot of a spell card's circle
#define ENEMY_BOSS_COUNT 8
#define ENEMY_LIFE_CALLBACK_COUNT 4
#define ENEMY_TRAIL_COUNT 0x60
#define ENEMY_DRAW_LIST_COUNT 4

typedef enum {
    ENEMY_MOVE_VELOCITY = 0,
    ENEMY_MOVE_ANGLE = 1, // by angle and speed
    ENEMY_MOVE_TO = 2, // from move.start by move.delta, over move.duration
    ENEMY_MOVE_CIRCLE = 3, // around move.start by move.circle (opcode 56)
} ENEMY_MOVE;

// Selects how ENEMY_MOVE_TO eases its progress.
typedef enum {
    ENEMY_EASE_LINEAR = 0,
    ENEMY_EASE_IN = 1, // t squared: speeds up from the start
    ENEMY_EASE_IN3 = 2,
    ENEMY_EASE_IN4 = 3,
    ENEMY_EASE_OUT = 4, // slows down to the end
    ENEMY_EASE_OUT3 = 5,
    ENEMY_EASE_OUT4 = 6,
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
    // Leaves the enemy 1 life, stops its damage, hides the boss bar and
    // marks REPLAY_EVENT_ENEMY_GONE. The death mode becomes
    // ENEMY_DEATH_REMOVE.
    ENEMY_DEATH_SURVIVE = 3,
} ENEMY_DEATH;

// Selects the turning script that the sprite shows last.
typedef enum {
    ENEMY_FACING_STILL = 0,
    ENEMY_FACING_LEFT = 1,
    ENEMY_FACING_RIGHT = 2,
    ENEMY_FACING_UNSET = 0xff,
} ENEMY_FACING;

// Selects how the trail of an enemy acts and shows.
typedef enum {
    ENEMY_TRAIL_ON = 1,
    // Shrinks along the trail: its hitboxes and its copies of the sprite.
    ENEMY_TRAIL_NARROW = 2,
    ENEMY_TRAIL_FADE = 4,
    ENEMY_TRAIL_STRIP = 8, // one strip instead of copies of the sprite
    ENEMY_TRAIL_HIDE_HEAD = 0x10, // the main sprite is not drawn
} ENEMY_TRAIL_FLAG;

// Selects the pair of draw lists, and so the draw layer, of an enemy.
typedef enum {
    ENEMY_DRAW_LOW = 0, // priority 5, with list 1
    ENEMY_DRAW_HIGH = 2, // priority 7, with list 3 (opcode 136)
} ENEMY_DRAW_LIST;

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
    bool takes_damage;
    bool hidden; // not drawn, not damaged and not killed
    bool hit_by_shots; // Player_CalcDamage tests it
    // Grazes the player every sixth frame, and touching it costs no life.
    // A bonus clear drops an ITEM_STAR from it.
    bool grazeable;
    bool boss; // shown in the boss bar
    bool clamp_pos; // keeps pos within move.bounds_min and bounds_max
    ENEMY_DEATH death;
    bool on_screen; // was inside the play area; leaving it frees the enemy
    bool rotate_sprite; // turns vm with move.angle
    bool no_call_stack; // calls and interrupts do not save the state
    // Keeps the spell card running and the bullets in place when the
    // timer callback starts.
    bool keep_spell_card;
    bool stays_off_screen;
    bool warping_to_boss; // skips the clamp and the move; ECL sets it
    // Stops the spell card's circle following the enemy.
    bool spell_circle_detached;
    // Hides the main sprite and skips the contact and the damage.
    bool hidden_by_bomb;
    // While a bomb runs or the player is not PLAYER_ALIVE, the update
    // skips the enemy and holds callbacks.timer back.
    bool halted_by_bomb;
    uint8_t facing; // ENEMY_FACING
} ENEMY_FLAGS;

typedef struct {
    VEC3 pos;
    VEC3 velocity;
    float angle;
} ENEMY_TRAIL_BOX;

typedef struct ENEMY {
    ANM_VM vm;
    // Hold the extra sprites that scripts start: vms[0] under vm, vms[1]
    // over it, turned the opposite way. A sprite is unused while its
    // script_index is negative, and becomes unused when its script ends.
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
        // Starts sub with vars every interval frames while sub >= 0.
        struct {
            int32_t sub;
            ECL_VARS vars;
            TIMER interval, timer;
        } periodic;
    } script;

    struct {
        VEC3 pos;
        VEC3 velocity;
        // Hold where the last move started, and how far it went.
        VEC3 prev_pos, step;
        float angle;
        float angular_velocity;
        float speed;
        float acceleration;
        struct {
            float angle, angular_velocity;
            float radius, radius_velocity;
        } circle;
        VEC3 start, delta;
        TIMER timer; // counts down move.duration
        int32_t duration;
        VEC2 bounds_min, bounds_max;
    } move;

    VEC3 hitbox_size;
    // Is also tested while its x is above 0. Outside a bomb, it adds its
    // damage / 2.5.
    VEC3 extra_hitbox_size;
    int32_t life, max_life;
    int32_t score;
    int32_t last_damage; // ECL variable 10061
    // While above 0, a boss takes a ninth of the damage and others none.
    TIMER invulnerable_timer;

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

    // Start subroutines when the life falls below one of life, or when
    // timer reaches time. Negative life or time means none. The boss timer
    // shows the seconds to time.
    struct {
        int32_t life[ENEMY_LIFE_CALLBACK_COUNT];
        int32_t life_sub[ENEMY_LIFE_CALLBACK_COUNT];
        int32_t time;
        int32_t time_sub;
        TIMER timer;
    } callbacks;

    struct {
        int8_t effect; // an EFFECT_TYPE spawned on death; negative for none
        // Selects the color of the EFFECT_PARTICLE bursts on death and on
        // an item drop.
        uint8_t particle_color;
        // Holds the item dropped on death: an ITEM_TYPE, -1 for the drop
        // table on every third such enemy, or below -1 for none.
        int8_t item;
    } death;

    uint8_t boss_index; // the slot in g_Enemies.bosses
    // Counts down the frame after a hit in which vm shows color2.
    uint8_t hit_flash_frames;
    uint16_t bomb_hide_frames;
    uint8_t draw_list; // ENEMY_DRAW_LIST
    ENEMY_FLAGS flags;

    // Hold the scripts, counted from 0x900, that the sprite turns with: to
    // still at first, to still from left and from right, to left and to
    // right. With left negative, the sprite never turns.
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

    // Holds the positions of the enemy in the last count updates, newest
    // first. Every step-th box is drawn, and every sixth below
    // contact_count touches the player.
    struct {
        uint8_t flags; // ENEMY_TRAIL_FLAG
        int16_t count, contact_count, step;
        ENEMY_TRAIL_BOX boxes[ENEMY_TRAIL_COUNT];
    } trail;

    struct ENEMY *next_drawn;
} ENEMY;

typedef enum {
    ENEMY_SPELL_NONE = 0,
    ENEMY_SPELL_RUNNING = 1,
    // The timer callback started during it. Each further start counts up.
    ENEMY_SPELL_TIMED_OUT = 2,
} ENEMY_SPELL;

typedef struct {
    ENEMY enemies[ENEMY_COUNT];
    // Holds the enemies that the timelines name, set by ECL.
    ENEMY *bosses[ENEMY_BOSS_COUNT];

    // Count the enemies that drop from the drop table, and select the next
    // item of the table.
    struct {
        uint16_t counter;
        uint16_t index;
    } drops;

    // Is the boss and spell card state that gui, bomb, player, item and ECL
    // read.
    struct {
        ENEMY_SPELL state;
        // Is cleared by a bomb, a death and the timer callback.
        bool capturable;
        // Is set, at each bomb, to whether a spell card runs. During a
        // spell card, a bomb deals its damage / 2.5 while this is set and
        // none otherwise.
        bool bombed;
        int32_t index;
        int32_t bonus; // falls by bonus_step every second (ECL)
        int32_t graze_bonus; // each graze: 2500 + 20 per 1500 cherry
        int32_t bonus_step;
        TIMER bonus_timer;
    } spell_card;

    struct {
        int32_t index; // the next entry
        TIMER time;
    } timelines[ECL_TIMELINE_COUNT];

    // Counts the updates. Outside dialogue, every 2400 - 240 * lives of
    // them adds 100 to the sub-rank.
    TIMER frames;

    // Is rebuilt by each update, last enemy first.
    ENEMY *draw_lists[ENEMY_DRAW_LIST_COUNT];
} ENEMY_MANAGER;

extern ENEMY_MANAGER g_Enemies;

// Adds the update element (priority 10) and the draw elements (priorities
// 5 and 7), and resets g_Enemies. Adding loads stgNenm.anm of
// g_GameManager.run.stage (1 to 8), then draws the drop counter and the
// drop index from g_Rng in that order, ends the spell card and moves the
// boss markers to -999.
RESULT Enemy_Register(void);

// Removes the three elements, releases the enemy sprites and moves the
// boss markers to -999.
void Enemy_Remove(void);

// Spawns an enemy at pos in the first free slot, starts subroutine sub
// and runs its first frame. A life of 0 or more replaces the script's
// before the first frame only; a score of 0 or more replaces it after.
// Item is as death.item, truncated to a byte. If the first frame ends the
// script, the slot is freed and returned anyway. Returns a spare enemy
// that never runs when no slot is free.
ENEMY *Enemy_Spawn(
    int32_t sub, VEC3 pos, int32_t life, int32_t item, int32_t score,
    bool mirrored);

// Spawns as Enemy_Spawn, unmirrored, with the variables vars from the
// start of the subroutine. A life of 0 or more is set both before and
// after the first frame.
ENEMY *Enemy_SpawnWithVars(
    int32_t sub, VEC3 pos, int32_t life, int32_t item, int32_t score,
    const ECL_VARS *vars);

// Keeps the position of enemy within its bounds, while flags.clamp_pos is
// set.
void Enemy_ClampPos(ENEMY *enemy);

// Renews sprite presentation identities after an instant position change.
void Enemy_RenewPresentation(ENEMY *enemy);

// Makes the orbit effects of enemy fade out and forgets them.
void Enemy_ReleaseOrbits(ENEMY *enemy);

// Kills every active enemy except the bosses, and starts the death
// subroutine of those that cannot be shot. Each grazeable enemy, and every
// sixth box of its trail, drops a homing ITEM_STAR and pops a bonus that
// starts at 2000 and grows by 30 up to max_bonus, yellow from max_bonus.
// Returns start plus the bonuses.
int32_t Enemy_ClearForBonus(int32_t max_bonus, int32_t start);

// Returns whether any boss slot is set.
bool Enemy_IsBossActive(void);
