#include "th06/gui.h"
#include "th06/gui/private.h"

#include "th06/ascii.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/note.h"
#include "th06/scene.h"
#include "th06/sound.h"
#include "th06/stage.h"
#include "th06/strings.h"
#include "th06/translation.h"

#include "core/base/arena.h"
#include "core/base/array.h"
#include "core/base/cp932.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/io/reader.h"
#include "core/json.h"

#include "stg/chain.h"

#include <string.h>
#include <stdio.h>

#define M_DIFF_ARENA_CAPACITY (16 * 1024 * 1024)

// Selects what a dialogue instruction does. The script stops on opcodes
// 0, 10 and 11 and never runs past them.
typedef enum {
    M_OP_END = 0,
    M_OP_PORTRAIT_SCRIPT = 1,
    M_OP_PORTRAIT_SPRITE = 2,
    M_OP_LINE = 3,
    M_OP_WAIT = 4,
    M_OP_INTERRUPT = 5,
    M_OP_RELEASE_TIMELINE = 6,
    M_OP_SONG = 7,
    M_OP_NAMEPLATE = 8,
    M_OP_STAGE_CLEAR = 9,
    M_OP_HALT = 10,
    M_OP_NEXT_SCENE = 11,
    M_OP_FADE_OUT_MUSIC = 12,
    M_OP_SKIPPABLE = 13,
} M_OP;

// Holds one parsed dialogue instruction. Opcodes 1, 2, 3, 5 and 8 use
// who: 0 is the player and 1 the boss, and opcode 5 takes 2 and 3 for the
// lines. index is the script or sprite of opcodes 1 and 2, the line of
// opcodes 3 and 8, and the label of opcode 5. value holds the argument of
// opcodes 4, 7 and 13.
typedef struct {
    int32_t time;
    uint8_t opcode;
    int16_t who;
    int16_t index;
    int32_t value;
    char *text; // the text of opcodes 3 and 8, in UTF-8
    bool removes_note; // drawing text hides the shown translator note
} M_INSTR;

typedef struct {
    M_INSTR *instrs;
    int32_t count;
} M_SCRIPT;

typedef ARRAY(M_INSTR) M_INSTR_ARRAY;

// Holds the state of applying the jdiff of one script, as thcrap's MSG
// patcher keeps it. A box is the lines from a line 0 to the next line 0.
typedef struct {
    const JSON_VALUE *entry;
    M_INSTR_ARRAY out;
    int32_t time;
    int32_t box; // counts the line instructions of a time without a diff
    const JSON_VALUE *lines; // the lines of the box, or nullptr
    int32_t line;
    int32_t last_opcode; // the opcode of the last replaced line, or -1
    int32_t last_line; // the position in out of the last replaced line
    // Holds the lines of the box whose replaced line had a translator note.
    // Holds entry until the first replaced line, and nullptr after a line
    // that hides the note.
    const JSON_VALUE *noted;
} M_PATCH_STATE;

// Tells how a frame of the dialogue script ended.
typedef enum {
    M_RUN_ENDED,
    M_RUN_WAITING, // the next instruction is not due yet
    M_RUN_HALTED, // an instruction holds the script and its timer
} M_RUN;

typedef struct {
    const char *faces[M_STAGE_FACE_COUNT];
    const char *msg;
} M_STAGE_FILES;

GUI g_Gui;

static const int32_t m_FaceFirstSprites[M_FACE_COUNT] = { 0x4a0, 0x4a2, 0x4a4 };
static const char *const m_Faces[][M_FACE_COUNT] = {
    { "data/face00a.anm", "data/face00b.anm", "data/face00c.anm" },
    { "data/face01a.anm", "data/face01b.anm", "data/face01c.anm" },
};

static const uint32_t m_TextColors[GUI_MSG_SIDE_COUNT] = { 0xe8f0ff, 0xffe8f0 };

static const int32_t m_StageFaceFirstSprites[M_STAGE_FACE_COUNT] = {
    0x4a8,
    0x4aa,
    0x4ab,
};
// Holds the files of stages 1 to 6, then those of every other stage.
static const M_STAGE_FILES m_StageFiles[] = {
    { { "data/face03a.anm", "data/face03b.anm" }, "data/msg1.dat" },
    { { "data/face05a.anm" }, "data/msg2.dat" },
    { { "data/face06a.anm", "data/face06b.anm" }, "data/msg3.dat" },
    { { "data/face08a.anm", "data/face08b.anm" }, "data/msg4.dat" },
    { { "data/face09a.anm", "data/face09b.anm" }, "data/msg5.dat" },
    { { "data/face09b.anm", "data/face10a.anm", "data/face10b.anm" },
      "data/msg6.dat" },
    { { "data/face08a.anm", "data/face12a.anm", "data/face12b.anm" },
      "data/msg7.dat" },
};

static M_SCRIPT *m_Scripts;
static int32_t m_ScriptCount;
static CHAIN_ELEM m_CalcElem;
static CHAIN_ELEM m_DrawElem;

static bool M_IsFinal(const uint8_t opcode)
{
    return opcode == M_OP_END || opcode == M_OP_HALT
        || opcode == M_OP_NEXT_SCENE;
}

static size_t M_ArgsSize(const uint8_t opcode)
{
    switch (opcode) {
    case M_OP_PORTRAIT_SCRIPT:
    case M_OP_PORTRAIT_SPRITE:
    case M_OP_WAIT:
    case M_OP_SONG:
        return 4;
    case M_OP_LINE:
    case M_OP_NAMEPLATE:
        return 5;
    case M_OP_INTERRUPT:
        return 3;
    case M_OP_SKIPPABLE:
        return 1;
    default:
        return 0;
    }
}

