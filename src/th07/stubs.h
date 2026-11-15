#pragma once

#include "th07/replay/types.h"

#include "core/base/result.h"
#include "core/gfx.h"
#include "core/math/vec.h"

#include <stdint.h>

// Stands in for a module not yet ported. Each declaration here moves out
// into its own module, under its own name, once that module exists.

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

RESULT ResultScreen_Register(int32_t mode);
RESULT MusicScreen_Register(void);
RESULT Ending_Register(void);

// Stand in for the gameplay modules that th07/game_manager starts and
// ends with each stage.
RESULT Player_Register(void);
void Player_Remove(void);
RESULT Bullet_Register(void);
void Bullet_Remove(void);
RESULT Enemy_Register(void);
void Enemy_Remove(void);
RESULT Ecl_Load(void);
void Ecl_Release(void);
RESULT Gui_Register(void);
void Gui_Remove(void);

// Stand in for th07/replay's recording and playback of a stage.
RESULT Replay_Register(REPLAY_MODE mode, const char *path);
void Replay_EndStage(void);
void Replay_Discard(void);

// Stands in for the start of the Supernatural Border.
void Player_StartBorder(void);

// Stand in for th07/gui's checks that a dialogue runs, and that the
// dialogue may be skipped.
bool Gui_IsMsgRunning(void);
bool Gui_IsMsgSkippable(void);

// Stands in for th07/gui's popup of a full cherry.
void Gui_ShowCherryFullPopup(int32_t cherry);

// Stands in for th07/enemy's check that a boss fights.
bool Enemy_IsBossActive(void);

// Stands in for th07/bullet's count of the bullets in play.
int32_t Bullet_Count(void);

// Stands in for the reads of score.dat that a new game makes into
// g_GameManager: the high score and its continues, the spell cards, the
// clears and the practice scores.
RESULT ResultScreen_LoadGameRecords(void);

// Stands in for the reads of score.dat that the title makes into
// g_GameManager.records: LoadClears, LoadPracticeScores, LoadSpellCards and
// the play time.
void ResultScreen_LoadTitleRecords(void);

// Stand in for App::AddTotalPlayTime, the restart of its clock, and the
// stop of that clock with App::AddPlayTime.
void PlayTime_Add(void);
void PlayTime_Restart(void);
void PlayTime_Stop(void);

// Stands in for the copy of source of the screen, scaled to dest of the
// texture of slot, after the frame. Returns false while a copy is pending.
bool Anm_SetPendingCopy(int32_t slot, GFX_RECT source, GFX_RECT dest);

// Stands in for th07/gui's check that the loading sprite shows.
bool Gui_IsLoadingShown(void);
