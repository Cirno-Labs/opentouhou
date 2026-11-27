#pragma once

#include "th02/ending/assets.h"
#include "th02/input.h"
#include "th02/session.h"

typedef enum {
    STORY_MUSIC_INTRO,
    STORY_MUSIC_CREDITS,
} STORY_MUSIC;

// Requests a song; a null callback requests nothing.
typedef void (*STORY_AUDIO)(void *context, STORY_MUSIC song);

void Ending_Init(void);

// Starts the ending at stage 127; it unlocks Extra after a clear without
// continues, and the Extra ending needs all normal clears.
void Ending_Begin(
    const ENDING_ASSETS *assets, const TH02_SESSION *session, STORY_AUDIO music,
    void *music_context, bool extra_unlocked);

// Runs the ending, paced by music progression or counted waits; returns
// true when done.
bool Ending_Update(
    const TH02_INPUT *input, bool music_active, uint16_t measure);