static RESULT M_CheckInstr(const M_INSTR *const instr)
{
    switch (instr->opcode) {
    case M_OP_PORTRAIT_SCRIPT:
    case M_OP_PORTRAIT_SPRITE:
        FAIL_IF(
            instr->who < 0 || instr->who >= GUI_MSG_SIDE_COUNT,
            "a portrait instruction names side %d", instr->who);
        return OK;
    case M_OP_LINE:
    case M_OP_NAMEPLATE:
        FAIL_IF(
            instr->index < 0 || instr->index >= GUI_MSG_LINE_COUNT,
            "a text instruction names line %d", instr->index);
        return OK;
    case M_OP_INTERRUPT:
        FAIL_IF(
            instr->who < 0
                || instr->who >= GUI_MSG_SIDE_COUNT + GUI_MSG_LINE_COUNT,
            "an interrupt names sprite %d", instr->who);
        return OK;
    case M_OP_SONG:
        FAIL_IF(
            instr->value < 0 || instr->value >= STAGE_SONG_COUNT,
            "a song instruction names song %d", instr->value);
        return OK;
    default:
        return OK;
    }
}

// Reads the instruction at the position into instr, and moves to the next
// one. On failure, instr->text may hold a text to release.
static RESULT M_ReadInstr(READER *const reader, M_INSTR *const instr)
{
    const size_t start = Reader_Tell(reader);
    instr->time = Reader_ReadU16(reader);
    instr->opcode = Reader_ReadU8(reader);
    const size_t size = Reader_ReadU8(reader);
    FAIL_IF(
        Reader_Failed(reader) || size > Reader_Remaining(reader),
        "the instruction at 0x%zx runs past the file", start);
    FAIL_IF(
        size < M_ArgsSize(instr->opcode),
        "the instruction at 0x%zx has %zu bytes of arguments", start, size);
    uint8_t args[UINT8_MAX] = {};
    Reader_ReadBytes(reader, args, size);
    READER fields;
    Reader_OpenBuffer(&fields, args, size);
    switch (instr->opcode) {
    case M_OP_WAIT:
    case M_OP_SONG:
        instr->value = Reader_ReadI32(&fields);
        break;
    case M_OP_SKIPPABLE:
        instr->value = Reader_ReadU8(&fields);
        break;
    case M_OP_INTERRUPT:
        instr->who = Reader_ReadI16(&fields);
        instr->index = Reader_ReadU8(&fields);
        break;
    case M_OP_LINE:
    case M_OP_NAMEPLATE: {
        instr->who = Reader_ReadI16(&fields);
        instr->index = Reader_ReadI16(&fields);
        const uint8_t *const text = &args[4];
        const uint8_t *const end = memchr(text, '\0', size - 4);
        FAIL_IF(
            end == nullptr, "the text at 0x%zx runs past its instruction",
            start);
        MUST(
            Cp932_DecodeText(
                (const char *)text, (size_t)(end - text), &instr->text),
            "the text at 0x%zx", start);
        break;
    }
    default:
        instr->who = Reader_ReadI16(&fields);
        instr->index = Reader_ReadI16(&fields);
        break;
    }
    return M_CheckInstr(instr);
}

// Parses the script at offset up to the first instruction it stops on.
static RESULT M_ParseScript(
    READER *const reader, const size_t offset, M_SCRIPT *const script)
{
    Reader_Seek(reader, offset);
    int32_t count = 0;
    M_INSTR instr;
    do {
        instr = (M_INSTR) {};
        const RESULT result = M_ReadInstr(reader, &instr);
        Mem_Free(instr.text);
        MUST(result, "instruction %d", count);
        count++;
    } while (!M_IsFinal(instr.opcode));

    script->instrs = Mem_AllocArrZ((size_t)count, sizeof(M_INSTR));
    script->count = count;
    Reader_Seek(reader, offset);
    for (int32_t i = 0; i < count; i++) {
        MUST(M_ReadInstr(reader, &script->instrs[i]));
    }
    return OK;
}

static RESULT M_ParseMsg(READER *const reader)
{
    const int32_t count = Reader_ReadI32(reader);
    FAIL_IF(
        Reader_Failed(reader) || count < 0
            || (size_t)count > Reader_Remaining(reader) / 4,
        "bad header");
    m_Scripts = Mem_AllocArrZ((size_t)count, sizeof(M_SCRIPT));
    m_ScriptCount = count;
    for (int32_t i = 0; i < count; i++) {
        Reader_Seek(reader, 4 + 4 * (size_t)i);
        const int32_t offset = Reader_ReadI32(reader);
        FAIL_IF(offset < 0, "script %d starts at %d", i, offset);
        MUST(
            M_ParseScript(reader, (size_t)offset, &m_Scripts[i]), "script %d",
            i);
    }
    return OK;
}

static void M_FreeMsg(void)
{
    for (int32_t i = 0; i < m_ScriptCount; i++) {
        for (int32_t j = 0; j < m_Scripts[i].count; j++) {
            Mem_Free(m_Scripts[i].instrs[j].text);
        }
        Mem_Free(m_Scripts[i].instrs);
    }
    Mem_Free(m_Scripts);
    m_Scripts = nullptr;
    m_ScriptCount = 0;
    for (int32_t i = 0; i < GUI_MSG_LINE_COUNT; i++) {
        g_Gui.msg.line_texts[i] = nullptr;
        g_Gui.msg.nameplate_texts[i] = nullptr;
    }
}

static void M_SetText(M_INSTR *const instr, const char *const text)
{
    Mem_Free(instr->text);
    instr->text = Str_Dup(text);
}

