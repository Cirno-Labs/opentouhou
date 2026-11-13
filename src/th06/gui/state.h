// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "th06/anm.h"
#include "th06/timer.h"

#include "core/base/result.h"

#include <stdint.h>

// Runs the dialogue scripts (MSG) of a stage and the interface over the
// play area. A dialogue holds the enemy timeline, stops the player from
// shooting and bombing, and ends the stage: it adds the stage clear bonus
// and switches the scene. The interface keeps the state of the boss bar,
// the banners and the popups that other modules show. Their sprite
// scripts run in the update and draw callbacks record graphics. While the
// game is frozen, the update does nothing. Names and texts are UTF-8.

#define GUI_SPRITE_COUNT 0x1a
#define GUI_MSG_SIDE_COUNT 2 // the player and the boss
#define GUI_MSG_LINE_COUNT 2

typedef enum {
    GUI_BOSS_BAR_HIDDEN = 0,
    GUI_BOSS_BAR_APPEARING = 1,
    GUI_BOSS_BAR_SHOWN = 2,
    GUI_BOSS_BAR_LEAVING = 3,
} GUI_BOSS_BAR;

typedef enum {
    GUI_STAGE_CLEAR_NONE = 0,
    GUI_STAGE_CLEAR_PENDING = 1, // the next update adds the bonus
    GUI_STAGE_CLEAR_SHOWN = 2,
} GUI_STAGE_CLEAR;

// Holds a message shown for a while at a fixed place.
typedef struct {
    bool shown;
    TIMER timer;
    int32_t value;
    VEC3 pos;
} GUI_POPUP;

typedef struct {
    struct {
        int32_t script; // negative while no dialogue runs
        int32_t pc; // the next instruction of script
        TIMER timer;
        int32_t wait_frames; // since the last line of text showed
        // Holds the releases that the script grants the timeline. The
        // timeline does not wait while it is above 0. Counts down each
        // frame.
        uint32_t timeline_release;
        bool skippable; // the skip button fast-forwards the script
        ANM_VM portraits[GUI_MSG_SIDE_COUNT];
        ANM_VM lines[GUI_MSG_LINE_COUNT];
        ANM_VM nameplates[GUI_MSG_LINE_COUNT];
        // Point at the texts in the loaded dialogue file that the lines and
        // the nameplates show.
        const char *line_texts[GUI_MSG_LINE_COUNT];
        const char *nameplate_texts[GUI_MSG_LINE_COUNT];
    } msg;

    // Holds the boss of the boss bar. The enemies and their scripts set
    // these.
    struct {
        bool present; // stops the spawns of the timeline
        int32_t seconds; // left on the timer; scales the spell card bonus
        int32_t lives; // the number at the left of the bar
        float health; // the life as a fraction of the maximum
    } boss;

    // Holds the interface that only drawing reads.
    struct {
        ANM_VM sprites[GUI_SPRITE_COUNT];
        GUI_BOSS_BAR boss_bar;
        // Rises by 4 a frame to 0xff while the bar shows, and falls by 4
        // while it leaves. It wraps below 0, as unsigned arithmetic does.
        uint32_t boss_bar_alpha;
        float boss_health_shown; // follows boss.health
        int32_t boss_seconds_drawn; // boss.seconds when last drawn
    } hud;

    struct {
        ANM_VM stage_name;
        ANM_VM song_name;
        int32_t song; // the song of g_Stage.data that song_name shows
        ANM_VM bomb_portrait, bomb_name, bomb_bar;
        const char *bomb_text;
        float bomb_width;
        ANM_VM spell_portrait, spell_name, spell_bar;
        const char *spell_text;
        float spell_width;
    } banners;

    // Holds the loading screen between stages. It shows while its sprite
    // is not negative.
    ANM_VM loading;

    struct {
        GUI_STAGE_CLEAR state;
        int32_t total;
    } stage_clear;

    struct {
        GUI_POPUP bonus; // the bonus for cleared bullets
        GUI_POPUP full_power;
        GUI_POPUP spell_bonus; // the bonus of a captured spell card
    } popups;
} GUI;

extern GUI g_Gui;

// Adds the update element (priority 12) and the draw element (priority
// 11). Adding loads the portraits and the dialogue file of
// g_GameManager.run.stage. Outside SCENE_NEXT_STAGE, it also resets g_Gui
// and loads the interface, the loading screen and the portraits of the
// character. In SCENE_NEXT_STAGE, it keeps them and lets the loading
// screen leave. A dialogue file fails the add when a script runs past the
// file, an instruction has fewer arguments than its opcode reads, a text
// is neither UTF-8 nor CP932, or an instruction names a side, a line or a
// song out of range. The jdiff of the dialogue file from the patches then
// changes the text of its lines, as thcrap's MSG patcher does, and a line
// past the second of a box is dropped.
RESULT Gui_Register(void);
// Removes both elements and releases the stage's portraits and dialogue
// file. Outside SCENE_NEXT_STAGE, also releases the rest.
void Gui_Remove(void);

// Starts dialogue script index of the dialogue file. Does nothing when
// the file has no such script. Script 0 or 10 on stage 6 or 7 also loads
// the effect sprites of that stage, and on stage 7 a third portrait of
// the boss. A failed load is logged.
void Gui_StartMsg(int32_t index);
bool Gui_IsMsgRunning(void);
// Returns whether a dialogue runs and grants the timeline no release.
bool Gui_MsgHoldsTimeline(void);
bool Gui_IsMsgSkippable(void);
// Returns whether the loading screen is fully up: its sprite shows and
// its script waits at a stop. The stage background is not drawn then.
bool Gui_IsLoadingShown(void);

// Shows the name of the bomb with portrait sprite. Name stays in use
// until the next call.
void Gui_ShowBombName(int32_t sprite, const char *name);
// Makes the name of the bomb leave.
void Gui_EndBombName(void);
// Shows the name of a spell card with portrait sprite 0x4a8 + sprite.
// Name stays in use until the next call.
void Gui_ShowSpellCardName(int32_t sprite, const char *name);
// Makes the name of the spell card leave.
void Gui_EndSpellCardName(void);
// Shows bonus in popups.spell_bonus until its timer reaches 280.
void Gui_ShowSpellCardBonus(int32_t bonus);
// Shows bonus in popups.bonus until its timer reaches 250.
void Gui_ShowBonusPopup(int32_t bonus);
// Shows popups.full_power until its timer reaches 180.
void Gui_ShowFullPowerPopup(void);
