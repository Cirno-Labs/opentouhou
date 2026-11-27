#pragma once

#include "th02/tram.h"

typedef struct {
    TRAM_TEXT stages[6];
    TRAM_TEXT music[12];
    TRAM_TEXT blank;
    TRAM_TEXT music_blank;
    uint8_t stage_label[16];
    uint8_t extra_label[16];
    uint8_t demo_label[10];
    uint8_t halfwidths[6];
    ALLOCATOR allocator;
} STAGE_TITLE_ASSETS;

// Loads the stage and music titles; the allocator must free individually.
RESULT StageTitles_Load(const ALLOCATOR *allocator, STAGE_TITLE_ASSETS *out);

// Frees the titles; accepts an empty cache.
void StageTitles_Free(STAGE_TITLE_ASSETS *assets);

// Starts the titles of stage 0..5, keeping assets bound until the stage
// ends; demos show no music title.
void StageTitles_Begin(
    const STAGE_TITLE_ASSETS *assets, uint8_t stage, bool demo);

// Shows a blinking gaiji label instead of a stage banner in demos.
void StageTitles_Reveal(void);

// Clears a normal banner once at world frame 160, retaining demo banners.
void StageTitles_Update(uint32_t frame);

// Draws the music title at timer 1 and clears it at 160.
void StageTitles_UpdateMusic(void);

// Restarts the music title timer for the boss theme.
void StageTitles_Boss(void);