// Gives instr a replaced line of the box. As thcrap's MSG patcher marks
// the lines, the line hides the shown translator note when it has no note
// and it is the first replaced line of the script, or the first one of
// another box after a line with a note.
static void M_TakeLine(
    M_PATCH_STATE *const state, M_INSTR *const instr, const char *const text)
{
    M_SetText(instr, text);
    instr->removes_note = false;
    if (strchr(text, '\x14') != nullptr) {
        state->noted = state->lines;
    } else if (state->noted != nullptr && state->noted != state->lines) {
        instr->removes_note = true;
        state->noted = nullptr;
    }
}

// Adds the lines of the box that its instructions did not take, after the
// last replaced line, as copies of it.
static void M_EndBox(M_PATCH_STATE *const state)
{
    if (state->last_opcode >= 0 && state->lines != nullptr) {
        const int32_t count = state->lines->array.count;
        for (; state->line < count; state->line++) {
            const char *const text =
                Json_GetString(Json_At(state->lines, state->line), nullptr);
            if (text == nullptr) {
                continue;
            }
            if (state->line >= GUI_MSG_LINE_COUNT) {
                LOG_WARNING(
                    "Dropped the dialogue line %d at time %d: a box has %d "
                    "lines",
                    state->line, state->time, GUI_MSG_LINE_COUNT);
                continue;
            }
            M_INSTR instr = state->out.items[state->last_line];
            instr.index = (int16_t)state->line;
            instr.text = nullptr;
            M_TakeLine(state, &instr, text);
            state->last_line++;
            Array_Insert(&state->out, state->last_line, instr);
        }
    }
    state->line = 0;
    state->lines = nullptr;
}

// Applies the diff to the line instruction instr, which is the last of
// out. Returns whether instr stays.
static bool M_PatchLine(M_PATCH_STATE *const state, M_INSTR *const instr)
{
    if (state->lines == nullptr) {
        state->box++;
        char key[48];
        if (instr->opcode == M_OP_NAMEPLATE) {
            snprintf(key, sizeof(key), "%d_h1_%d", state->time, state->box);
        } else {
            snprintf(key, sizeof(key), "%d_%d", state->time, state->box);
        }
        const JSON_VALUE *lines = Json_Get(state->entry, key);
        if (lines != nullptr && lines->type == JSON_OBJECT) {
            lines = Json_Get(lines, "lines");
        }
        if (lines != nullptr && lines->type == JSON_ARRAY) {
            state->lines = lines;
        }
    }
    if (state->lines == nullptr) {
        return true;
    }
    const char *const text =
        Json_GetString(Json_At(state->lines, state->line), nullptr);
    state->line++;
    if (text == nullptr) {
        return false;
    }
    M_TakeLine(state, instr, text);
    state->last_opcode = instr->opcode;
    state->last_line = state->out.count - 1;
    return true;
}

// Returns whether the box counter starts again for a line 0 of opcode
// after a line of last_opcode. thcrap keeps counting only between two
// lines of opcode 3.
static bool M_RestartsBoxes(const int32_t last_opcode, const uint8_t opcode)
{
    return last_opcode == M_OP_NAMEPLATE || opcode == M_OP_NAMEPLATE;
}

// Applies the jdiff entry of a script, as thcrap's th06 MSG patcher does:
// each box of a time code takes its lines from "<time>_<box>", or from
// "<time>_h1_<box>" for nameplates. Fewer lines delete the extra line
// instructions; more lines add copies of the last one.
static void M_PatchScript(M_SCRIPT *const script, const JSON_VALUE *const entry)
{
    M_PATCH_STATE state = {
        .entry = entry,
        .time = -1,
        .box = -1,
        .last_opcode = -1,
        .noted = entry,
    };
    Array_Init(&state.out, &g_HeapAllocator);
    for (int32_t i = 0; i < script->count; i++) {
        M_INSTR instr = script->instrs[i];
        if (M_IsFinal(instr.opcode)) {
            M_EndBox(&state);
        }
        if (instr.time != state.time) {
            state.time = instr.time;
            state.box = -1;
        }
        Array_Append(&state.out, instr);
        if (instr.opcode != M_OP_LINE && instr.opcode != M_OP_NAMEPLATE) {
            continue;
        }
        if (instr.index == 0) {
            M_EndBox(&state);
            if (state.last_opcode >= 0
                && M_RestartsBoxes(state.last_opcode, instr.opcode)) {
                state.box = -1;
            }
        }
        state.line = instr.index;
        M_INSTR *const last = &state.out.items[state.out.count - 1];
        if (!M_PatchLine(&state, last)) {
            Mem_Free(last->text);
            state.out.count--;
        }
    }
    Mem_Free(script->instrs);
    script->instrs = Mem_AllocArr((size_t)state.out.count, sizeof(M_INSTR));
    memcpy(
        script->instrs, state.out.items,
        (size_t)state.out.count * sizeof(M_INSTR));
    script->count = state.out.count;
    Array_Free(&state.out);
}

static void M_PatchMsg(const char *const path)
{
    ARENA arena;
    Arena_Init(&arena, "dialogue jdiff", M_DIFF_ARENA_CAPACITY);
    const JSON_VALUE *const diff =
        Files_LoadDiff(path, Arena_Allocator(&arena));
    for (int32_t i = 0; diff != nullptr && i < m_ScriptCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%d", i);
        const JSON_VALUE *const entry = Json_Get(diff, key);
        if (entry != nullptr && entry->type == JSON_OBJECT) {
            M_PatchScript(&m_Scripts[i], entry);
        }
    }
    Arena_Free(&arena);
}

