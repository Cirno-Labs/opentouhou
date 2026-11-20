#pragma once

#include "th07/screens/result/types.h"

#include "core/math/vec.h"

#include <stdint.h>

// A rectangle in play-area pixels.
typedef struct {
    float x, y, width, height;
} PLAY_RECT;

// Holds the state of the game being played. The original keeps lives,
// bombs, power, deaths and bombs used as floats behind checksums against
// tampering; the port keeps the floats and every random number the
// checksums draw, and drops the checksums.
typedef struct {
    // Counts in units of ten points.
    struct {
        uint32_t value; // up to 999,999,999
        uint32_t shown; // counts up to value by step a frame
        uint32_t step;
        uint32_t high;
        uint8_t high_continues; // continues_used when high was last set
    } score;

    struct {
        float lives;
        float bombs;
        float power; // up to 128
        int32_t point_items; // this stage
        int32_t point_items_total;
        int32_t extends; // won by point items, -1 once none are left
        int32_t next_extend_items; // point_items_total of the next extend
        int8_t power_item_combo; // power items in a row at full power, up to 30
        int32_t graze; // up to 9999
        int32_t graze_total; // up to 999999
    } player;

    // Counts the cherry points.
    struct {
        int32_t value; // up to max
        int32_t max;
        int32_t plus; // fills toward the Supernatural Border, up to 50000
        int32_t offset; // added to value, max and plus against tampering
    } cherry;

    struct {
        int32_t difficulty; // 0 to 3, 4 for Extra, 5 for Phantasm
        uint8_t character;
        uint8_t shot_type;
        uint8_t shot_index; // character * 2 + shot_type
        int32_t stage; // 1 to 6, 7 for Extra, 8 for Phantasm
        uint16_t rng_seed; // the gameplay seed when the stage started
        int32_t stage_frames;
        int32_t progress_frames; // frames played, which the result shows
        uint8_t start_lives; // a continue restores them
        int32_t max_continues;
        bool score_disabled; // slow mode: no score and no replay are saved
    } run;

    // Moves between min and max in hundredths, through sub.
    struct {
        int32_t value;
        int32_t min;
        int32_t max;
        int32_t sub;
    } rank;

    struct {
        float deaths;
        float bombs_used;
        int32_t spell_cards_captured;
        uint8_t continues_used;
    } stats;

    struct {
        uint8_t paused; // 0, 1 while the pause menu opens, 2 while it is open
        bool in_continue_menu;
        bool playing; // neither menu is open
        bool game_cleared;
        bool in_practice;
        bool in_demo;
        bool in_replay;
        int32_t demo_frames;
        uint8_t demo_index; // the demo replay played last, from 0 to 2
        uint8_t replay_speed; // 0 normal, 1 slow, 2 fast
        char replay_path[0x200];
        bool pause_requested; // the replay marks a pause in the next frame
        // Set on a calc that slow mode drops. The next calc adds its
        // buttons to those of the dropped calc.
        bool calc_dropped;
        uint32_t drop_frames; // counts the calcs of slow playback and slow mode
        bool replay_fps_warning;
        int16_t replay_fps; // the frame rate that slow playback follows
    } mode;

    // Holds the records of score.dat that the game keeps during play.
    struct {
        CLEAR_RECORD clears[SCORE_SHOT_COUNT];
        // By shot_index, stage and difficulty.
        PRACTICE_RECORD practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                                [SCORE_PRACTICE_DIFFICULTY_COUNT];
        SPELL_CARD_RECORD spell_cards[SPELL_CARD_COUNT];
        // Holds spell_cards as the game started; the result screen puts
        // them back when the game records no score.
        SPELL_CARD_RECORD saved_spell_cards[SPELL_CARD_COUNT];
        PLAY_STATS_RECORD play_stats;
        // Phantasm was open when the title last came up.
        bool phantasm_was_open;
    } records;

    // Holds the wall clock milliseconds that play_time.h's functions last
    // measured from, 0 while the play clock is stopped.
    struct {
        uint64_t run_tick;
        uint64_t play_tick;
    } play_time;

    // The play area moves while ScreenEffect_Shake runs; the pause puts
    // it back.
    PLAY_RECT play_area;
    PLAY_RECT player_area; // where the player may move

    struct {
        VIEWPORT viewport;
    } camera;
} GAME_MANAGER;

extern GAME_MANAGER g_GameManager;

// Sets gm up as the game starts: zeroed, with the play area at 32, 16,
// 384 x 448, the last demo at 2, Phantasm open last time and the replay
// frame rate at 60.
void GameManager_Create(GAME_MANAGER *gm);

// Returns whether a box centered at x, y lies at least partly inside a
// 384 x 448 play area. The original ignores the size of play_area.
bool GameManager_IsInPlayArea(float x, float y, float width, float height);

// Returns whether shot (character * 2 + shot type) opens the Extra stage:
// a clear of Easy, Normal, Hard or Lunatic without continues.
bool GameManager_HasExtraOpen(int32_t shot);

bool GameManager_AnyShotHasExtraOpen(void);

// Returns whether shot opens the Phantasm stage. With 60 spell cards
// captured, first marks Phantasm cleared for shot if it cleared Extra, as
// the original does.
bool GameManager_HasPhantasmOpen(int32_t shot);

// Returns whether any shot opens the Phantasm stage, marking the clears
// as GameManager_HasPhantasmOpen does for every shot.
bool GameManager_IsPhantasmOpen(void);

// Adds points to the score, which counts tens, and drops the remainder.
void GameManager_AddScore(int32_t points);

// Each adds to its count, then draws two random numbers as the original's
// checksum update does.
void GameManager_AddLives(int32_t lives);
void GameManager_AddBombs(int32_t bombs);
void GameManager_AddPower(int32_t power);
void GameManager_AddDeaths(int32_t deaths);
void GameManager_AddBombsUsed(int32_t bombs);

// Draws the two random numbers of the original's checksum update, which
// follows each direct change to lives, bombs or power.
void GameManager_DrawChecksumKeys(void);

// Draws the eight random numbers of the original's life checksum keys,
// which a killing hit makes before the death.
void GameManager_DrawLifeChecksumKeys(void);

// Adds amount hundredths to the rank, up to rank.max.
void GameManager_AddRank(int32_t amount);

// Takes amount hundredths from the rank, down to rank.min.
void GameManager_SubRank(int32_t amount);
