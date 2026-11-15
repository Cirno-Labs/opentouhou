#pragma once

#include "th07/anm.h"

#include "core/base/result.h"
#include "core/math/vec.h"

#include "stg/timer.h"

#include <stdint.h>

// Runs the dialogue scripts (MSG) of a stage and the interface over the
// play area. A dialogue holds the enemy timeline, stops the player from
// shooting and bombing, ends the Border, collects the items, and ends the
// stage: it adds the stage clear bonus and switches the scene. The
// interface keeps the state of the boss bar, the banners, the popups and
// the transition between stages that other modules show. Their sprite
// scripts run in the update; the draw callback records graphics. Timers
// run at the game speed. Names and texts are UTF-8.

#define GUI_SPRITE_COUNT 0x21
#define GUI_MSG_SIDE_COUNT 2 // the player and the boss
#define GUI_MSG_LINE_COUNT 2
#define GUI_TITLE_COUNT 5
#define GUI_BOSS_SECTION_COUNT 8
#define GUI_TILE_COLUMNS 12
#define GUI_TILE_ROWS 14
#define GUI_TILE_COUNT (GUI_TILE_COLUMNS * GUI_TILE_ROWS)
#define GUI_MSG_NONE -1
// Follows the script that ended the stage until the scene switches; the
// dialogue counts as running then.
#define GUI_MSG_TO_NEXT_STAGE -2

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

typedef enum {
    GUI_NOTICE_NONE = 0,
    GUI_NOTICE_FULL_POWER = 1,
    GUI_NOTICE_BORDER_START = 2,
    GUI_NOTICE_CHERRY_MAX = 3,
    GUI_NOTICE_BORDER_BONUS = 4,
} GUI_NOTICE;

// Holds a message shown for a while at a fixed place.
typedef struct {
    bool shown;
    TIMER timer;
    int32_t value;
    VEC3 pos;
} GUI_POPUP;

typedef struct {
    int32_t frames; // counts the stage's update calls

    struct {
        // GUI_MSG_NONE while no dialogue runs, or GUI_MSG_TO_NEXT_STAGE.
        int32_t script;
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
    } msg;

    // Makes the next update request SCENE_NEXT_STAGE.
    bool next_stage_requested;

    // Holds the boss of the boss bar. The enemies and their scripts set
    // these.
    struct {
        bool present; // stops the spawns of the timeline
        int32_t seconds; // left on the timer; scales the spell card bonus
        int32_t lives; // the marks at the left of the bar
        float health; // the life as a fraction of the maximum
        // Hold the bar sections that ECL sets, as fractions of the bar;
        // unused while start is 0. A section shows from end up to start.
        struct {
            float start, end;
            int32_t color;
        } sections[GUI_BOSS_SECTION_COUNT];
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
        ANM_VM titles[GUI_TITLE_COUNT]; // of the stage and its songs
        ANM_VM bomb_portrait, bomb_glow, bomb_frame, bomb_name, bomb_bar;
        ANM_VM spell_portrait, spell_glow, spell_frame, spell_name, spell_bar;
        ANM_VM spell_score_bar, spell_score_digit;
    } banners;

    // Holds the screens between stages. Each sprite shows while it is not
    // negative.
    struct {
        ANM_VM icon;
        ANM_VM caption;
        ANM_VM snapshot; // the play area as the last stage ended
        ANM_VM tiles[GUI_TILE_COUNT]; // break the snapshot up
        int32_t tiles_running; // the tiles whose scripts have not ended
    } loading;

    struct {
        GUI_STAGE_CLEAR state;
        int32_t total; // in tens of points
        int32_t point_items;
        int32_t cherry;
        int32_t graze;
    } stage_clear;

    struct {
        GUI_POPUP bonus; // the bonus of a boss's clear
        GUI_POPUP notice;
        GUI_NOTICE notice_kind;
        GUI_POPUP spell_bonus; // the bonus of a captured spell card
    } popups;
} GUI;

extern GUI g_Gui;

// Adds the update element (priority 13) and the draw element (priority
// 12). Adding loads the portraits, the titles and the dialogue file of
// g_GameManager.run.stage, 1 to 8. Outside SCENE_NEXT_STAGE,
// SCENE_RESTART_STAGE and SCENE_NEXT_STAGE_AGAIN, it also resets g_Gui and
// loads the interface, the portraits of the character and the loading
// screen. In those scenes, it keeps them, copies the play area into the
// snapshot and lays the tiles out. Another stage, a missing file or a
// dialogue file that GuiMsg_Read rejects fails the add.
RESULT Gui_Register(void);

// Removes both elements and releases the stage's files. Outside the three
// scenes that keep the interface, also releases the rest and ends the
// dialogue.
void Gui_Remove(void);

// Starts dialogue script index of the dialogue file. Does nothing when the
// file has no such script. Turns the bullets into items, clears the
// enemies without a bonus and collects the items. An index that is a
// multiple of 10 shows the stage boss's art; on stages 6 to 8 it also sets
// the spell card background and makes the cleared bullets ITEM_BOSS_STAR.
// A negative index is fatal, as it crashes the original.
void Gui_StartMsg(int32_t index);

// Returns whether a dialogue runs, GUI_MSG_TO_NEXT_STAGE included.
bool Gui_IsMsgRunning(void);

// Returns whether a dialogue script runs and grants the timeline no
// release.
bool Gui_MsgHoldsTimeline(void);
bool Gui_IsMsgSkippable(void);

// Returns whether the loading screen is fully up: its icon shows and its
// script waits at a stop. The stage background is not drawn then.
bool Gui_IsLoadingShown(void);

// Shows the name of the bomb with portrait sprite. Name is used only
// during the call.
void Gui_ShowBombName(int32_t sprite, const char *name);

// Makes the name of the bomb and its bar leave.
void Gui_EndBombName(void);

// Shows the name of a spell card with its score bar and, unless sprite is
// negative, portrait sprite 0x4ad + sprite. Name is used only during the
// call.
void Gui_ShowSpellCardName(int32_t sprite, const char *name);

// Makes the name of the spell card, its bar and its score bar leave.
void Gui_EndSpellCardName(void);

// Shows bonus in popups.spell_bonus until its timer reaches 280.
void Gui_ShowSpellCardBonus(int32_t bonus);

// Shows bonus in popups.bonus until its timer reaches 250.
void Gui_ShowBonusPopup(int32_t bonus);

// Each shows its notice in popups.notice until its timer reaches 180; a
// later notice replaces an earlier one.
void Gui_ShowFullPowerPopup(void);
void Gui_ShowCherryFullPopup(int32_t cherry);
void Gui_ShowBorderStartPopup(void);
void Gui_ShowBorderBonusPopup(int32_t points);
