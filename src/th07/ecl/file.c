#include "th07/ecl/file.h"

#include "th07/ecl/private.h"

#include "th07/game_manager.h"

#include "shell/game.h"
#include "core/vfs.h"
#include "core/base/assert.h"
#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/base/utils.h"
#include "core/io/reader.h"

#include <string.h>

#define M_SUB_TABLE 0x44
#define M_INSTR_HEADER_SIZE 12
#define M_END_TIME -1
#define M_TIMELINE_HEADER_SIZE 8
#define M_TIMELINE_ARGS_SIZE 24
#define M_NAME_SIZE 48
#define M_NAME_KEY 0xaa

static const char *const m_Paths[9] = {
    "dummy",
    "data/ecldata1.ecl",
    "data/ecldata2.ecl",
    "data/ecldata3.ecl",
    "data/ecldata4.ecl",
    "data/ecldata5.ecl",
    "data/ecldata6.ecl",
    "data/ecldata7.ecl",
    "data/ecldata8.ecl",
};

// Holds the number of argument words that each opcode reads.
static const uint8_t m_WordsRead[162] = {
    0, 0, 2, 3, 2, 2, 2, 3, 2, 3, 2,  2,  3, 3, 3, 3, 3, 1, 1,  3, 3, 3, 3, 3,
    2, 2, 5, 8, 4, 4, 4, 4, 4, 4, 4,  4,  4, 4, 4, 4, 1, 1, 0,  3, 3, 1, 3, 3,
    1, 1, 1, 3, 1, 2, 4, 5, 8, 2, 2,  1,  1, 1, 4, 0, 8, 8, 8,  8, 8, 8, 8, 8,
    8, 1, 1, 0, 0, 0, 3, 7, 0, 2, 13, 13, 1, 2, 2, 4, 1, 1, 13, 0, 7, 7, 0, 1,
    3, 2, 1, 1, 5, 3, 1, 1, 1, 1, 1,  1,  2, 1, 1, 1, 1, 1, 1,  1, 1, 3, 6, 1,
    1, 2, 2, 1, 1, 1, 1, 0, 1, 2, 1,  6,  1, 0, 0, 1, 1, 1, 4,  4, 4, 0, 1, 1,
    2, 2, 0, 1, 3, 4, 1, 4, 2, 3, 1,  1,  2, 2, 3, 4, 1, 1,
};

static ECL_SUB *m_Subs;
static int32_t m_SubCount;
static ECL_TIMELINE_ENTRY *m_Timelines[ECL_TIMELINE_COUNT];
static int32_t m_TimelineCount;

// Returns the number of argument words that opcode reads.
static int32_t M_WordsRead(const int16_t opcode)
{
    if (opcode < 0 || opcode >= (int16_t)ARRAY_SIZE(m_WordsRead)) {
        return 0;
    }
    return m_WordsRead[opcode];
}

static bool M_IsJump(const int16_t opcode)
{
    return opcode == ECL_OP_JUMP || opcode == ECL_OP_LOOP
        || (opcode >= ECL_OP_JUMP_EQUAL_INT
            && opcode <= ECL_OP_JUMP_GREATER_EQUAL_FLOAT);
}

// Returns the word that holds the byte offset of a jump.
static int32_t M_JumpWord(const int16_t opcode)
{
    return opcode <= ECL_OP_LOOP ? 1 : 3;
}

static void M_FreeSubs(void)
{
    for (int32_t i = 0; i < m_SubCount; i++) {
        for (int32_t j = 0; j < m_Subs[i].count; j++) {
            Mem_Free(m_Subs[i].instrs[j].text);
        }
        Mem_Free(m_Subs[i].instrs);
    }
    Mem_Free(m_Subs);
    m_Subs = nullptr;
    m_SubCount = 0;
}

static RESULT M_ReadName(
    READER *const reader, const size_t start, ECL_INSTR *const instr)
{
    char bytes[M_NAME_SIZE];
    Reader_Seek(reader, start + M_INSTR_HEADER_SIZE + 4);
    Reader_ReadBytes(reader, bytes, sizeof(bytes));
    for (size_t i = 0; i < sizeof(bytes); i++) {
        bytes[i] = (char)(bytes[i] ^ M_NAME_KEY);
    }
    const char *const end = memchr(bytes, '\0', sizeof(bytes));
    FAIL_IF(end == nullptr, "the spell card at 0x%zx has no name", start);
    const uint16_t index = (uint16_t)((uint32_t)instr->args[0].i >> 16);
    FAIL_IF(
        index >= ECL_SPELL_CARD_COUNT, "the spell card at 0x%zx is number %u",
        start, index);
    MUST(
        Cp932_Decode(bytes, (size_t)(end - bytes), &instr->text),
        "the spell card at 0x%zx", start);
    return OK;
}

