#pragma once

#include "th02/stage/scroll.h"

typedef struct {
    uint32_t frame;
    uint32_t extra_boss_frames;
    uint16_t keys;
    uint16_t vsync_count;
    int16_t midboss_row;
    uint8_t quit;
    uint8_t midboss_active;
    uint8_t progression;
    uint8_t slowdown;
} STAGE_WORLD;

typedef struct {
    STAGE_WORLD world;
    STAGE_SCROLL scroll;
    uint8_t index;
    bool reduce_effects;
} STAGE_STATE;

extern const STAGE_STATE *const g_Stage;

// Clears the stage state, once per run.
void Stage_Init(void);

// Begins a stage, resetting hazards and effects but keeping weather,
// particle records and hazard payloads.
void Stage_Begin(uint8_t stage, bool reduce_effects);

// Clears world and scroll for a new stage.
void Stage_Reset(void);

// Sets the sensed input keys.
void Stage_SetKeys(uint16_t keys);

// Sets the slowdown frame count the next vsync wait reads.
void Stage_SetSlowdown(uint8_t frames);

// Sets the midboss trigger row.
void Stage_SetMidbossRow(int16_t row);

// Restarts the world frame counter.
void Stage_RestartFrames(void);

// Counts one more frame spent fighting the extra boss.
void Stage_CountExtraBossFrame(void);

// Requests the stage quit.
void Stage_Quit(void);

// Clears a pending stage quit.
void Stage_ClearQuit(void);

// Sets the vsync frame count carried between waits.
void Stage_SetVSyncCount(uint16_t count);

// Clears the pending row-change flag.
void Stage_ClearRowChange(void);

// Sets whether the midboss is active.
void Stage_SetMidbossActive(bool active);

// Advances the world frame counter by one.
void Stage_AdvanceFrame(void);

// Sets the boss fight progression.
void Stage_SetProgression(uint8_t progression);

// Zeros the scroll line and address; talk resets them at stage entry.
void Stage_ResetScrollPosition(void);

// Sets the front and back pages to 1 and 0; gameplay resets them at stage
// entry.
void Stage_ResetPages(void);
