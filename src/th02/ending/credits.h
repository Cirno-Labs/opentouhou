#pragma once

#include "th02/ending/assets.h"
#include "th02/ending/ending.h"
#include "th02/input.h"
#include "th02/palette.h"
#include "th02/session.h"

void Credits_Init(void);

// Starts the credits; requires opaque 16x32 titles in slots 0..54.
void Credits_Begin(const ENDING_ASSETS *assets);

// Runs the credits paced by music measures, or 100 frames per gate
// without music; returns true when done.
bool Credits_Update(
    const TH02_INPUT *input, bool music_active, uint16_t measure);
