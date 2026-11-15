#pragma once

#include "core/base/result.h"
#include "core/gfx.h"

#include <stdint.h>

// Stands in for a module not yet ported. Each declaration here moves out
// into its own module, under its own name, once that module exists.

// Stands in for the effects quality of the original's configuration, from
// 0 to 2, 2 by default.
extern uint8_t g_EffectsQuality;

// Stands in for the part of th07/gui's state that th07/ascii and
// th07/enemy use.
typedef struct {
    int32_t frames; // counts the stage's update calls
    // Hold the boss bar: whether it shows, the seconds left and the life.
    struct {
        bool present;
        int32_t seconds;
        float health;
    } boss;
} GUI;

extern GUI g_Gui;

RESULT ResultScreen_Register(int32_t mode);
RESULT MusicScreen_Register(void);
RESULT Ending_Register(void);

// Stand in for the gameplay modules that th07/game_manager starts and
// ends with each stage.
RESULT Gui_Register(void);
void Gui_Remove(void);

// Stand in for th07/gui's checks that a dialogue runs, and that the
// dialogue may be skipped.
bool Gui_IsMsgRunning(void);
bool Gui_IsMsgSkippable(void);

// Stand in for th07/gui's start of dialogue index, and its check that the
// dialogue holds the timelines back.
void Gui_StartMsg(int32_t index);
bool Gui_MsgHoldsTimeline(void);

// Stands in for th07/gui's popup of the bonus of a boss's clear.
void Gui_ShowBonusPopup(int32_t bonus);

// Stands in for th07/gui's popup of a full cherry.
void Gui_ShowCherryFullPopup(int32_t cherry);

// Stands in for th07/gui's popup of full power.
void Gui_ShowFullPowerPopup(void);

// Stand in for th07/gui's popups of the Border's start and of its bonus.
void Gui_ShowBorderStartPopup(void);
void Gui_ShowBorderBonusPopup(int32_t points);

// Stand in for th07/gui's display of the bomb's name with portrait sprite,
// and its end. The name stays in use until the next call.
void Gui_ShowBombName(int32_t sprite, const char *name);
void Gui_EndBombName(void);

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