static RESULT M_LoadMsg(const char *const path)
{
    M_FreeMsg();
    READER reader;
    MUST(Files_Load(path, &reader), "%s", path);
    const RESULT result = M_ParseMsg(&reader);
    Reader_Close(&reader);
    if (!IS_OK(result)) {
        M_FreeMsg();
    }
    MUST(result, "%s", path);
    M_PatchMsg(path);
    g_Gui.msg.script = -1;
    g_Gui.msg.pc = 0;
    return OK;
}

static int32_t M_Wrap(const int64_t value)
{
    return (int32_t)(uint32_t)(uint64_t)value;
}

static const M_STAGE_FILES *M_GetStageFiles(void)
{
    const int32_t stage = g_GameManager.run.stage;
    if (stage >= 1 && stage <= 6) {
        return &m_StageFiles[stage - 1];
    }
    return &m_StageFiles[6];
}

static void M_UpdateBossBar(void)
{
    ANM_VM *const bar = &g_Gui.hud.sprites[M_BOSS_BAR];
    if (g_Gui.boss.present) {
        if (g_Gui.hud.boss_bar == GUI_BOSS_BAR_HIDDEN) {
            Anm_StartScript(bar, M_BOSS_BAR_APPEAR_SCRIPT);
            g_Gui.hud.boss_bar = GUI_BOSS_BAR_APPEARING;
            g_Gui.hud.boss_bar_alpha = 0;
        } else {
            // A bar that leaves when a boss comes back counts as shown
            // once its leaving script ends.
            if (Anm_Step(bar)) {
                g_Gui.hud.boss_bar = GUI_BOSS_BAR_SHOWN;
            }
            if (g_Gui.hud.boss_bar_alpha < M_BOSS_BAR_ALPHA_RISE_END) {
                g_Gui.hud.boss_bar_alpha += M_BOSS_BAR_ALPHA_STEP;
            } else {
                g_Gui.hud.boss_bar_alpha = M_BOSS_BAR_ALPHA_MAX;
            }
        }
    } else if (g_Gui.hud.boss_bar != GUI_BOSS_BAR_HIDDEN) {
        if (g_Gui.hud.boss_bar <= GUI_BOSS_BAR_SHOWN) {
            Anm_StartScript(bar, M_BOSS_BAR_LEAVE_SCRIPT);
            g_Gui.hud.boss_bar = GUI_BOSS_BAR_LEAVING;
        }
        // From 0xff, the alpha passes 3 and wraps below 0.
        if (g_Gui.hud.boss_bar_alpha > 0) {
            g_Gui.hud.boss_bar_alpha -= M_BOSS_BAR_ALPHA_STEP;
        } else {
            g_Gui.hud.boss_bar_alpha = 0;
        }
        if (Anm_Step(bar)) {
            g_Gui.hud.boss_bar = GUI_BOSS_BAR_HIDDEN;
            g_Gui.hud.boss_health_shown = 0.0f;
            g_Gui.hud.boss_bar_alpha = 0;
        }
    }
    if (g_Gui.hud.boss_bar >= GUI_BOSS_BAR_SHOWN) {
        const float health = g_Gui.boss.health;
        float *const shown = &g_Gui.hud.boss_health_shown;
        if (health > *shown) {
            *shown += M_BOSS_HEALTH_RISE;
            if (health < *shown) {
                *shown = health;
            }
        } else if (health < *shown) {
            *shown -= M_BOSS_HEALTH_FALL;
            if (health > *shown) {
                *shown = health;
            }
        }
    }
}

static void M_UpdatePopup(
    GUI_POPUP *const popup, const int32_t frames, const bool slide)
{
    if (!popup->shown) {
        return;
    }
    if (slide) {
        popup->pos.x = popup->timer.current < 30
            ? (float)popup->timer.current * -312.0f / 30.0f + 416.0f
            : 104.0f;
    }
    if (popup->timer.current >= frames) {
        popup->shown = false;
    }
    Timer_Tick(&popup->timer);
}

static void M_AddStageClearBonus(void)
{
    const GAME_MANAGER *const gm = &g_GameManager;
    int32_t bonus = 0;
    bonus += gm->run.stage * 1000;
    bonus += gm->player.graze * 10;
    bonus += gm->player.power * 100;
    bonus = M_Wrap((int64_t)bonus * gm->player.point_items);
    if (gm->run.stage >= 6) {
        bonus += gm->player.lives * 3000000;
        bonus += gm->player.bombs * 1000000;
    }
    switch (gm->run.difficulty) {
    case 0:
        bonus /= 2;
        bonus -= bonus % 10;
        break;
    case 2:
        bonus = M_Wrap((int64_t)bonus * 12) / 10;
        bonus -= bonus % 10;
        break;
    case 3:
        bonus = M_Wrap((int64_t)bonus * 15) / 10;
        bonus -= bonus % 10;
        break;
    case 4:
        bonus = M_Wrap((int64_t)bonus * 2);
        bonus -= bonus % 10;
        break;
    default:
        break;
    }
    // The penalty for starting with more lives applies on every
    // difficulty and in practice, though the stage clear text shows it
    // only below the extra difficulty and outside practice.
    switch (gm->run.start_lives) {
    case 3:
        bonus = M_Wrap((int64_t)bonus * 5) / 10;
        bonus -= bonus % 10;
        break;
    case 4:
        bonus = M_Wrap((int64_t)bonus * 2) / 10;
        bonus -= bonus % 10;
        break;
    default:
        break;
    }
    g_Gui.stage_clear.total = bonus;
    g_GameManager.score.value += (uint32_t)bonus;
    g_Gui.stage_clear.state = GUI_STAGE_CLEAR_SHOWN;
}

