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
    uint32_t score, highest_score, frame_rand;
    int32_t continues_total, bonus_per_stage[SESSION_HISTORY_COUNT];
    uint16_t continues_per_scene[SESSION_HISTORY_COUNT], point_value;
    int16_t stage_id, lives, bombs, pellet_speed;
    uint8_t route;
    SESSION_END ending;
} SESSION;

// Starts a credit with the current settings and menu random frame. Start
// clears run history. Continue preserves stage, route, highest score, ending
// and the four history slots, but clears score and restores credit resources.
// Zero-initialize once for deterministic Continue before the first Start.
void Session_Begin(
    SESSION *session, SESSION_BEGIN begin, const TH01_SETTINGS *settings,
    uint32_t frame_rand);

// Maps the current stage and route to one of seven cached graphics scenes.
// Requires a normal stage ID in 0..19 and route in 0..1.
uint8_t Session_Scene(const SESSION *session);
