#pragma once

#include "core/base/types.h"

typedef struct {
    int32_t value;
    int32_t delta;
    int32_t highest;
    int32_t reset_unknown;
    uint16_t transferred;
    uint16_t extends;
    int8_t lives;
    int8_t bombs;
    uint8_t highest_continues;
} SCORE;

extern const SCORE *const g_Score;

// Clears the score state for a new run.
void Score_Init(void);

// Splits a stored score into score / 10 and highest_continues, the
// remainder's low byte, keeping the current score when greater.
void Score_InitHighest(int32_t stored_score);

// Counts the extend thresholds value has reached; as in retail, two past
// the table alias adjacent state, so value must stay below the last
// (1111490560 when reset_unknown is zero or 40000).
void Score_InitExtends(void);

// Adds delta to value with 32-bit wrap and clears delta and transferred,
// granting no extends.
void Score_Commit(void);

// Clears only value, delta and transferred and sets reset_unknown to 40000.
void Score_Reset(void);

// Grants at most one reached extend, with effect 8 even when lives and
// bombs are capped. Requires extends 0..7.
void Score_Update(bool reduce_effects);

// Adds delta to value, keeping delta, and at five lives consumes reached
// extends with no bomb or effect. Requires extends 0..7.
void Score_GrantBonus(void);

// Starts a run's value, lives and bombs, and clears delta.
void Score_BeginSession(int32_t value, int8_t lives, int8_t bombs);

// Restores lives and bombs for a continue.
void Score_Continue(int8_t lives, int8_t bombs);

// Grants one bomb.
void Score_GrantBomb(void);

// Grants one life.
void Score_GrantLife(void);

// Loses one life and refills bombs, granting two extra at the last life.
void Score_LoseLife(int8_t start_bombs);

// Spends one bomb.
void Score_UseBomb(void);

// Adds points to delta with 32-bit wrap, as a boss or an enemy awards them.
void Score_AddPoints(uint32_t points);

// Adds points to value with 32-bit wrap, as Mima's and Sigma's direct
// awards do.
void Score_AddBonus(uint32_t points);

// Sets value for a run's start or a stage clear, keeping delta.
void Score_Set(int32_t value);

// Clears delta for a stage or Extra clear, keeping value.
void Score_ClearDelta(void);

// Adds shot damage to delta's low word, without carry into the high word,
// as retail's hit test does.
void Score_AddDamage(int16_t damage);
