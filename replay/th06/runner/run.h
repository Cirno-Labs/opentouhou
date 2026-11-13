// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "types.h"

#include "core/base/result.h"

// Runs the original replay input against an OTRC case. Requires Files_Init
// before the call. Initializes and shuts down game state for each case.
// Uses no physical input. Version-1 START is a post-first-calc checkpoint;
// a case alone cannot initialize playback, so replay_path is required.
// Returns failure for invalid input, a mismatch or incomplete coverage.
// Full coverage requires all current fields and the final replay stage
// reaching replay-end and then the title scene before the case ends.
RESULT Runner_Run(
    const char *case_path, const char *replay_path,
    const RUNNER_OPTIONS *options, RUNNER_REPORT *report);
