#pragma once

#include "th01/boss_strategy.h"
#include "th01/gameplay.h"
#include "th01/invincibility.h"
#include "th01/palette.h"
#include "th01/player.h"
#include "th01/text.h"
#include "th01/vsync_irq.h"

// Shares the state of th01/gameplay between the files of its sequences.

typedef enum {
    GAMEPLAY_SEQUENCE_STAGE,
    GAMEPLAY_SEQUENCE_LIFE,
    GAMEPLAY_SEQUENCE_PAUSE,
    GAMEPLAY_SEQUENCE_EXIT,
    GAMEPLAY_SEQUENCE_END,
    GAMEPLAY_SEQUENCE_TERMINAL,
} GAMEPLAY_SEQUENCE;

typedef struct {
    GAMEPLAY_REQUEST request;
    GAMEPLAY_SEQUENCE sequence;
    VSYNC_WAIT wait;
    bool wait_for_shot;
    bool boss_stage, waiting, bgm_started;
    uint8_t menu_selection, animation_frame;
    int16_t gameover_left[5], gameover_top[5];
    bool gameover_clipped[5];
    uint16_t continue_frame;
    // Ignores confirm at Continue until released, with fix_debouncing.
    bool continue_locked;
    PALETTE_FADE switch_fade;
    PLAYER_BOMB_CONTINUATION player_bomb;
    INVINCIBILITY invincibility;
} GAMEPLAY;

extern GAMEPLAY g_Gameplay;
extern const GAMEPLAY_ASSETS *g_GameplayAssets;

// Returns the sprites of a boss stage or of an ordinary stage.
const PTN *Gameplay_StageSprites(void);

// Copies the score, the bombs and the pellet speed into the session.
void Gameplay_SyncSession(void);

// Returns the strategy of the boss of the stage, or nullptr without a boss.
const BOSS_STRATEGY *Gameplay_Boss(void);

// Returns the loaded assets of the boss of the stage.
const void *Gameplay_BossAssets(void);

// Fills route with the route selection assets and returns the boss context.
BOSS_CONTEXT Gameplay_BossContext(
    ROUTE_SELECT_ASSETS *route, const TH01_INPUT *raw, uint64_t host_tick);

// Calls call with the boss assets and a fresh context and returns its result.
bool Gameplay_BossCall(
    bool (*call)(const void *assets, const BOSS_CONTEXT *ctx),
    const TH01_INPUT *raw, uint64_t host_tick);

// Copies video page from to video page to.
void Gameplay_CopyPage(int32_t from, int32_t to);

// Senses the game input and sets the player hit on the quit key.
void Gameplay_Sense(const TH01_INPUT *raw);

// Draws text in the menu font at left and top.
void Gameplay_Draw(
    const char *text, int32_t left, int32_t top, uint8_t color,
    GAME_TEXT_WEIGHT weight, bool clear);

// Enters the stage start at its first step.
void GameplayStage_Begin(void);

// Runs one step of the stage start and returns whether the tick goes on.
bool GameplayStage_Step(uint64_t host_tick);

// Enters the life at its first step.
void GameplayLife_Begin(void);

// Enters the life at the step after the orb, as the pause menu closes.
void GameplayLife_Resume(void);

// Runs one step of the life and returns whether the tick goes on.
bool GameplayLife_Step(const TH01_INPUT *raw, uint64_t host_tick);

// Dims the palette, draws the pause menu and enters it.
void GameplayPause_Begin(void);

// Runs one step of the pause menu and returns whether the tick goes on.
bool GameplayPause_Step(const TH01_INPUT *raw);

// Enters the life exit, which waits one VSync first.
void GameplayExit_Begin(void);

// Runs one step of the life exit and returns whether the tick goes on.
bool GameplayExit_Step(const TH01_INPUT *raw, uint64_t host_tick);

// Enters the bonus of the stage, or the total bonus after a boss.
void GameplayEnd_BeginBonus(
    int16_t finished, const TH01_INPUT *raw, uint64_t host_tick);

// Draws the hit player and enters the game over animation.
void GameplayEnd_BeginGameover(void);

// Records the ending and the high score and enters the wait before it.
void GameplayEnd_BeginVictory(void);

// Draws the Continue prompt and enters it.
void GameplayEnd_BeginContinue(void);

// Saves the session and blacks out to the opening.
void GameplayEnd_OpenMenu(void);

// Returns whether the palette blacks out for a program switch.
bool GameplayEnd_Switching(void);

// Runs one step of the end of a stage or run and returns whether the tick
// goes on.
bool GameplayEnd_Step(const TH01_INPUT *raw, uint64_t host_tick);
