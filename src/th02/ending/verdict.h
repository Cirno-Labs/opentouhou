#pragma once

#include "th02/ending/assets.h"
#include "th02/ending/ending.h"
#include "th02/input.h"
#include "th02/palette.h"
#include "th02/session.h"

void Verdict_Init(void);

// Starts the verdict, changing no counter, page or palette until Update.
// Requires charging off; keep session and assets alive until done.
void Verdict_Begin(
    const ENDING_ASSETS *assets, const TH02_SESSION *session,
    ENDING_STORY route, uint16_t style);

// Runs the verdict; returns true when done. Frame delays clear count1,
// while fades keep the counters.
bool Verdict_Update(const TH02_INPUT *input);