static void M_UpdateInterface(void)
{
    for (int32_t i = 0; i < GUI_SPRITE_COUNT; i++) {
        if (i == M_BOSS_BAR && g_Gui.msg.script < 0) {
            M_UpdateBossBar();
        } else {
            Anm_Step(&g_Gui.hud.sprites[i]);
        }
    }
    Anm_Step(&g_Gui.banners.stage_name);
    Anm_Step(&g_Gui.banners.song_name);
    Anm_Step(&g_Gui.banners.bomb_portrait);
    Anm_Step(&g_Gui.banners.bomb_name);
    Anm_Step(&g_Gui.banners.spell_portrait);
    Anm_Step(&g_Gui.banners.spell_name);
    if (g_Gui.loading.sprite >= 0 && Anm_Step(&g_Gui.loading)) {
        g_Gui.loading.sprite = -1;
    }
    M_UpdatePopup(&g_Gui.popups.bonus, M_BONUS_POPUP_FRAMES, true);
    M_UpdatePopup(&g_Gui.popups.full_power, M_FULL_POWER_POPUP_FRAMES, true);
    M_UpdatePopup(&g_Gui.popups.spell_bonus, M_SPELL_BONUS_POPUP_FRAMES, false);
    if (g_Gui.stage_clear.state == GUI_STAGE_CLEAR_PENDING) {
        M_AddStageClearBonus();
    }
}

static bool M_SkipHeld(void)
{
    return g_Gui.msg.skippable && g_Buttons.held.skip;
}

static void M_NextScene(void)
{
    GAME_MANAGER *const gm = &g_GameManager;
    gm->score.shown = gm->score.value;
    if (gm->mode.in_practice) {
        Scene_Request(SCENE_GAME_RESULT);
    } else if (
        gm->run.stage < 5 || (gm->run.difficulty != 0 && gm->run.stage == 5)) {
        Scene_Request(SCENE_NEXT_STAGE);
    } else if (gm->mode.in_replay) {
        Scene_Request(SCENE_REPLAY_END);
    } else if (gm->run.difficulty == 4) {
        gm->mode.game_cleared = true;
        Scene_Request(SCENE_GAME_RESULT);
    } else {
        Scene_Request(SCENE_ENDING);
    }
}

static void M_ShowLine(const M_INSTR *const instr)
{
    if (instr->index == 0 && g_Gui.msg.lines[1].script_index >= 0) {
        g_Gui.msg.line_texts[1] = " ";
        Result_Log(
            Anm_RenderText(
                &g_Gui.msg.lines[1], m_TextColors[instr->who], 0, "%s", " "),
            LOG_LEVEL_WARNING);
    }
    ANM_VM *const line = &g_Gui.msg.lines[instr->index];
    Anm_StartScript(line, instr->index + M_LINE_SCRIPT);
    line->font_height = M_LINE_FONT_SIZE;
    line->font_width = line->font_height;
    g_Gui.msg.line_texts[instr->index] = instr->text;
    if (instr->removes_note) {
        Note_Remove();
    }
    Result_Log(
        Anm_RenderText(line, m_TextColors[instr->who], 0, "%s", instr->text),
        LOG_LEVEL_WARNING);
    g_Gui.msg.wait_frames = 0;
}

// Returns whether the wait of instr holds the script this frame. The
// shoot button ends it only after 8 frames, and only on the frame it is
// pressed.
static bool M_Waits(const M_INSTR *const instr)
{
    if (M_SkipHeld()
        || (g_Buttons.pressed.shoot
            && g_Gui.msg.wait_frames >= M_SHOOT_WAIT_FRAMES)) {
        return false;
    }
    if (g_Gui.msg.wait_frames >= instr->value) {
        return false;
    }
    g_Gui.msg.wait_frames++;
    return true;
}

