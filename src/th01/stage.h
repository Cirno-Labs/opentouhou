#pragma once

#include "th01/player.h"
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
    int16_t left, top;
    STAGE_PELLET_GROUP group;
    int16_t speed;
} STAGE_PELLET_SPAWN;

typedef struct {
    uint32_t score;
    int16_t combo, maximum_combo;
    uint8_t flip_cycle;
    bool cleared;
} STAGE_GAMEPLAY;

typedef struct {
    STAGE_OBJECTS *objects;
    STAGE_GAMEPLAY *gameplay;
    ORB *orb;
    PLAYER *player;
    uint32_t *rng;
    const PTN *sprites;
    const GAME_TEXT_FONT *font;
    int16_t stage_id, stage_num;
    SETTINGS_DIFFICULTY difficulty;
    bool test_damage;
    void (*hud_score_and_combo)(void *user);
    void (*item_add)(void *user, int32_t card_slot, bool bomb);
    void (*effect)(void *user, int16_t number);
    void (*pellet_group)(void *user, STAGE_PELLET_SPAWN spawn);
    void (*pellets_update)(void *user);
    void *user;
} STAGE_CONTEXT;

// Execute reached callbacks synchronously; all are required. HUD observes
// the updated shared score and combo. Item creation uses the card slot and
// must not consume RNG. Pellet creation uses Q12.4 speed and may consume
// shared RNG before control returns. HUD, item and pellet-creation
// consumers must not mutate stage object storage, score, combo or player
// state. The composite pellets_update exception is specified below. Keep
// immutable sprites and cached ASCII digit glyphs alive across updates.
// Supply real HUD, item and pellet consumers for these calls. Initialize
// flip_cycle once per main scene as MasterRng_Next(rng) % 60; StageObjects_Init
// must not initialize it anew at each stage change. Scores and counters wrap to
// their source widths; updates allocate nothing.

// Validates all ten cached halfwidth score digits before simulation.
RESULT Stage_ValidateScoreFont(const GAME_TEXT_FONT *font);

// Starts flips and immediately updates score/combo/HUD, drops and effect 2.
// Does not move or reflect the Orb. Cards are tested even inside portals.
// Bomb selection draws before testing the selected card's alive flag.
void Stage_HitTestCards(const STAGE_CONTEXT *context);

// Draws score popups before card animations, retaining their overlap. Runs
// six frames per image; revenge fires at 25, HP/removal at 30. All removed
// sets cleared and player.is_hit together. The world must interpret this
// pair as stage exit rather than a miss. Empty layouts clear safely.
void Stage_UpdateCards(const STAGE_CONTEXT *context);

// Runs bumper, bar, turret and portal branches in obstacle order. Reset
// still executes collisions, firing and portal animation before clearing
// each obstacle. It can move the Orb and consume RNG. Reset does not erase
// portal animation before drawing its normal sprite, retaining the source
// artifact. Turrets on Easy retain their current cycle without advancing.
// Call before abandoning a stage group while the Orb is in a portal.
void Stage_UpdateObstacles(const STAGE_CONTEXT *context, bool reset);

typedef struct {
    ORB_COMPOSITE orb;
    bool active, waiting;
} STAGE_ORB_COMPOSITE;

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
// Does not advance force age, player update, bomb frame or world counters.
// Supply real pellet, item and HUD consumers through the context.
bool Stage_BeginOrbComposite(
    STAGE_ORB_COMPOSITE *composite, const STAGE_CONTEXT *context);
bool Stage_ResumeOrbComposite(
    STAGE_ORB_COMPOSITE *composite, const STAGE_CONTEXT *context);
