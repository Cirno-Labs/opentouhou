#pragma once

#include "replay/harness/game.h"
#include "th02/session.h"

#include <stddef.h>
#include <stdint.h>

// Applies a th02 case's start values (fields.h) to the native game, as the
// dosbox port does at the Start handoff.

// Writes the start values other than boss_hp into the session that the
// first Start handoff passes.
void Th02Start_Apply(
    const START_VALUE *starts, size_t count, TH02_SESSION *session);

// Writes the start value boss_hp, if any, as the remaining damage of a
// boss or phase whose damage counter has just been cleared.
void Th02Start_ApplyBoss(
    const START_VALUE *starts, size_t count, int16_t *damage,
    int16_t threshold);