static M_RUN M_RunInstrs(void)
{
    const M_SCRIPT *const script = &m_Scripts[g_Gui.msg.script];
    if (M_SkipHeld()) {
        Timer_Set(&g_Gui.msg.timer, script->instrs[g_Gui.msg.pc].time);
    }
    for (;;) {
        const M_INSTR *const instr = &script->instrs[g_Gui.msg.pc];
        if (g_Gui.msg.timer.current < instr->time) {
            return M_RUN_WAITING;
        }
        switch (instr->opcode) {
        case M_OP_END:
            g_Gui.msg.script = -1;
            return M_RUN_ENDED;
        case M_OP_PORTRAIT_SCRIPT:
            Anm_StartScript(
                &g_Gui.msg.portraits[instr->who],
                instr->index
                    + (instr->who != 0 ? M_BOSS_PORTRAIT_SCRIPT
                                       : M_PLAYER_PORTRAIT));
            break;
        case M_OP_PORTRAIT_SPRITE:
            Anm_SetSprite(
                &g_Gui.msg.portraits[instr->who],
                instr->index
                    + (instr->who != 0 ? M_BOSS_PORTRAIT_SPRITE
                                       : M_PLAYER_PORTRAIT));
            break;
        case M_OP_LINE:
            M_ShowLine(instr);
            break;
        case M_OP_WAIT:
            if (M_Waits(instr)) {
                return M_RUN_HALTED;
            }
            break;
        case M_OP_INTERRUPT:
            if (instr->who < GUI_MSG_SIDE_COUNT) {
                g_Gui.msg.portraits[instr->who].pending_interrupt =
                    instr->index;
            } else {
                g_Gui.msg.lines[instr->who - GUI_MSG_SIDE_COUNT]
                    .pending_interrupt = instr->index;
            }
            break;
        case M_OP_RELEASE_TIMELINE:
            g_Gui.msg.timeline_release++;
            break;
        case M_OP_SONG:
            GuiTextImage_ShowBossSong(g_GameManager.run.stage);
            Anm_StartScript(&g_Gui.banners.song_name, M_SONG_NAME_SCRIPT);
            g_Gui.banners.song_name.font_width = M_SONG_FONT_SIZE;
            g_Gui.banners.song_name.font_height = M_SONG_FONT_SIZE;
            g_Gui.banners.song = instr->value;
            if (instr->value >= 0 && instr->value < STAGE_SONG_COUNT
                && g_Stage.data.song_names[instr->value] != nullptr
                && !GuiTextImage_Replaces(M_SONG_NAME_SCRIPT)) {
                Result_Log(
                    Anm_RenderTextRightAligned(
                        &g_Gui.banners.song_name, 0xe0ffff, 0,
                        g_Strings[STRING_SONG_NAME_FORMAT],
                        Translation_GetSongName(
                            g_GameManager.run.stage, instr->value,
                            g_Stage.data.song_names[instr->value])),
                    LOG_LEVEL_WARNING);
            }
            if (g_Stage.data.song_paths[instr->value] != nullptr) {
                Sound_PlayMusic(g_Stage.data.song_paths[instr->value]);
            }
            break;
        case M_OP_NAMEPLATE: {
            ANM_VM *const nameplate = &g_Gui.msg.nameplates[instr->index];
            Anm_StartScript(nameplate, instr->index + M_NAMEPLATE_SCRIPT);
            g_Gui.msg.nameplate_texts[instr->index] = instr->text;
            if (!GuiTextImage_Replaces(M_NAMEPLATE_SCRIPT)) {
                if (instr->removes_note) {
                    Note_Remove();
                }
                Result_Log(
                    Anm_RenderTextRightAligned(
                        nameplate, m_TextColors[instr->who], 0, "%s",
                        instr->text),
                    LOG_LEVEL_WARNING);
            }
            g_Gui.msg.wait_frames = 0;
            break;
        }
        case M_OP_STAGE_CLEAR:
            g_Gui.stage_clear.state = GUI_STAGE_CLEAR_PENDING;
            if (g_GameManager.run.stage < 6) {
                Anm_StartScript(&g_Gui.loading, M_LOADING_SCRIPT);
            } else {
                g_GameManager.score.next_extend = -1;
            }
            break;
        case M_OP_FADE_OUT_MUSIC:
            Sound_FadeOutMusic(4.0f);
            break;
        case M_OP_HALT:
            return M_RUN_HALTED;
        case M_OP_NEXT_SCENE:
            M_NextScene();
            return M_RUN_HALTED;
        case M_OP_SKIPPABLE:
            g_Gui.msg.skippable = instr->value != 0;
            break;
        default:
            break;
        }
        g_Gui.msg.pc++;
    }
}

static void M_RunMsg(void)
{
    if (g_Gui.msg.script < 0) {
        return;
    }
    if (g_Gui.msg.timeline_release > 0) {
        g_Gui.msg.timeline_release--;
    }
    switch (M_RunInstrs()) {
    case M_RUN_ENDED:
        return;
    case M_RUN_WAITING:
        Timer_Tick(&g_Gui.msg.timer);
        break;
    case M_RUN_HALTED:
        break;
    }
    for (int32_t i = 0; i < GUI_MSG_SIDE_COUNT; i++) {
        Anm_Step(&g_Gui.msg.portraits[i]);
    }
    for (int32_t i = 0; i < GUI_MSG_LINE_COUNT; i++) {
        Anm_Step(&g_Gui.msg.lines[i]);
    }
    for (int32_t i = 0; i < GUI_MSG_LINE_COUNT; i++) {
        Anm_Step(&g_Gui.msg.nameplates[i]);
    }
    if (g_Gui.msg.timer.current < M_BOX_OPEN_FRAMES && M_SkipHeld()) {
        Timer_Set(&g_Gui.msg.timer, M_BOX_OPEN_FRAMES);
    }
}

static CHAIN_STATUS M_Update(void *const arg)
{
    if (g_GameManager.mode.frozen) {
        return CHAIN_CONTINUE;
    }
    M_UpdateInterface();
    M_RunMsg();
    return CHAIN_CONTINUE;
}

// Places the sprites of the boss bar and the frame where the drawing
// places them, because their scripts may move or scale them from there.
static CHAIN_STATUS M_Draw(void *const arg)
{
    return GuiDraw_Frame();
}

static bool M_KeepsInterface(void)
{
    return Scene_GetRequested() == SCENE_NEXT_STAGE;
}

static RESULT M_LoadInterface(void)
{
    MUST(Anm_Load(M_FRONT_SLOT, "data/front.anm", M_FRONT_SCRIPT));
    MUST(Anm_Load(M_LOADING_SLOT, "data/loading.anm", M_LOADING_SCRIPT));
    g_Gui.loading.sprite = -1;
    const uint8_t character = g_GameManager.run.character;
    if (character < ARRAY_SIZE(m_Faces)) {
        for (int32_t i = 0; i < M_FACE_COUNT; i++) {
            MUST(Anm_Load(
                M_FACE_SLOT + i, m_Faces[character][i], m_FaceFirstSprites[i]));
        }
    }
    return OK;
}