// Reads the instruction at the position into instr, and moves to the next
// one.
static RESULT M_ReadInstr(READER *const reader, ECL_INSTR *const instr)
{
    const size_t start = Reader_Tell(reader);
    instr->time = Reader_ReadI32(reader);
    FAIL_IF(
        Reader_Failed(reader), "the instruction at 0x%zx runs past the file",
        start);
    if (instr->time == M_END_TIME) {
        instr->opcode = -1;
        return OK;
    }
    instr->opcode = Reader_ReadI16(reader);
    const int16_t size = Reader_ReadI16(reader);
    Reader_Skip(reader, 1);
    instr->difficulty_mask = Reader_ReadU8(reader);
    instr->param_mask = Reader_ReadU16(reader);
    FAIL_IF(
        Reader_Failed(reader) || size < M_INSTR_HEADER_SIZE
            || (size_t)size > Reader_Size(reader) - start,
        "the instruction at 0x%zx runs past the file", start);
    const int32_t words = (size - M_INSTR_HEADER_SIZE) / 4;
    FAIL_IF(
        words < M_WordsRead(instr->opcode),
        "the instruction at 0x%zx is too short for opcode %d", start,
        instr->opcode);
    for (int32_t i = 0; i < MIN(words, (int32_t)ECL_ARG_COUNT); i++) {
        instr->args[i].i = Reader_ReadI32(reader);
    }
    if (instr->opcode == ECL_OP_START_SPELL_CARD) {
        MUST(M_ReadName(reader, start, instr));
    }
    Reader_Seek(reader, start + (size_t)size);
    return OK;
}

// Parses the subroutine at offset up to its end, and turns the byte
// offset of each jump into an instruction index.
static RESULT M_ParseSub(
    READER *const reader, const size_t offset, ECL_SUB *const sub)
{
    FAIL_IF(offset > Reader_Size(reader), "the subroutine is past the file");
    Reader_Seek(reader, offset);
    int32_t count = 0;
    ECL_INSTR instr;
    do {
        memset(&instr, 0, sizeof(instr));
        const RESULT result = M_ReadInstr(reader, &instr);
        Mem_Free(instr.text);
        MUST(result);
        count++;
    } while (instr.time != M_END_TIME);

    sub->instrs = Mem_AllocArrZ((size_t)count, sizeof(ECL_INSTR));
    sub->count = count;
    int64_t *const offsets = Mem_AllocArr((size_t)count, sizeof(int64_t));
    Reader_Seek(reader, offset);
    RESULT result = OK;
    for (int32_t i = 0; i < count && IS_OK(result); i++) {
        offsets[i] = (int64_t)(Reader_Tell(reader) - offset);
        result = M_ReadInstr(reader, &sub->instrs[i]);
    }
    for (int32_t i = 0; i < count && IS_OK(result); i++) {
        ECL_INSTR *const jump = &sub->instrs[i];
        if (!M_IsJump(jump->opcode)) {
            continue;
        }
        const int64_t target =
            offsets[i] + jump->args[M_JumpWord(jump->opcode)].i;
        jump->target = 0;
        while (jump->target < count && offsets[jump->target] != target) {
            jump->target++;
        }
        if (jump->target == count) {
            result = FAIL(
                "instruction %d jumps to %lld, which is not an instruction", i,
                (long long)target);
        }
    }
    Mem_Free(offsets);
    return result;
}

// Returns whether word index of instr is a constant that names no
// subroutine. A negative constant passes when none may be negative.
static bool M_NamesNoSub(
    const ECL_INSTR *const instr, const int32_t index, const bool none)
{
    if ((instr->param_mask & (1u << index)) != 0) {
        return false;
    }
    const int32_t value = instr->args[index].i;
    return !(none && value < 0) && !Ecl_IsSub(value);
}

static bool M_IsConstant(const ECL_INSTR *const instr, const int32_t index)
{
    return (instr->param_mask & (1u << index)) == 0;
}

