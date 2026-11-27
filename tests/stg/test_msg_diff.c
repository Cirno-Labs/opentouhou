#include "tests/test.h"

#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/json.h"
#include "stg/msg_diff.h"

#include <string.h>

// A synthetic dialogue format: opcode 0 ends a script, 3 is a line and 8
// a nameplate, and a box holds at most two lines.
enum {
    M_OP_END = 0,
    M_OP_LINE = 3,
    M_OP_NAMEPLATE = 8,
};

static bool M_IsFinal(const uint8_t opcode)
{
    return opcode == M_OP_END;
}

static const MSG_DIFF_FORMAT m_Format = {
    .line = M_OP_LINE,
    .nameplate = M_OP_NAMEPLATE,
    .line_count = 2,
    .is_final = M_IsFinal,
};

static MSG_SCRIPT m_Script;

static void M_AddInstr(
    const int32_t time, const uint8_t opcode, const int16_t index,
    const char *const text)
{
    m_Script.instrs = Mem_ReallocArr(
        m_Script.instrs, (size_t)(m_Script.count + 1), sizeof(MSG_INSTR));
    m_Script.instrs[m_Script.count] = (MSG_INSTR) {
        .time = time,
        .opcode = opcode,
        .index = index,
        .text = text != nullptr ? Str_Dup(text) : nullptr,
    };
    m_Script.count++;
}

static void M_Line(const int32_t time, const int16_t index, const char *text)
{
    M_AddInstr(time, M_OP_LINE, index, text);
}

static void M_Nameplate(
    const int32_t time, const int16_t index, const char *const text)
{
    M_AddInstr(time, M_OP_NAMEPLATE, index, text);
}

static void M_End(const int32_t time)
{
    M_AddInstr(time, M_OP_END, 0, nullptr);
}

static void M_Teardown(void)
{
    for (int32_t i = 0; i < m_Script.count; i++) {
        Mem_Free(m_Script.instrs[i].text);
    }
    Mem_Free(m_Script.instrs);
    memset(&m_Script, 0, sizeof(m_Script));
}

static void M_Apply(const char *const diff)
{
    ARENA arena;
    Arena_Init(&arena, "test", 1 << 16);
    JSON_VALUE *tree = nullptr;
    CHECK(Result_Log(
        Json_Parse(diff, strlen(diff), Arena_Allocator(&arena), &tree),
        LOG_LEVEL_ERROR));
    MsgDiff_Apply(&m_Script, 1, tree, m_Format);
    Arena_Free(&arena);
}

static MSG_INSTR *M_FindLine(const int32_t time, const int16_t index)
{
    for (int32_t i = 0; i < m_Script.count; i++) {
        MSG_INSTR *const instr = &m_Script.instrs[i];
        if (instr->time == time && instr->opcode == M_OP_LINE
            && instr->index == index) {
            return instr;
        }
    }
    return nullptr;
}

TEST(a_jdiff_replaces_the_lines_of_a_box)
{
    M_Line(10, 0, "a");
    M_Line(10, 1, "b");
    M_End(10);
    M_Apply("{'0': {'10_0': {lines: ['One', 'Two']}}}");
    CHECK_EQ_STR(M_FindLine(10, 0)->text, "One");
    CHECK_EQ_STR(M_FindLine(10, 1)->text, "Two");
    M_Teardown();
}

TEST(a_jdiff_box_can_be_a_bare_array)
{
    M_Line(10, 0, "a");
    M_End(10);
    M_Apply("{'0': {'10_0': ['One']}}");
    CHECK_EQ_STR(M_FindLine(10, 0)->text, "One");
    M_Teardown();
}

TEST(fewer_jdiff_lines_delete_the_extra_lines)
{
    M_Line(10, 0, "a");
    M_Line(10, 1, "b");
    M_End(10);
    M_Apply("{'0': {'10_0': ['One']}}");
    CHECK_EQ_STR(M_FindLine(10, 0)->text, "One");
    CHECK(M_FindLine(10, 1) == nullptr);
    M_Teardown();
}

