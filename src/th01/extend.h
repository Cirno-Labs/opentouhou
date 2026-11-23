#pragma once

#include "th01/hud.h"
#include "th01/pellets.h"

typedef struct {
    HUD *hud;
    const HUD_CONTEXT *display;
    int8_t *resident_lives;
    int16_t *next;
    void (*effect)(void *user, int16_t number);
    void *user;
} EXTEND_CONTEXT;

// Reconstructs the next threshold from the signed resident score bits at
// main-scene restart: signed division by 400000 plus one, wrapped to 16 bits.
// Fresh games start with next=1; the gameplay owner retains that field.
int16_t Extend_NextForScore(uint32_t resident_score);

// Handles one threshold per call, using the unsigned live gameplay score
// and the wrapped 32-bit product of signed next and 400000. Advances next
// even at the six-life cap. Otherwise updates the real live/resident lives,
// draws the HUD with the previous count, posts effect 15, raises shared pellet
// speed by one raw unit, then advances next. Requires a synchronous audio
// event consumer; it must not change borrowed state. HUD and pellet consumers
// are real module operations. Gameplay cadence and scene integration remain
// external. Allocates nothing and reads no files, fonts, clocks or devices.
void Extend_UpdateAndRender(const EXTEND_CONTEXT *context);