static RESULT M_LoadStage(void)
{
    const M_STAGE_FILES *const files = M_GetStageFiles();
    for (int32_t i = 0; i < M_STAGE_FACE_COUNT && files->faces[i] != nullptr;
         i++) {
        MUST(Anm_Load(
            M_STAGE_FACE_SLOT + i, files->faces[i],
            m_StageFaceFirstSprites[i]));
    }
    return M_LoadMsg(files->msg);
}

static void M_StopBanner(ANM_VM *const vm)
{
    vm->script = nullptr;
    vm->shown = false;
}

static void M_StartBanners(void)
{
    g_Gui.boss.present = false;
    g_Gui.hud.boss_bar = GUI_BOSS_BAR_HIDDEN;
    g_Gui.boss.health = 0.0f;
    g_Gui.hud.boss_health_shown = 0.0f;
    Anm_StartScript(&g_Gui.banners.bomb_portrait, M_BOMB_PORTRAIT_SCRIPT);
    Anm_StartScript(&g_Gui.banners.spell_portrait, M_SPELL_PORTRAIT_SCRIPT);
    Anm_StartScript(&g_Gui.banners.bomb_name, M_BOMB_NAME_SCRIPT);
    Anm_StartScript(&g_Gui.banners.spell_name, M_SPELL_NAME_SCRIPT);
    Anm_StartScript(&g_Gui.banners.bomb_bar, M_BOMB_BAR_SCRIPT);
    Anm_StartScript(&g_Gui.banners.spell_bar, M_SPELL_BAR_SCRIPT);
    M_StopBanner(&g_Gui.banners.bomb_portrait);
    M_StopBanner(&g_Gui.banners.bomb_name);
    M_StopBanner(&g_Gui.banners.spell_portrait);
    M_StopBanner(&g_Gui.banners.spell_name);
    g_Gui.banners.bomb_name.font_width = M_NAME_FONT_SIZE;
    g_Gui.banners.bomb_name.font_height = M_NAME_FONT_SIZE;
    g_Gui.banners.spell_name.font_width = M_NAME_FONT_SIZE;
    g_Gui.banners.spell_name.font_height = M_NAME_FONT_SIZE;
    GuiTextImage_ShowStage(g_GameManager.run.stage);
    Anm_StartScript(&g_Gui.banners.stage_name, M_STAGE_NAME_SCRIPT);
    Anm_StartScript(&g_Gui.banners.song_name, M_SONG_NAME_SCRIPT);
    g_Gui.banners.song_name.font_width = M_SONG_FONT_SIZE;
    g_Gui.banners.song_name.font_height = M_SONG_FONT_SIZE;
    if (g_Stage.data.name != nullptr
        && !GuiTextImage_Replaces(M_STAGE_NAME_SCRIPT)) {
        Result_Log(
            Anm_RenderTextCentered(
                &g_Gui.banners.stage_name, 0xe0ffff, 0, "%s",
                Translation_GetStageName(
                    g_GameManager.run.stage, g_Stage.data.name)),
            LOG_LEVEL_WARNING);
    }
    if (g_Stage.data.song_names[0] != nullptr
        && !GuiTextImage_Replaces(M_SONG_NAME_SCRIPT)) {
        Result_Log(
            Anm_RenderTextRightAligned(
                &g_Gui.banners.song_name, 0xe0ffff, 0,
                g_Strings[STRING_SONG_NAME_FORMAT],
                Translation_GetSongName(
                    g_GameManager.run.stage, 0, g_Stage.data.song_names[0])),
            LOG_LEVEL_WARNING);
    }
    g_Gui.banners.song = 0;
    g_Gui.msg.script = -1;
    g_Gui.stage_clear.state = GUI_STAGE_CLEAR_NONE;
    g_Gui.popups.bonus.shown = false;
    g_Gui.popups.full_power.shown = false;
    g_Gui.popups.spell_bonus.shown = false;
}

static void M_Release(void)
{
    for (int32_t i = 0; i < M_STAGE_FACE_COUNT; i++) {
        Anm_Release(M_STAGE_FACE_SLOT + i);
    }
    M_FreeMsg();
    GuiTextImage_Release();
    if (!M_KeepsInterface()) {
        Anm_Release(M_FRONT_SLOT);
        Anm_Release(M_LOADING_SLOT);
        for (int32_t i = 0; i < M_FACE_COUNT; i++) {
            Anm_Release(M_FACE_SLOT + i);
        }
    }
}

static RESULT M_Load(void)
{
    if (!M_KeepsInterface()) {
        MUST(M_LoadInterface());
    } else {
        Anm_StartScript(&g_Gui.loading, M_LOADING_SCRIPT);
        g_Gui.loading.pending_interrupt = 1;
    }
    MUST(M_LoadStage());
    if (!M_KeepsInterface()) {
        for (int32_t i = 0; i < GUI_SPRITE_COUNT; i++) {
            Anm_StartScript(&g_Gui.hud.sprites[i], i + M_FRONT_SCRIPT);
        }
    }
    GuiTextImage_Load();
    M_StartBanners();
    return OK;
}

static RESULT M_Added(void *const arg)
{
    const RESULT result = M_Load();
    if (!IS_OK(result)) {
        M_Release();
    }
    return result;
}

static void M_Deleted(void *const arg)
{
    M_Release();
}

static void M_ShowPopup(GUI_POPUP *const popup, const int32_t value)
{
    popup->shown = true;
    Timer_Set(&popup->timer, 0);
    popup->value = value;
}

RESULT Gui_Register(void)
{
    if (!M_KeepsInterface()) {
        g_Gui = (GUI) {};
    }
    Chain_InitElem(&m_CalcElem, M_Update, nullptr);
    m_CalcElem.added = M_Added;
    m_CalcElem.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &m_CalcElem, M_CALC_PRIORITY));
    Chain_InitElem(&m_DrawElem, M_Draw, nullptr);
    return Chain_AddDraw(&g_Chain, &m_DrawElem, M_DRAW_PRIORITY);
}

