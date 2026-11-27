#pragma once

#include "th01/settings.h"

#define SESSION_HISTORY_COUNT 4

typedef enum {
    SESSION_ROUTE_MAKAI,
    SESSION_ROUTE_JIGOKU,
} SESSION_ROUTE;

typedef enum {
    SESSION_END_NONE,
    SESSION_END_MAKAI,
    SESSION_END_JIGOKU,
} SESSION_END;

typedef enum {
    SESSION_START,
    SESSION_CONTINUE,
} SESSION_BEGIN;

typedef struct {
    TH01_SETTINGS settings;
    uint32_t score;
    uint32_t highest_score;
    uint32_t frame_rand;
    // Retains the last life-exit checkpoint for the ending RNG seed.
    uint32_t exit_rand;
    // Rank-table leader and live HUD maximum; highest_score tracks the run.
    uint32_t table_high_score;
    uint32_t high_score;
    int32_t continues_total;
    int32_t bonus_per_stage[SESSION_HISTORY_COUNT];
    uint16_t continues_per_scene[SESSION_HISTORY_COUNT];
    uint16_t point_value;
    int16_t stage_id;
    int16_t lives;
    int16_t bombs;
    int16_t pellet_speed;
    uint8_t route;
    SESSION_END ending;
} SESSION;

extern SESSION g_Session;

// Zeroes the session.
void Session_Init(void);

// Starts a credit with the current settings. Start clears run history;
// Continue keeps stage, route and histories but clears score.
// Call Session_Init once before the first Start.
void Session_Begin(
    SESSION_BEGIN begin, const TH01_SETTINGS *settings, uint32_t frame_rand);

// Maps the current stage and route to one of seven cached graphics scenes.
// Requires a normal stage ID in 0..19 and route in 0..1.
uint8_t Session_Scene(void);

// Stores the score, the bombs and the pellet speed of the run.
void Session_Store(uint32_t score, int16_t bombs, int16_t pellet_speed);

// Counts one Continue for the run and for the scene of the stage.
void Session_CountContinue(void);

// Prepares the stage restart after Continue: clears the point value and
// sets the pellet speed and the lives.
void Session_Continue(int16_t pellet_speed, int16_t lives);

// Clears the score, the highest score and the Continue counts.
void Session_ClearHistory(void);

// Clears the ending and returns to the first stage.
void Session_ClearProgress(void);