TEST(more_jdiff_lines_add_lines_after_the_box)
{
    M_Line(10, 0, "a");
    M_End(10);
    M_Apply("{'0': {'10_0': ['One', 'Two']}}");
    CHECK_EQ_STR(M_FindLine(10, 0)->text, "One");
    CHECK_EQ_STR(M_FindLine(10, 1)->text, "Two");
    M_Teardown();
}

TEST(jdiff_lines_past_the_format_line_count_are_dropped)
{
    M_Line(10, 0, "a");
    M_End(10);
    M_Apply("{'0': {'10_0': ['One', 'Two', 'Three']}}");
    CHECK_EQ_STR(M_FindLine(10, 0)->text, "One");
    CHECK_EQ_STR(M_FindLine(10, 1)->text, "Two");
    CHECK(M_FindLine(10, 2) == nullptr);
    M_Teardown();
}

TEST(a_jdiff_nameplate_takes_its_h1_key)
{
    M_Nameplate(10, 0, "a");
    M_Line(10, 0, "a");
    M_End(10);
    M_Apply("{'0': {'10_h1_0': ['Rumia'], '10_0': ['Line']}}");
    for (int32_t i = 0; i < m_Script.count; i++) {
        if (m_Script.instrs[i].opcode == M_OP_NAMEPLATE) {
            CHECK_EQ_STR(m_Script.instrs[i].text, "Rumia");
        }
    }
    CHECK_EQ_STR(M_FindLine(10, 0)->text, "Line");
    M_Teardown();
}

TEST(jdiff_boxes_count_within_a_time_code)
{
    M_Line(10, 0, "a");
    M_Nameplate(10, 0, "x"); // keeps the box counter advancing
    M_Line(10, 0, "a");
    M_End(10);
    M_Apply("{'0': {'10_0': ['A'], '10_1': ['B']}}");
    CHECK_EQ_STR(m_Script.instrs[0].text, "A");
    CHECK_EQ_STR(m_Script.instrs[2].text, "B");
    M_Teardown();
}

TEST(a_new_time_code_counts_its_boxes_from_0)
{
    M_Line(10, 0, "a");
    M_Line(11, 0, "a");
    M_End(11);
    M_Apply("{'0': {'10_0': ['A'], '11_0': ['B']}}");
    CHECK_EQ_STR(M_FindLine(10, 0)->text, "A");
    CHECK_EQ_STR(M_FindLine(11, 0)->text, "B");
    M_Teardown();
}

TEST(jdiff_lines_keep_their_markup)
{
    M_Line(10, 0, "a");
    M_End(10);
    M_Apply("{'0': {'10_0': ['<c$Rumia$x> says \"hi\"']}}");
    CHECK_EQ_STR(M_FindLine(10, 0)->text, "<c$Rumia$x> says \"hi\"");
    M_Teardown();
}

TEST(the_first_jdiff_line_of_a_note_hides_it)
{
    M_Line(10, 0, "a");
    M_End(10);
    M_Apply("{'0': {'10_0': ['One\\u0014A note']}}");
    CHECK(!M_FindLine(10, 0)->removes_note);
    M_Teardown();
}

TEST(a_line_after_a_note_box_hides_it)
{
    M_Line(10, 0, "a");
    M_Line(10, 1, "b");
    M_Line(20, 0, "a");
    M_End(20);
    M_Apply("{'0': {'10_0': ['One', 'Two\\u0014A note'], '20_0': ['Three']}}");
    CHECK(!M_FindLine(10, 1)->removes_note);
    CHECK(M_FindLine(20, 0)->removes_note);
    M_Teardown();
}

TEST(a_jdiff_for_another_script_changes_nothing)
{
    M_Line(10, 0, "a");
    M_End(10);
    M_Apply("{'1': {'10_0': ['One']}}");
    CHECK_EQ_STR(M_FindLine(10, 0)->text, "a");
    M_Teardown();
}
