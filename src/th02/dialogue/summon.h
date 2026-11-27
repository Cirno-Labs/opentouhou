#pragma once

#include "th02/wait.h"
#include "th02/draw.h"
#include "th02/palette.h"
#include "th02/sound_effects.h"

typedef struct {
    const PATTERN_BANK *patterns;
    void (*draw)(const PATTERN_BANK *patterns);
    WAIT wait;
    uint16_t radius;
    uint8_t fade;
    bool done;
} DIALOGUE_SUMMON;

// Starts the summon, clipping to (32,16)..(416,320). draw, called once the
// circle closes to show whatever the summon reveals, may be null.
void DialogueSummon_Begin(
    DIALOGUE_SUMMON *summon, const PATTERN_BANK *patterns,
    void (*draw)(const PATTERN_BANK *patterns));

// Runs the summon, 150 one-frame then ten three-frame waits; returns true
// when done. A late counter sample does not catch up missed waits.
bool DialogueSummon_Update(DIALOGUE_SUMMON *summon);
