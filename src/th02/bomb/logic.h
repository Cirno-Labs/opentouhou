#pragma once

#include "th02/bomb/assets.h"
#include "th02/entity.h"

typedef struct {
    int16_t frame;
    int16_t circle_frame;
    int16_t circle_done;
    ENTITY_POINT center;
    int16_t page_scroll[2];
    ENTITY_POINT columns[50];
    ENTITY_POINT particles[32];
    uint8_t ages[50];
    uint8_t stage_bombs;
    uint8_t active;
    uint8_t tile_modes[3];
    uint8_t row;
    uint8_t color_zero[3];
    uint8_t color_three[3];
} BOMB;

extern const BOMB *const g_Bomb;

// Resets the bomb state, once per process; stage changes and bombs keep
// the animation history.
void Bomb_Init(void);

// Clears only the active flag and the stage bomb count.
void Bomb_ResetStage(void);

// Starts a bomb, keeping particle ages and positions; requires a nonzero
// bomb count, which decrements with byte wrap.
void Bomb_Activate(PLAYFIELD_PAGES pages);

// Invalidates the tiles under the previous circle frame.
void Bomb_Invalidate(PLAYFIELD_PAGES pages, const BOMB_ASSETS *assets);

// Runs one bomb frame. Unless fix_transition_overread, transition_tail must
// hold the 72 bytes retail reads past its transition table.
void Bomb_Update(
    PLAYFIELD_PAGES pages, const BOMB_ASSETS *assets, uint8_t *slowdown,
    bool reduce_effects, const uint8_t *transition_tail,
    bool fix_transition_overread);
