#pragma once

// The run's state that the original hands from program to program through
// its resident segment: rank, stage, score, power, lives, bombs and the
// continues used, set up for a new game or an Extra start.

#include "th02/settings.h"
#include "th02/sound_effects.h"

#define SESSION_EXTRA_RANK 4

typedef struct {
    uint8_t stage;
    int8_t debug;
    int32_t score;
    uint16_t continues_used;
    int8_t bombs, lives, rank, start_power, music;
    uint8_t start_bombs, start_lives;
    int32_t frame;
    int16_t unused_1;
    int8_t unused_2;
    uint8_t opening_return;
    bool reduce_effects;
    int8_t unused_3;
    uint8_t shot_type;
    int8_t demo_num;
    int16_t skill, unused_4;
    int32_t highest_score;
} TH02_SESSION;

typedef struct {
    SOUND_EFFECT force_effect;
    uint16_t wait_frames;
    bool save_settings, select_shot, enter_main;
} SESSION_EVENTS;

typedef struct {
    int8_t stage, lives, bombs, rank;
    uint8_t power;
    int16_t performance, big_power_override;
} SESSION_ENTRY;

// Clears the session, once per process; new games keep reserved fields.
void Session_Init(TH02_SESSION *session);

// Cancels a pending new game.
void Session_CancelStart(void);

// Begins a new game with the item sound and a 20-frame wait before
// FinishStart.
SESSION_EVENTS Session_BeginStart(bool extra);

// Starts the new game, keeping frame, skill, shot, debug and reduced effects;
// Extra starts at rank 4, stage 5, with two spare lives and one bomb.
SESSION_EVENTS Session_FinishStart(
    TH02_SESSION *session, const TH02_SETTINGS *settings);

// Starts a demo, keeping stage, score, power, frame and high score.
SESSION_EVENTS Session_BeginDemo(
    TH02_SESSION *session, const TH02_SETTINGS *settings, uint8_t demo);

// Fills the MAIN entry from the start counters, not the remaining ones,
// with zero power raised to one.
void Session_EnterMain(const TH02_SESSION *session, SESSION_ENTRY *entry);
