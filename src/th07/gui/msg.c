#include "th07/gui/private.h"

#include "th07/stage.h"

#include "core/base/cp932.h"
#include "core/base/memory.h"

#include <string.h>

#define M_TEXT_OFFSET 4

static bool M_IsFinal(const uint8_t opcode)
{
    return opcode == GUI_MSG_OP_END || opcode == GUI_MSG_OP_HALT
        || opcode == GUI_MSG_OP_NEXT_SCENE;
}

static size_t M_ArgsSize(const uint8_t opcode)
{
    switch (opcode) {
    case GUI_MSG_OP_PORTRAIT_SCRIPT:
    case GUI_MSG_OP_PORTRAIT_SPRITE:
    case GUI_MSG_OP_WAIT:
    case GUI_MSG_OP_SONG:
        return 4;
    case GUI_MSG_OP_LINE:
    case GUI_MSG_OP_NAMEPLATE:
        return M_TEXT_OFFSET + 1;
    case GUI_MSG_OP_INTERRUPT:
        return 3;
    case GUI_MSG_OP_SKIPPABLE:
        return 1;
    default:
        return 0;
    }
}

static RESULT M_CheckInstr(const MSG_INSTR *const instr)
{
    switch (instr->opcode) {
    case GUI_MSG_OP_PORTRAIT_SCRIPT:
    case GUI_MSG_OP_PORTRAIT_SPRITE:
        FAIL_IF(
            instr->who < 0 || instr->who >= GUI_MSG_SIDE_COUNT,
            "a portrait instruction names side %d", instr->who);
        return OK;
    case GUI_MSG_OP_LINE:
    case GUI_MSG_OP_NAMEPLATE:
        FAIL_IF(
            instr->who < 0 || instr->who >= GUI_MSG_SIDE_COUNT,
            "a text instruction names side %d", instr->who);
        FAIL_IF(
            instr->index < 0 || instr->index >= GUI_MSG_LINE_COUNT,
            "a text instruction names line %d", instr->index);
        return OK;
    case GUI_MSG_OP_INTERRUPT:
        FAIL_IF(
            instr->who < 0
                || instr->who >= GUI_MSG_SIDE_COUNT + GUI_MSG_LINE_COUNT,
            "an interrupt names sprite %d", instr->who);
        return OK;
    case GUI_MSG_OP_SONG:
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
static RESULT M_ReadInstr(READER *const reader, MSG_INSTR *const instr)
{
    const size_t start = Reader_Tell(reader);
    instr->time = Reader_ReadU16(reader);
    instr->opcode = Reader_ReadU8(reader);
    const size_t size = Reader_ReadU8(reader);
    FAIL_IF(
        Reader_Failed(reader) || size > Reader_Remaining(reader),
        "the instruction at 0x%zx runs past the file", start);
    FAIL_IF(
        instr->opcode > GUI_MSG_OP_FADE_OUT_SCREEN,
        "the instruction at 0x%zx has opcode %d", start, instr->opcode);
    FAIL_IF(
        size < M_ArgsSize(instr->opcode),
        "the instruction at 0x%zx has %zu bytes of arguments", start, size);
    uint8_t args[UINT8_MAX] = {};
    Reader_ReadBytes(reader, args, size);
    READER fields;
    Reader_OpenBuffer(&fields, args, size, nullptr);
    switch (instr->opcode) {
    case GUI_MSG_OP_WAIT:
    case GUI_MSG_OP_SONG:
        instr->value = Reader_ReadI32(&fields);
        break;
    case GUI_MSG_OP_SKIPPABLE:
        instr->value = Reader_ReadU8(&fields);
        break;
    case GUI_MSG_OP_INTERRUPT:
        instr->who = Reader_ReadI16(&fields);
        instr->index = Reader_ReadU8(&fields);
        break;
    case GUI_MSG_OP_LINE:
    case GUI_MSG_OP_NAMEPLATE: {
        instr->who = Reader_ReadI16(&fields);
        instr->index = Reader_ReadI16(&fields);
        const uint8_t *const text = &args[M_TEXT_OFFSET];
        const uint8_t *const end = memchr(text, '\0', size - M_TEXT_OFFSET);
        FAIL_IF(
            end == nullptr, "the text at 0x%zx runs past its instruction",
            start);
        MUST(
            Cp932_Decode(
                (const char *)text, (size_t)(end - text), &instr->text),
            "the text at 0x%zx", start);
        break;
    }
    case GUI_MSG_OP_PORTRAIT_SCRIPT:
    case GUI_MSG_OP_PORTRAIT_SPRITE:
        instr->who = Reader_ReadI16(&fields);
        instr->index = Reader_ReadI16(&fields);
        break;
    default:
        break;
    }
    return M_CheckInstr(instr);
}

// Reads the script at offset up to the first instruction it stops on.
static RESULT M_ReadScript(
    READER *const reader, const size_t offset, MSG_SCRIPT *const script)
{
    Reader_Seek(reader, offset);
    int32_t count = 0;
    MSG_INSTR instr;
    do {
        memset(&instr, 0, sizeof(instr));
        const RESULT result = M_ReadInstr(reader, &instr);
        Mem_Free(instr.text);
        MUST(result, "instruction %d", count);
        count++;
    } while (!M_IsFinal(instr.opcode));

    script->instrs = Mem_AllocArrZ((size_t)count, sizeof(MSG_INSTR));
    script->count = count;
    Reader_Seek(reader, offset);
    for (int32_t i = 0; i < count; i++) {
        MUST(M_ReadInstr(reader, &script->instrs[i]));
    }
    return OK;
}

static RESULT M_Read(READER *const reader, GUI_MSG_FILE *const out)
{
    const int32_t count = Reader_ReadI32(reader);
    FAIL_IF(
        Reader_Failed(reader) || count < 0
            || (size_t)count > Reader_Remaining(reader) / 4,
        "bad header");
    out->scripts = Mem_AllocArrZ((size_t)count, sizeof(MSG_SCRIPT));
    out->count = count;
    for (int32_t i = 0; i < count; i++) {
        Reader_Seek(reader, 4 + 4 * (size_t)i);
        const int32_t offset = Reader_ReadI32(reader);
        FAIL_IF(offset < 0, "script %d starts at %d", i, offset);
        MUST(
            M_ReadScript(reader, (size_t)offset, &out->scripts[i]), "script %d",
            i);
    }
    return OK;
}

RESULT GuiMsg_Read(READER *const reader, GUI_MSG_FILE *const out)
{
    memset(out, 0, sizeof(*out));
    const RESULT result = M_Read(reader, out);
    if (!IS_OK(result)) {
        GuiMsg_Free(out);
    }
    return result;
}

void GuiMsg_Free(GUI_MSG_FILE *const file)
{
    for (int32_t i = 0; i < file->count; i++) {
        for (int32_t j = 0; j < file->scripts[i].count; j++) {
            Mem_Free(file->scripts[i].instrs[j].text);
        }
        Mem_Free(file->scripts[i].instrs);
    }
    Mem_Free(file->scripts);
    memset(file, 0, sizeof(*file));
}
