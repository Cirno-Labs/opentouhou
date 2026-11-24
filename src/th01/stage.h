#pragma once

#include "th01/pellet_assets.h"
#include "th01/player.h"
#include "th01/shots.h"
#include "th01/stage_objects.h"
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
    int16_t combo, maximum_combo;
    uint8_t flip_cycle;
    bool cleared;
    // True until the first stage transition of the session.
    bool first_stage;
    bool timer_initialized;
    bool game_cleared;
} STAGE_GAMEPLAY;

extern STAGE_GAMEPLAY g_Stage;

// The HUD shows the updated score and combo. Pellet groups use Q12.4 speed
// and may consume shared RNG. Keep immutable sprites and cached ASCII digit
// glyphs alive across updates. Initialize flip_cycle once per main scene as
// MasterRng_Next(rng) % 60; StageObjects_Init must not initialize it anew at
// each stage change. Scores and counters wrap to their source widths;
// updates allocate nothing.

// Validates all ten cached halfwidth score digits before simulation.
RESULT Stage_ValidateScoreFont(const GAME_TEXT_FONT *font);

// Starts flips and immediately updates score/combo/HUD, drops and effect 2.
// Does not move or reflect the Orb. Cards are tested even inside portals.
// Bomb selection draws before testing the selected card's alive flag. Reads
// the stage id, difficulty and g_TestDamage from their owning globals.
void Stage_HitTestCards(void);

// Draws score popups before card animations, retaining their overlap. Runs
// six frames per image; revenge fires at 25, HP/removal at 30. All removed
// sets cleared and player.is_hit together. The world must interpret this
// pair as stage exit rather than a miss. Empty layouts clear safely.
void Stage_UpdateCards(const PTN *sprites, const GAME_TEXT_FONT *font);

// Runs bumper, bar, turret and portal branches in obstacle order. Reset
// still executes collisions, firing and portal animation before clearing
// each obstacle. It can move the Orb and consume RNG. Reset does not erase
// portal animation before drawing its normal sprite, retaining the source
// artifact. Turrets on Easy retain their current cycle without advancing.
// Call before abandoning a stage group while the Orb is in a portal.
void Stage_UpdateObstacles(const PTN *sprites, bool reset);

typedef struct {
    bool active, waiting, boss;
} STAGE_ORB_COMPOSITE;

// Zeroes the Orb composite state for a new world.
void StageOrb_Init(void);

// Returns whether the Orb composite is blocked waiting on frame_delay(1).
bool StageOrb_Waiting(void);

// Runs the real Orb composite through its frame_delay(1). Begin advances
// motion before the delay; Resume continues only when count1 is ready.
// Zero-initialize composite once and keep context/state/assets stable while
// suspended. Repeated blocked polls have no side effects. A ready delay
// runs through immediately; it is not an unconditional next-edge wait.
// Required pellets_update runs after that delay. It must implement real
// pellet motion/clipping, then Shots_UpdateAndRender, then pellet drawing
// in source order. This operation may change shared player/Orb state and
// consume RNG; stage phases read those changes before proceeding. Do not
// update shots again outside it. Cards, obstacles, lethal Orb hit testing,
// erase using current image, card drawing, conditional Orb put and previous
// coordinates follow in source order, reading hit/portal flags afresh.
// Boss stages skip cards and obstacles. Does not advance force age, player
// update, bomb frame or world counters.
bool Stage_BeginOrbComposite(
    const PTN *sprites, const GAME_TEXT_FONT *font,
    const PELLET_ASSETS *pellet_assets);
bool Stage_ResumeOrbComposite(
    const PTN *sprites, const GAME_TEXT_FONT *font,
    const PELLET_ASSETS *pellet_assets);