static RESULT M_CheckInstr(const ECL_INSTR *const instr)
{
    const int32_t arg0 = instr->args[0].i;
    switch (instr->opcode) {
    case ECL_OP_CALL:
    case ECL_OP_SPAWN:
    case ECL_OP_SPAWN_RELATIVE:
        FAIL_IF(!Ecl_IsSub(arg0), "it names subroutine %d", arg0);
        break;
    case ECL_OP_SET_INTERRUPT:
    case ECL_OP_SET_LIFE_CALLBACK_SUB:
    case ECL_OP_SET_TIME_CALLBACK_SUB:
        FAIL_IF(M_NamesNoSub(instr, 0, true), "it names subroutine %d", arg0);
        break;
    case ECL_OP_SET_PERIODIC_CALL:
        FAIL_IF(
            M_NamesNoSub(instr, 1, true), "it names subroutine %d",
            instr->args[1].i);
        break;
    case ECL_OP_SET_LIFE_CALLBACK_AT:
        FAIL_IF(
            M_NamesNoSub(instr, 2, true), "it names subroutine %d",
            instr->args[2].i);
        break;
    case ECL_OP_CALL_FUNC:
    case ECL_OP_SET_FRAME_FUNC:
        // A negative function of opcode 122 clears the frame function.
        FAIL_IF(
            M_IsConstant(instr, 0)
                && (arg0 >= ECL_FUNC_COUNT
                    || (arg0 < 0 && instr->opcode == ECL_OP_CALL_FUNC)),
            "it names pattern function %d", arg0);
        break;
    case ECL_OP_INTERPOLATE:
        FAIL_IF(
            M_IsConstant(instr, 2)
                && (instr->args[2].i < 0 || instr->args[2].i > 7),
            "it names curve %d", instr->args[2].i);
        break;
    default:
        break;
    }
    return OK;
}

static RESULT M_CheckSubs(void)
{
    for (int32_t i = 0; i < m_SubCount; i++) {
        for (int32_t j = 0; j < m_Subs[i].count; j++) {
            MUST(
                M_CheckInstr(&m_Subs[i].instrs[j]),
                "instruction %d of subroutine %d", j, i);
        }
    }
    return OK;
}

// Reads the timeline entry at the position into entry, and moves to the
// next one. An entry holds only the argument bytes that its size gives it;
// the others are zero. The terminator needs only its time.
static RESULT M_ReadEntry(READER *const reader, ECL_TIMELINE_ENTRY *const entry)
{
    const size_t start = Reader_Tell(reader);
    entry->time = Reader_ReadI16(reader);
    FAIL_IF(Reader_Failed(reader), "the timeline runs past the file");
    if (entry->time < 0) {
        return OK;
    }
    entry->arg = Reader_ReadI16(reader);
    entry->opcode = Reader_ReadI16(reader);
    const int16_t size = Reader_ReadI16(reader);
    FAIL_IF(
        Reader_Failed(reader) || size < M_TIMELINE_HEADER_SIZE
            || (size_t)size > Reader_Size(reader) - start,
        "the timeline entry at 0x%zx runs past the file", start);
    uint8_t bytes[M_TIMELINE_ARGS_SIZE] = {};
    Reader_ReadBytes(
        reader, bytes,
        MIN((size_t)size - M_TIMELINE_HEADER_SIZE, sizeof(bytes)));
    READER args;
    Reader_OpenBuffer(&args, bytes, sizeof(bytes), nullptr);
    entry->args.spawn.pos.x = Reader_ReadF32(&args);
    entry->args.spawn.pos.y = Reader_ReadF32(&args);
    entry->args.spawn.pos.z = Reader_ReadF32(&args);
    entry->args.spawn.life = Reader_ReadI32(&args);
    entry->args.spawn.item = Reader_ReadI32(&args);
    entry->args.spawn.score = Reader_ReadI32(&args);
    if (entry->opcode == ECL_TIMELINE_INTERRUPT_BOSS) {
        Reader_Seek(&args, 0);
        entry->args.interrupt.boss = Reader_ReadI32(&args);
        entry->args.interrupt.interrupt = Reader_ReadI32(&args);
    }
    FAIL_IF(
        entry->opcode >= ECL_TIMELINE_SPAWN
            && entry->opcode <= ECL_TIMELINE_SPAWN_RANDOM_MIRRORED_DEFAULT
            && !Ecl_IsSub(entry->arg),
        "the timeline entry at 0x%zx spawns subroutine %d", start, entry->arg);
    Reader_Seek(reader, start + (size_t)size);
    return OK;
}

