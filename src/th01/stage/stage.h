#pragma once

#include "th01/boss.h"
#include "th01/pellets/assets.h"
#include "th01/player.h"
#include "th01/shots.h"
#include "th01/stage/objects.h"
#include "th01/text.h"

typedef enum {
    STAGE_PELLET_1_AIMED = 12,
    STAGE_PELLET_2_SPREAD_WIDE_AIMED = 21,
    STAGE_PELLET_3_SPREAD_WIDE_AIMED = 23,
    STAGE_PELLET_4_SPREAD_WIDE_AIMED = 25,
    STAGE_PELLET_5_SPREAD_WIDE_AIMED = 27,
    STAGE_PELLET_1_RANDOM_NARROW_AIMED = 29,
} STAGE_PELLET_GROUP;

typedef struct {
    uint32_t score;
    uint32_t frame;
    BOSS boss;
    uint16_t time;
    int16_t combo;
    int16_t maximum_combo;
    uint8_t flip_cycle;
    bool cleared;
    // True until the first stage transition of the session.
    bool first_stage;
    bool timer_initialized;
    bool game_cleared;
} STAGE_GAMEPLAY;

typedef struct {
    bool active;
    bool waiting;
    bool boss;
} STAGE_ORB_COMPOSITE;

extern const STAGE_GAMEPLAY *const g_Stage;

// Zeroes the stage for a new world, with first_stage set.
void Stage_Init(void);

// Adds points to the score, wrapping in unsigned 32 bits.
void Stage_AddScore(uint32_t points);

// Resets the stage frame and clears the boss when the stage is left.
void Stage_Leave(void);

// Sets the current boss, or BOSS_NONE for an ordinary stage.
void Stage_SetBoss(BOSS boss);

// Sets whether this is the first stage shown since the last world reset.
void Stage_SetFirstStage(bool first_stage);

// Sets the stage frame counter.
void Stage_SetFrame(uint32_t frame);

// Advances the stage frame counter by one.
void Stage_AdvanceFrame(void);

// Sets the card combo counter.
void Stage_SetCombo(int16_t combo);

// Sets the highest combo reached this stage.
void Stage_SetMaximumCombo(int16_t maximum_combo);

// Marks the stage cleared, as the last card leaves the Orb composite.
void Stage_MarkCleared(void);

// Returns whether the stage is cleared, and clears the flag.
bool Stage_TakeCleared(void);

// Sets whether the stage timer has already been initialized this stage.
void Stage_SetTimerInitialized(bool timer_initialized);

// Sets the stage timer value, in frames.
void Stage_SetTime(uint16_t time);

// Marks the game cleared, after the last boss of the run is defeated.
void Stage_SetGameCleared(void);

// Syncs the score back from the session, such as on a continue.
void Stage_SyncScore(uint32_t score);

// Sets the sprite flip cycle offset for a new run.
void Stage_SetFlipCycle(uint8_t flip_cycle);

// Initialize flip_cycle once per main scene as MasterRng_Next(rng) % 60,
// not at each stage change.

// Validates all ten cached halfwidth score digits before simulation.
RESULT Stage_ValidateScoreFont(const GAME_TEXT_FONT *font);

// Hit-tests the cards against the Orb and bomb and starts their flips,
// updating score, combo and HUD. Cards are tested even inside portals.
void Stage_HitTestCards(void);

// Animates the cards and their score popups. Clearing the last card sets
// cleared and player.is_hit together, which means stage exit, not a miss.
void Stage_UpdateCards(const PTN *sprites, const GAME_TEXT_FONT *font);

// Updates bumpers, bars, turrets and portals; reset then clears them.
// Call with reset before abandoning a stage while the Orb is in a portal.
void Stage_UpdateObstacles(const PTN *sprites, bool reset);

// Zeroes the Orb composite state for a new world.
void StageOrb_Init(void);

// Runs the Orb composite through its frame delay: Begin moves the Orb,
// Resume continues when count1 is ready, then pellets_update and the cards.
// Keep the composite and its assets stable while suspended.
bool Stage_BeginOrbComposite(
    const PTN *sprites, const GAME_TEXT_FONT *font,
    const PELLET_ASSETS *pellet_assets);
bool Stage_ResumeOrbComposite(
    const PTN *sprites, const GAME_TEXT_FONT *font,
    const PELLET_ASSETS *pellet_assets);