void Gui_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawElem);
}

void Gui_StartMsg(const int32_t index)
{
    if (index < 0 || index >= m_ScriptCount) {
        return;
    }
    g_Gui.msg = (typeof(g_Gui.msg)) {};
    g_Gui.msg.script = index;
    g_Gui.msg.pc = 0;
    for (int32_t i = 0; i < GUI_MSG_LINE_COUNT; i++) {
        g_Gui.msg.lines[i].script_index = -1;
    }
    g_Gui.msg.skippable = true;
    if (index != 0 && index != 10) {
        return;
    }
    if (g_GameManager.run.stage == 6) {
        Result_Log(
            Anm_Load(M_EFFECT_SLOT, "data/eff06.anm", M_EFFECT_FIRST_SPRITE),
            LOG_LEVEL_ERROR);
    } else if (g_GameManager.run.stage == 7) {
        Result_Log(
            Anm_Load(M_EFFECT_SLOT, "data/eff07.anm", M_EFFECT_FIRST_SPRITE),
            LOG_LEVEL_ERROR);
        Result_Log(
            Anm_Load(
                M_STAGE_FACE_SLOT, "data/face12c.anm",
                m_StageFaceFirstSprites[0]),
            LOG_LEVEL_ERROR);
    }
}

bool Gui_IsMsgRunning(void)
{
    return g_Gui.msg.script >= 0;
}

bool Gui_MsgHoldsTimeline(void)
{
    if (g_Gui.msg.timeline_release > 0) {
        return false;
    }
    return g_Gui.msg.script >= 0;
}

bool Gui_IsMsgSkippable(void)
{
    return g_Gui.msg.skippable;
}

bool Gui_IsLoadingShown(void)
{
    return g_Gui.loading.sprite >= 0 && g_Gui.loading.stopped;
}

// Returns the width of the bar under a banner name: the name as a row of
// height 15 draws it, and 16 pixels more. With the bundled font, this is
// the original's 7.5 pixels for each CP932 byte. With base_tsa, the name
// takes whole pixels, rounded down, and 4 pixels more, up to the texture
// width of its sprite. A name of whole pixels draws its crop of the row
// one texel to one pixel.
static float M_BannerWidth(const ANM_VM *const vm, const char *const name)
{
    float width;
    Result_Log(
        Anm_MeasureText(name, M_BANNER_FONT_SIZE, &width), LOG_LEVEL_WARNING);
    if (Translation_HasBaseTsa() && vm->sprite_data != nullptr) {
        width = (float)(int32_t)width + 4.0f;
        if (vm->sprite_data->texture_width < width) {
            width = vm->sprite_data->texture_width;
        }
    }
    return width + 16.0f;
}

void Gui_ShowBombName(const int32_t sprite, const char *const name)
{
    Anm_StartScript(&g_Gui.banners.bomb_portrait, M_BOMB_PORTRAIT_SCRIPT);
    Anm_SetSprite(&g_Gui.banners.bomb_portrait, sprite);
    Anm_StartScript(&g_Gui.banners.bomb_name, M_BOMB_NAME_SCRIPT);
    g_Gui.banners.bomb_text = name;
    Result_Log(
        Anm_RenderText(&g_Gui.banners.bomb_name, 0xf0f0ff, 0, "%s", name),
        LOG_LEVEL_WARNING);
    g_Gui.banners.bomb_width = M_BannerWidth(&g_Gui.banners.bomb_name, name);
    Sound_Queue(SOUND_CARD);
}

void Gui_EndBombName(void)
{
    g_Gui.banners.bomb_name.pending_interrupt = 1;
}

void Gui_ShowSpellCardName(const int32_t sprite, const char *const name)
{
    Note_Remove();
    Anm_StartScript(&g_Gui.banners.spell_portrait, M_SPELL_PORTRAIT_SCRIPT);
    Anm_SetSprite(
        &g_Gui.banners.spell_portrait, sprite + M_SPELL_PORTRAIT_SPRITE);
    Anm_StartScript(&g_Gui.banners.spell_name, M_SPELL_NAME_SCRIPT);
    g_Gui.banners.spell_text = name;
    ANM_VM *const vm = &g_Gui.banners.spell_name;
    Result_Log(
        Translation_HasBaseTsa()
            ? Anm_RenderText(vm, 0xfff0f0, 0, "%s", name)
            : Anm_RenderTextRightAligned(vm, 0xfff0f0, 0, "%s", name),
        LOG_LEVEL_WARNING);
    g_Gui.banners.spell_width = M_BannerWidth(vm, name);
    Sound_Queue(SOUND_CARD);
}

void Gui_EndSpellCardName(void)
{
    g_Gui.banners.spell_name.pending_interrupt = 1;
}

void Gui_ShowSpellCardBonus(const int32_t bonus)
{
    M_ShowPopup(&g_Gui.popups.spell_bonus, bonus);
    g_Gui.popups.spell_bonus.pos = (VEC3) { 224, 16, 0 };
}

void Gui_ShowBonusPopup(const int32_t bonus)
{
    M_ShowPopup(&g_Gui.popups.bonus, bonus);
    g_Gui.popups.bonus.pos = (VEC3) { 416, 32, 0 };
}

void Gui_ShowFullPowerPopup(void)
{
    M_ShowPopup(&g_Gui.popups.full_power, 0);
    g_Gui.popups.full_power.pos = (VEC3) { 416, 232, 0 };
}
