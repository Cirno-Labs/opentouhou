#pragma once

#include "core/base/result.h"
#include "core/gfx.h"
#include "core/math/vec.h"

#include <stdint.h>

// Stands in for a module not yet ported. Each declaration here moves out
// into its own module, under its own name, once that module exists.

// A rectangle in screen pixels.
typedef struct {
    float x, y, width, height;
} PLAY_RECT;

// Stands in for the part of th07/game_manager's state that the scene
// switch and th07/ascii read and write.
typedef struct {
    struct {
        uint32_t value;
        uint32_t shown; // counts up to value by step a frame
        uint32_t step;
    } score;
    struct {
        float lives;
        float bombs;
        float power;
        int32_t graze; // this stage
        int32_t point_items; // this stage
        int32_t point_items_total;
        int32_t extends; // won by point items
        int32_t next_extend_items; // point_items_total of the next one
    } player;
    // Each counts from offset, which the original adds against tampering.
    struct {
        int32_t value;
        int32_t max;
        int32_t plus; // fills toward the Supernatural Border
        int32_t offset;
    } cherry;
    struct {
        int32_t difficulty; // 0 to 3, 4 for Extra, 5 for Phantasm
        int32_t stage;
        uint8_t character;
        uint8_t shot_type;
        uint8_t shot_index; // character * 2 + shot type
        uint8_t start_lives;
        int32_t max_continues;
        bool score_disabled;
        int32_t stage_frames;
    } run;
    struct {
        uint8_t continues_used;
    } stats;
    struct {
        bool frozen; // holds the screen shake still
        uint8_t paused; // 0, 1 while the pause menu opens, 2 while it is open
        bool in_continue_menu;
        bool in_practice;
        bool in_demo;
        bool in_replay;
        // Counts the calcs that a demo played.
        int32_t demo_frames;
        uint8_t demo_index; // the demo replay played last, from 0 to 2
        // Holds the playback choice of the replay menu: 0 normal, 1 slow,
        // 2 fast.
        uint8_t replay_menu_choice;
        char replay_path[0x200];
        // Set on a calc that slow mode drops. The next calc adds its
        // buttons to those of the dropped calc.
        bool calc_dropped;
    } mode;
    // By difficulty, then the totals.
    struct {
        int32_t plays[7]; // with every shot type, then by shot_index
        int32_t clears;
        int32_t continues;
    } play_counts[7];
    // Holds what the title reads of score.dat.
    struct {
        // By shot_index, the stages cleared on each difficulty, 99 for the
        // whole game: without continues and with.
        struct {
            uint8_t no_continues[6];
            uint8_t with_continues[6];
        } clears[6];
        // By shot_index, stage and difficulty.
        struct {
            uint32_t play_count;
            uint32_t score; // in units of ten points
        } practice[6][6][4];
        // By spell card, the captures with every shot type.
        int32_t spell_captures[0x8d];
        uint32_t play_hours;
    } records;
    // Phantasm was open when the title last came up.
    bool phantasm_was_open;
    PLAY_RECT play_area;
    struct {
        VIEWPORT viewport;
    } camera;
} GAME_MANAGER;

extern GAME_MANAGER g_GameManager;

// Stands in for the part of th07/player's state that th07/ascii reads.
typedef struct {
    VEC3 pos; // play-area pixels
    int32_t border_state; // 1 while the Supernatural Border runs
    int32_t shot_bombs; // the bombs a life of the shot type starts with
} PLAYER;

extern PLAYER g_Player;

// Stands in for the effects quality of the original's configuration, from
// 0 to 2, 2 by default.
extern uint8_t g_EffectsQuality;

// Stands in for the part of th07/gui's state that th07/ascii reads.
typedef struct {
    int32_t frames; // counts the stage's update calls
} GUI;

extern GUI g_Gui;

RESULT GameManager_Register(void);
void GameManager_Remove(void);
RESULT ResultScreen_Register(int32_t mode);
RESULT MusicScreen_Register(void);
RESULT Ending_Register(void);
void Replay_Discard(void);

// Stand in for GameManager::RerollChecksumKeys and SetBombs, which the
// original keeps in ascii.cpp. The reroll draws two random numbers.
void GameManager_RerollChecksumKeys(void);
void GameManager_SetBombs(int32_t bombs);

// Stands in for the reads of score.dat that the title makes into
// g_GameManager.records: LoadClears, LoadPracticeScores, LoadSpellCards and
// the play time.
void ResultScreen_LoadTitleRecords(void);

// Stand in for App::AddTotalPlayTime and the restart of its clock.
void PlayTime_Add(void);
void PlayTime_Restart(void);

// Stands in for the copy of source of the screen, scaled to dest of the
// texture of slot, after the frame. Returns false while a copy is pending.
bool Anm_SetPendingCopy(int32_t slot, GFX_RECT source, GFX_RECT dest);

// Stands in for th07/gui's check that the loading sprite shows.
bool Gui_IsLoadingShown(void);
