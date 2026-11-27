#pragma once

// Sets up th07/gui for tests: its files on test sprites, the game state,
// and stand-ins for the modules it drives. th07_gui_setup.c defines the
// globals of those modules and the stand-ins.

#include "tests/th07/th07_anm_builder.h"
#include "tests/th07/th07_msg_builder.h"
#include "th07/scene.h"

#include "core/base/result.h"

#include <stdint.h>

// The label that the scripts of GuiSetup_WriteAnm show sprite 1 at.
#define GUI_SETUP_LABEL 5

// Counts the calls into the stand-ins and holds their last arguments.
typedef struct {
    int bullets_cleared;
    int enemies_cleared;
    int32_t enemy_max_bonus;
    int32_t enemy_start;
    int items_collected;
    int borders_ended;
    int resaves;
    int screen_fades;
    int32_t screen_fade_duration;
    uint32_t screen_fade_color;
    int scene_requests;
    SCENE requested; // SCENE_QUIT before the first request
} GUI_CALLS;

// Holds what the stand-ins return.
typedef struct {
    SCENE scene; // of Scene_GetRequested
    bool replay_stages[9]; // of Replay_HasStage, during playback
    int32_t spell_captures;
    int32_t spell_attempts;
} GUI_FAKE;

extern GUI_CALLS g_GuiCalls;
extern GUI_FAKE g_GuiFake;

// Writes the data directory of case_name with the files of stage and msg
// as its dialogue file. front.anm holds the lines and the nameplates;
// loading.anm the icon and the snapshot; face_rm00.anm the player's and
// the boss's portraits; std<stage>txt.anm the titles and the song titles.
// Returns the directory, valid until GuiSetup_TearDown.
const char *GuiSetup_WriteData(
    const char *case_name, int32_t stage, const MSG_BUILDER *msg);

// Writes the boss's portraits, the titles and msg as the dialogue file of
// stage.
void GuiSetup_WriteStage(int32_t stage, const MSG_BUILDER *msg);

// Adds an ANM of sprites 16, 16, 200, 300, 16 and 16 wide to builder.
ANM_BUILDER_ANM *GuiSetup_AddAnm(ANM_BUILDER *builder);

// Adds script id, which shows sprite 0, stops, and shows sprite 1 at
// GUI_SETUP_LABEL.
void GuiSetup_AddScript(ANM_BUILDER_ANM *anm, int32_t id);

// Writes builder as name in the data directory.
void GuiSetup_WriteBuilder(const ANM_BUILDER *builder, const char *name);

// Writes the ANM of GuiSetup_AddAnm with the given scripts, each as
// GuiSetup_AddScript adds it.
void GuiSetup_WriteAnm(const char *name, const int32_t *scripts, int32_t count);

// Starts the recording renderer, opens the written data and resets the
// game state to stage, Normal and the gameplay scene.
void GuiSetup_Start(int32_t stage);

// Writes the data, starts and registers the interface.
RESULT GuiSetup_SetUpStage(
    const char *case_name, int32_t stage, const MSG_BUILDER *msg);

// Like GuiSetup_SetUpStage; a failed register is fatal.
void GuiSetup_SetUp(
    const char *case_name, int32_t stage, const MSG_BUILDER *msg);

void GuiSetup_TearDown(void);

// Runs the update chain for frames frames.
void GuiSetup_Run(int32_t frames);

// Holds the play buttons of word from the next frame.
void GuiSetup_Press(uint16_t word);

// Starts dialogue script index and clears the calls that the start made.
void GuiSetup_StartMsg(int32_t index);
