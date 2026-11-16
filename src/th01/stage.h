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
    VIDEO *video;
    const PTN *sprites;
    const GAME_TEXT_FONT *font;
    int16_t stage_id, stage_num;
    SETTINGS_DIFFICULTY difficulty;
    bool test_damage;
    void (*hud_score_and_combo)(void *user);
    void (*item_add)(void *user, int32_t card_slot, bool bomb);
    void (*effect)(void *user, int16_t number);
    void (*pellet_group)(void *user, STAGE_PELLET_SPAWN spawn);
    void *user;
} STAGE_CONTEXT;

// Execute reached callbacks synchronously; all are required. HUD observes
// the updated shared score and combo. Item creation uses the card slot and
// must not consume RNG. Pellet creation uses Q12.4 speed and may consume
// shared RNG before control returns. Consumers must not mutate stage
// object storage, score, combo or player state. Keep immutable sprites and
// cached ASCII digit glyphs alive across updates. Gameplay integration is
// unfinished until real HUD, item and pellet consumers supply these calls.
// Initialize flip_cycle once per main scene as Math_Random(rng) % 60;
// StageObjects_Init must not initialize it anew at each stage change.
// Scores and counters wrap to their source widths; updates allocate nothing.

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
