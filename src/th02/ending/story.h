#pragma once

#include "th02/ending/assets.h"
#include "th02/ending/ending.h"
#include "th02/input.h"
#include "th02/palette.h"
#include "th02/session.h"

void Story_Init(void);

// Starts the story for the current shot type, the good one only without
// continues. Requires page zero, charging off and full-screen clipping.
void Story_Begin(
    const ENDING_ASSETS *assets, const TH02_SESSION *session, STORY_AUDIO music,
    void *music_context);

// Runs the story; returns true when done. The measure-five gate waits for
// the music, or 100 frames without it.
bool Story_Update(const TH02_INPUT *input, bool music_active, uint16_t measure);
