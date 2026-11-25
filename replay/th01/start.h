#pragma once

#include "game.h"

#include <stddef.h>
#include <stdint.h>

// Applies a th01 case's start values (fields.h) to the native game, as the
// dosbox port does in the resident block that REIIDEN reads.

// Writes the start values other than boss_hp into the session that a Start
// has set up, before gameplay begins.
void Th01Start_Apply(const START_VALUE *starts, size_t count);

// Writes the start value boss_hp, if any, over the HP that a boss's setup
// has set.
void Th01Start_ApplyBoss(const START_VALUE *starts, size_t count, int16_t *hp);
