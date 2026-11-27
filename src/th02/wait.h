#pragma once

// Waits that span frames: a number of VSyncs, the next VSync, a key, a
// palette fade or a song measure, checked each frame until they end.

#include "th02/input.h"
#include "th02/palette.h"
#include "th02/vsync_irq.h"

typedef enum {
    WAIT_READY,
    WAIT_DELAY,
    WAIT_EDGE,
    WAIT_KEY,
    WAIT_FADE,
    WAIT_MEASURE,
} WAIT_KIND;

typedef struct {
    WAIT_KIND kind;
    uint16_t count;
    uint16_t measure;
    KEY_WAIT key;
    PALETTE_FADE fade;
    uint8_t key_frames;
    bool music;
} WAIT;

// Clears the pending wait, leaving both interrupt counters.
void Wait_Cancel(WAIT *wait);

// Waits frames VSyncs; clears count1 even for zero frames.
void Wait_Delay(WAIT *wait, uint16_t frames);

// Waits for count1 to change, without clearing it.
void Wait_Edge(WAIT *wait);

// Waits for a key release, then a press.
void Wait_Key(WAIT *wait, const TH02_INPUT *input);

// Waits for a palette fade, advanced when count1 changes.
void Wait_Fade(WAIT *wait, PALETTE_FADE_KIND kind, int16_t speed);

// Waits for the music to reach measure, or 100 frames without music.
void Wait_Measure(WAIT *wait, uint16_t measure, bool music);

// Reports whether the wait is over; a zeroed wait is. Key waits need input
// and measure waits the current measure.
bool Wait_Ready(WAIT *wait, const TH02_INPUT *input, uint16_t measure);
