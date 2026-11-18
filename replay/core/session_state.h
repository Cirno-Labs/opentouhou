#pragma once

#include "case.h"

// Describes state that both backends can read in every scene. Gameplay
// fields are sampled only while gameplay is true.
typedef struct {
    uint32_t frame;
    uint32_t scene;
    uint32_t requested_scene;
    uint32_t buttons;
    uint32_t rng_seed;
    uint32_t rng_generated;
    bool gameplay;
    bool score_valid;
    uint32_t score;
    uint32_t shown_score;
} SESSION_POINT;

typedef struct {
    const char *id;
    const char *const *start_names;
    int32_t start_count;
    const char *const *calc_names;
    int32_t calc_count;
    void (*start)(uint32_t *row);
    void (*calc)(uint32_t *row);
} SESSION_STATE_GAME;

typedef struct SESSION_STATE SESSION_STATE;

// Builds the common scene columns and the game's named state columns.
SESSION_STATE *SessionState_Create(const SESSION_STATE_GAME *game);
void SessionState_Free(SESSION_STATE *state);

// Returns the complete schema, owned by state.
const CASE_NAMES *SessionState_Columns(const SESSION_STATE *state);

// Samples one completed calc. Clears gameplay fields outside gameplay,
// where the original game's statistics can be absent or uninitialized.
const uint32_t *SessionState_Sample(
    SESSION_STATE *state, const SESSION_POINT *point);

// Opens a v1 binary trace with no START columns. The archive stores the
// readable input timeline separately. The caller closes the writer.
CASE_WRITER *SessionState_OpenWriter(
    const SESSION_STATE *state, WRITER *output);