static RESULT M_ParseTimeline(
    READER *const reader, const size_t offset,
    ECL_TIMELINE_ENTRY **const timeline)
{
    FAIL_IF(offset > Reader_Size(reader), "the timeline is past the file");
    Reader_Seek(reader, offset);
    int32_t count = 0;
    ECL_TIMELINE_ENTRY entry;
    do {
        memset(&entry, 0, sizeof(entry));
        MUST(M_ReadEntry(reader, &entry));
        count++;
    } while (entry.time >= 0);
    *timeline = Mem_AllocArrZ((size_t)count, sizeof(ECL_TIMELINE_ENTRY));
    Reader_Seek(reader, offset);
    for (int32_t i = 0; i < count; i++) {
        MUST(M_ReadEntry(reader, &(*timeline)[i]));
    }
    return OK;
}

static RESULT M_Parse(READER *const reader)
{
    const int16_t sub_count = Reader_ReadI16(reader);
    const int16_t timeline_count = Reader_ReadI16(reader);
    int32_t timelines[ECL_TIMELINE_COUNT];
    for (int32_t i = 0; i < ECL_TIMELINE_COUNT; i++) {
        timelines[i] = Reader_ReadI32(reader);
    }
    FAIL_IF(Reader_Failed(reader), "the header runs past the file");
    FAIL_IF(
        sub_count < 0 || timeline_count < 0
            || timeline_count > ECL_TIMELINE_COUNT,
        "the header gives %d subroutines and %d timelines", sub_count,
        timeline_count);
    m_Subs = Mem_AllocArrZ((size_t)sub_count, sizeof(ECL_SUB));
    m_SubCount = sub_count;
    for (int32_t i = 0; i < sub_count; i++) {
        Reader_Seek(reader, M_SUB_TABLE + 4 * (size_t)i);
        const int32_t offset = Reader_ReadI32(reader);
        FAIL_IF(
            Reader_Failed(reader) || offset < 0,
            "the subroutine table runs past the file");
        MUST(
            M_ParseSub(reader, (size_t)offset, &m_Subs[i]), "subroutine %d", i);
    }
    MUST(M_CheckSubs());
    m_TimelineCount = timeline_count;
    for (int32_t i = 0; i < timeline_count; i++) {
        FAIL_IF(timelines[i] < 0, "timeline %d is past the file", i);
        MUST(
            M_ParseTimeline(reader, (size_t)timelines[i], &m_Timelines[i]),
            "timeline %d", i);
    }
    return OK;
}

ECL_INSTR *Ecl_GetInstr(const int16_t sub, const int32_t pc)
{
    ASSERT(Ecl_IsSub(sub));
    const ECL_SUB *const s = &m_Subs[sub];
    ASSERT(pc >= 0 && pc < s->count);
    return &s->instrs[pc];
}

bool Ecl_IsSub(const int32_t sub)
{
    return sub >= 0 && sub < m_SubCount;
}

RESULT Ecl_Load(void)
{
    Ecl_Release();
    const int32_t stage = g_GameManager.run.stage;
    ASSERT(stage >= 0 && stage < (int32_t)ARRAY_SIZE(m_Paths));
    const char *const path = m_Paths[stage];
    READER reader;
    MUST(Vfs_Open(Shell_GetFiles(), path, VFS_IN_MEMORY, &reader), "%s", path);
    const RESULT result = M_Parse(&reader);
    Reader_Close(&reader);
    if (!IS_OK(result)) {
        Ecl_Release();
    }
    return Result_Prefix(result, "%s", path);
}

void Ecl_Release(void)
{
    M_FreeSubs();
    for (int32_t i = 0; i < ECL_TIMELINE_COUNT; i++) {
        Mem_Free(m_Timelines[i]);
        m_Timelines[i] = nullptr;
    }
    m_TimelineCount = 0;
}

int32_t Ecl_GetTimelineCount(void)
{
    return m_TimelineCount;
}

const ECL_TIMELINE_ENTRY *Ecl_GetTimeline(const int32_t index)
{
    ASSERT(index >= 0 && index < m_TimelineCount);
    return m_Timelines[index];
}
