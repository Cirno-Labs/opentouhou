#pragma once

#include "th07/gui.h"

#include "core/base/result.h"
#include "core/io/reader.h"

#include "stg/chain.h"

#include <stdint.h>

#define M_CALC_PRIORITY 13
#define M_DRAW_PRIORITY 12
#define M_FRONT_SLOT 0x15
#define M_FRONT_TEXTURE_SLOT 0x16 // the second ANM of front.anm
#define M_LOADING_SLOT 0x17
#define M_STAGE_TEXT_SLOT 0x18
#define M_FACE_SLOT 0x19
#define M_STAGE_FACE_SLOT 0x1c
#define M_SNAPSHOT_TEXTURE_SLOT 4
#define M_FRONT_SCRIPT 0x600
#define M_LOADING_SCRIPT 0x61e
#define M_SNAPSHOT_SCRIPT 0x725
#define M_TILE_SCRIPT 0x726 // and 0x727, in a checkerboard
#define M_TITLE_SCRIPT 0x800
#define M_SONG_TITLE_SCRIPT 0x804
#define M_STAGE_6_SONG_TITLE_SCRIPT 0x805
#define M_SONG_TITLE_SPRITE 0x803
#define M_LINE_SCRIPT 0x700
#define M_NAMEPLATE_SCRIPT 0x702
#define M_BOMB_NAME_SCRIPT 0x704
#define M_SPELL_NAME_SCRIPT 0x705
#define M_PLAYER_PORTRAIT 0x4a0
#define M_BOMB_PORTRAIT_SCRIPT 0x4a1
#define M_BOSS_PORTRAIT_SCRIPT 0x4a2
#define M_SPELL_PORTRAIT_SCRIPT 0x4a3
#define M_BOMB_GLOW_SCRIPT 0x4a4
#define M_SPELL_GLOW_SCRIPT 0x4a5
#define M_BOMB_FRAME_SCRIPT 0x4a6
#define M_SPELL_FRAME_SCRIPT 0x4a7
#define M_GLOW_SPRITE 0x4ac
#define M_BOSS_PORTRAIT_SPRITE 0x4ad
#define M_BOSS_ART_TARGET_SPRITE 0x609
#define M_STAGE_ART_SPRITE 0x60e // two a stage: the stage, then its boss
#define M_CAPTION_SPRITE 0x10c
#define M_SCORE_DIGIT_SPRITE 0x84
#define M_LINE_FONT_SIZE 15
#define M_NAME_FONT_SIZE 15
#define M_SHOOT_WAIT_FRAMES 12
#define M_BOX_OPEN_FRAMES 60
#define M_BOSS_BAR 11
#define M_BOSS_BAR_ALPHA_STEP 4
#define M_BOSS_BAR_ALPHA_RISE_END 0xfc
#define M_BOSS_BAR_ALPHA_MAX 0xff
#define M_BOSS_HEALTH_RISE 0.01f
#define M_BOSS_HEALTH_FALL 0.02f
#define M_BONUS_POPUP_FRAMES 250
#define M_NOTICE_POPUP_FRAMES 180
#define M_SPELL_BONUS_POPUP_FRAMES 280
#define M_MAX_POWER 128

// Selects what a dialogue instruction does. The script stops on opcodes
// 0, 10 and 11 and never runs past them.
typedef enum {
    GUI_MSG_OP_END = 0,
    GUI_MSG_OP_PORTRAIT_SCRIPT = 1,
    GUI_MSG_OP_PORTRAIT_SPRITE = 2,
    GUI_MSG_OP_LINE = 3,
    GUI_MSG_OP_WAIT = 4,
    GUI_MSG_OP_INTERRUPT = 5,
    GUI_MSG_OP_RELEASE_TIMELINE = 6,
    GUI_MSG_OP_SONG = 7,
    GUI_MSG_OP_NAMEPLATE = 8,
    GUI_MSG_OP_STAGE_CLEAR = 9,
    GUI_MSG_OP_HALT = 10,
    GUI_MSG_OP_NEXT_SCENE = 11,
    GUI_MSG_OP_FADE_OUT_MUSIC = 12,
    GUI_MSG_OP_SKIPPABLE = 13,
    GUI_MSG_OP_FADE_OUT_SCREEN = 14,
} GUI_MSG_OP;

// Holds one parsed dialogue instruction. Opcodes 1, 2, 3, 5 and 8 use
// who: 0 is the player and 1 the boss, and opcode 5 takes 2 and 3 for the
// lines. index is the script or sprite of opcodes 1 and 2, the line of
// opcodes 3 and 8, and the interrupt of opcode 5. value holds the argument
// of opcodes 4, 7 and 13.
typedef struct {
    int32_t time;
    uint8_t opcode;
    int16_t who;
    int16_t index;
    int32_t value;
    char *text; // the text of opcodes 3 and 8
} GUI_MSG_INSTR;

typedef struct {
    GUI_MSG_INSTR *instrs;
    int32_t count;
} GUI_MSG_SCRIPT;

typedef struct {
    GUI_MSG_SCRIPT *scripts;
    int32_t count;
} GUI_MSG_FILE;

// Reads a dialogue file into out. Each script runs to its first opcode 0,
// 10 or 11. A header or an instruction past the file, fewer argument bytes
// than the opcode reads, an opcode above 14, a text without its NUL or not
// in CP932, and a side, a line or a song out of range fail and leave out
// empty.
RESULT GuiMsg_Read(READER *reader, GUI_MSG_FILE *out);
void GuiMsg_Free(GUI_MSG_FILE *file);

CHAIN_STATUS GuiDraw_Frame(void);

// Returns whether the stage clear bonus counts the lives and the bombs:
// after stage 6, and after stage 6 itself outside practice and outside a
// replay that does not hold stage 5.
bool GuiStageClear_CountsLivesAndBombs(void);
