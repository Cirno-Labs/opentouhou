#pragma once

// Clock samples: host times, within one frame, at which the game reached
// points that matter for its audio. The game logic steps per frame, but on
// the PC-98 the music plays on PMD's own timer, so which part of a song a
// frame meets depends on how much real time the frames before it took. A
// DOSBox-X capture records these times and a replay hands them back with
// the frame's input, so that music and effects start on the same audio
// sample as in the capture. Live play fills only the frame and scanout
// times. The music wait and the text layer's timing read them as well.

#include <stddef.h>
#include <stdint.h>

// The points that a frame can pass several times. The nth sample of a kind
// within one frame has key n * TH02_CLOCK_KIND_COUNT + kind.
typedef enum {
    // The frame's VSync.
    TH02_CLOCK_FRAME,
    // The zero of the audio time: PMD's first write of 3Fh to port 18Ah.
    TH02_CLOCK_ORIGIN,
    // PMD's function 6.
    TH02_CLOCK_LOAD,
    // The return from PMD's song start.
    TH02_CLOCK_START,
    // PMD's song stop.
    TH02_CLOCK_STOP,
    // The game reads the song's measure.
    TH02_CLOCK_MEASURE,
    TH02_CLOCK_KIND_COUNT,
} TH02_CLOCK_KIND;

// The remaining keys sit in ranges of their own above the kinds' keys,
// which case scripts store as numbers, so the values never change.

// The frame's scanout, once per frame.
#define TH02_CLOCK_SCANOUT_KEY 65536

// The return from the frame's nth sound effect call is this plus n.
#define TH02_CLOCK_EFFECT_KEY 131072

// The entry of the frame's nth sound effect call is this plus n, taken
// before a pending interrupt runs on the return.
#define TH02_CLOCK_EFFECT_ENTRY_KEY 196608

typedef struct {
    uint32_t key;
    uint64_t microseconds;
} TH02_CLOCK_SAMPLE;
