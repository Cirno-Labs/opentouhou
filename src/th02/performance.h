#pragma once

// The hidden performance rating that the game raises over time and lowers
// on misses, and the item counts that feed the skill score of a run.

#include "core/base/types.h"

typedef struct {
    int16_t value, item_skill;
    uint8_t maximum, total_misses, total_bombs, collected, dropped;
} PERFORMANCE;

extern const PERFORMANCE *const g_Performance;

// Resets the performance for a new run.
void Performance_Init(int8_t rank);

// Raises the value when stage_frame % 1800 is 1500; call it after the
// frame increment, before Score_Update.
void Performance_Advance(uint32_t stage_frame);

// Lowers the value by two with word wrap, down to -6; values above two drop
// to zero.
void Performance_MissPenalty(void);

// Adds an item's weight with byte wrap.
void Performance_AddCollection(uint8_t weight);

// Takes at most one group of 32 from the collection; call it after item
// removal and effect 13.
void Performance_CommitCollection(void);

// Counts an item lost off the bottom, with the skill penalty on every
// sixteenth.
void Performance_Drop(void);

// Returns the skill score with signed-word wrap, clamping item_skill to
// 0..25 in place.
int16_t Performance_Skill(int8_t rank, uint16_t continues);

// Sets value to the demo's fixed level.
void Performance_BeginDemo(void);

// Counts one bomb used.
void Performance_CountBomb(void);

// Counts one miss.
void Performance_CountMiss(void);
