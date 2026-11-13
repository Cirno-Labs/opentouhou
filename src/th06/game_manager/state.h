// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "th06/screens/result/types.h"

#include "core/math/vec.h"

#include <stdint.h>

// A rectangle in play-area pixels.
typedef struct {
    float x, y, width, height;
} PLAY_RECT;

// Holds the state of the game being played.
typedef struct {
    struct {
        uint32_t value;
        uint32_t shown; // counts up to value by step a frame
        uint32_t step;
        uint32_t high;
        int8_t next_extend; // index of the next extend score
    } score;

    struct {
        int8_t lives;
        int8_t bombs;
        uint16_t power;
        uint16_t point_items; // this stage
        uint16_t point_items_total;
        int8_t power_item_combo; // power items in a row at full power, up to 30
        int32_t graze; // up to 9999
        int32_t graze_total; // up to 999999
    } player;

    struct {
        int32_t difficulty;
        uint8_t character;
        uint8_t shot_type;
        int32_t stage; // 1 to 6, 7 for the extra stage
        int32_t last_practice_stage;
        uint16_t rng_seed; // the gameplay seed when the stage started
        int32_t stage_frames;
        int32_t progress_frames; // frames the enemy timelines ran outside boss
                                 // fights
        // Hold the lives and bombs that the game started with: a respawn
        // restores the bombs, and the stage clear bonus counts the lives.
        uint8_t start_lives;
        uint8_t start_bombs;
    } run;

    // Moves between min and max in hundredths, through sub.
    struct {
        int32_t value;
        int32_t min;
        int32_t max;
        int32_t sub;
    } rank;

    struct {
        int32_t deaths;
        int32_t bombs_used;
        int32_t spell_cards_captured;
        uint8_t continues_used;
    } stats;

    struct {
        bool frozen; // an enemy script stops the gameplay timers
        uint8_t paused; // 0, 1 while the pause menu opens, 2 while it is open
        bool in_continue_menu;
        bool playing; // neither menu is open
        bool game_cleared;
        bool in_practice;
        bool in_replay;
        bool in_demo;
        int32_t demo_frames;
        char replay_path[0x200];
    } mode;

    struct {
        SPELL_CARD_RECORD spell_cards[64];
        CLEAR_RECORD clears[4];
        PRACTICE_RECORD practice[4][6][4];
    } records;

    // The play area moves while ScreenEffect_Shake runs, and gameplay
    // reads it then too: the respawn position, GameManager_IsInPlayArea.
    PLAY_RECT play_area;
    PLAY_RECT player_area; // where the player may move

    struct {
        float distance;
        float offset_x, offset_y;
        float zoom;
        VIEWPORT viewport;
        MAT4 view;
        MAT4 projection;
    } camera;
} GAME_MANAGER;

extern GAME_MANAGER g_GameManager;

// Sets gm up for a new game: zeroed, with the play area at 32, 16,
// 384 x 448.
void GameManager_Create(GAME_MANAGER *gm);
// Returns whether a box centred at x, y lies at least partly inside the
// play area.
bool GameManager_IsInPlayArea(float x, float y, float width, float height);
// Returns whether shot of character opens the Extra stage: a clear of
// Normal, Hard or Lunatic without continues. A clear of Easy and a clear
// with continues do not count.
bool GameManager_IsExtraUnlocked(int32_t character, int32_t shot);
// Adds amount hundredths to the rank, up to rank.max.
void GameManager_AddRank(int32_t amount);
// Takes amount hundredths from the rank, down to rank.min.
void GameManager_SubRank(int32_t amount);
