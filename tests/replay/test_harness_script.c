#include "replay/harness/script.h"
#include "tests/harness_game.h"
#include "tests/test.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

#include <string.h>

static RESULT M_Read(const char *const text, SCRIPT *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, text, strlen(text), "inputs.txt");
    const RESULT result = Script_Read(&reader, &g_TestGame, out);
    Reader_Close(&reader);
    return result;
}

static char *M_Write(const SCRIPT *const script)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Script_Write(&writer, &g_TestGame, script);
    Writer_WriteU8(&writer, 0);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    char *const text = Str_Dup((const char *)bytes);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    return text;
}

TEST(a_script_holds_keys_and_rates_from_their_frames)
{
    SCRIPT script;
    CHECK(Result_Ignore(M_Read(
        "# start\nframes 100\ninput 3 shoot+up\n\ninput 10 none\n"
        "fps 30 59.5\nfps 60 none\n",
        &script)));
    CHECK_EQ_INT(script.frames, 100);
    CHECK_EQ_INT(Script_Buttons(&script, 2), 0);
    CHECK_EQ_INT(Script_Buttons(&script, 3), 0x11);
    CHECK_EQ_INT(Script_Buttons(&script, 9), 0x11);
    CHECK_EQ_INT(Script_Buttons(&script, 10), 0);
    CHECK(!Script_Fps(&script, 29).available);
    CHECK(Script_Fps(&script, 59).available);
    CHECK(Script_Fps(&script, 59).rate == 59.5f);
    CHECK(!Script_Fps(&script, 60).available);
    Script_Free(&script);
}

TEST(a_script_holds_start_values_and_writes_none)
{
    SCRIPT script;
    CHECK(Result_Ignore(
        M_Read("start lives -3\ninput 3 shoot\nstart stage 9\n", &script)));
    CHECK_EQ_INT(script.starts.count, 2);
    CHECK_EQ_INT(script.starts.items[0].field, 1);
    CHECK_EQ_INT((int32_t)script.starts.items[0].value, -3);
    CHECK_EQ_INT(script.starts.items[1].field, 0);
    CHECK_EQ_INT(script.starts.items[1].value, 9);
    char *const text = M_Write(&script);
    CHECK_EQ_STR(text, "input 3 shoot\n");
    Mem_Free(text);
    Script_Free(&script);
}

TEST(a_script_rejects_bad_lines)
{
    const char *const texts[] = {
        "input 3 shot\n",
        "input 3 shoot\ninput 3 bomb\n",
        "input x shoot\n",
        "fps 1 fast\n",
        "frames\n",
        "wait 3 shoot\n",
        "input 3 shoot bomb\n",
        "start level 3\n",
        "start stage -1\n",
        "start lives x\n",
        "start stage 1\nstart stage 2\n",
        "start stage\n",
    };
    for (size_t i = 0; i < ARRAY_SIZE(texts); i++) {
        SCRIPT script = { .frames = 7 };
        CHECK(!Result_Ignore(M_Read(texts[i], &script)));
        CHECK_EQ_INT(script.frames, 7);
    }
}

TEST(a_recorded_script_writes_only_changes)
{
    SCRIPT script = {};
    Array_Init(&script.inputs, &g_HeapAllocator);
    Array_Init(&script.fps, &g_HeapAllocator);
    Script_RecordButtons(&script, 0, 0);
    Script_RecordButtons(&script, 1, 0x2);
    Script_RecordButtons(&script, 2, 0x2);
    Script_RecordButtons(&script, 4, 0);
    Script_RecordFps(&script, 0, (FPS) {});
    Script_RecordFps(&script, 30, (FPS) { true, 60.0f });
    Script_RecordFps(&script, 60, (FPS) { true, 60.0f });
    script.frames = 90;
    char *const text = M_Write(&script);
    CHECK_EQ_STR(text, "frames 90\ninput 1 bomb\ninput 4 none\nfps 30 60\n");
    SCRIPT again;
    CHECK(Result_Ignore(M_Read(text, &again)));
    CHECK_EQ_INT(again.inputs.count, 2);
    CHECK_EQ_INT(Script_Buttons(&again, 3), 0x2);
    Mem_Free(text);
    Script_Free(&again);
    Script_Free(&script);
}

TEST(a_description_is_read_and_written_back_verbatim)
{
    SCRIPT script;
    CHECK(Result_Ignore(
        M_Read("# line one\n#\n# line two\ninput 3 shoot\n", &script)));
    CHECK_EQ_STR(script.description, "# line one\n#\n# line two\n");
    char *const text = M_Write(&script);
    CHECK_EQ_STR(text, "# line one\n#\n# line two\ninput 3 shoot\n");
    Mem_Free(text);
    Script_Free(&script);
}

TEST(a_script_without_a_description_writes_nothing_extra)
{
    SCRIPT script;
    CHECK(Result_Ignore(M_Read("input 3 shoot\n", &script)));
    CHECK(script.description == nullptr);
    char *const text = M_Write(&script);
    CHECK_EQ_STR(text, "input 3 shoot\n");
    Mem_Free(text);
    Script_Free(&script);
}

TEST(a_comment_after_the_first_line_is_not_a_description)
{
    SCRIPT script;
    CHECK(Result_Ignore(
        M_Read("# header\ninput 3 shoot\n# not a header\n", &script)));
    CHECK_EQ_STR(script.description, "# header\n");
    Script_Free(&script);
}

TEST(a_record_twice_in_one_frame_keeps_the_last)
{
    SCRIPT script = {};
    Array_Init(&script.inputs, &g_HeapAllocator);
    Array_Init(&script.fps, &g_HeapAllocator);
    Script_RecordButtons(&script, 5, 0x1);
    Script_RecordButtons(&script, 5, 0x2);
    CHECK_EQ_INT(script.inputs.count, 1);
    CHECK_EQ_INT(Script_Buttons(&script, 5), 0x2);
    Script_RecordButtons(&script, 6, 0x2);
    Script_RecordButtons(&script, 6, 0);
    CHECK_EQ_INT(script.inputs.count, 2);
    CHECK_EQ_INT(Script_Buttons(&script, 6), 0);
    Script_Free(&script);
}

TEST(a_script_holds_dates_and_clocks_from_their_frames)
{
    SCRIPT script;
    CHECK(Result_Ignore(M_Read(
        "# start\nframes 100\ndate 0 2026-11-27\ndate 10 none\n"
        "clock 0 1 1500\nclock 0 2 2500\nclock 5 1 9999999999\n",
        &script)));
    CHECK_EQ_INT(Script_Date(&script, 0), 20261127);
    CHECK_EQ_INT(Script_Date(&script, 9), 20261127);
    CHECK_EQ_INT(Script_Date(&script, 10), 0);
    uint64_t microseconds;
    CHECK(Script_Clock(&script, 0, 1, &microseconds));
    CHECK_EQ_INT(microseconds, 1500);
    CHECK(Script_Clock(&script, 0, 2, &microseconds));
    CHECK_EQ_INT(microseconds, 2500);
    CHECK(!Script_Clock(&script, 1, 1, &microseconds));
    size_t count;
    const SCRIPT_CLOCK *const clocks = Script_Clocks(&script, 0, &count);
    CHECK_EQ_INT(count, 2);
    CHECK_EQ_INT(clocks[0].key, 1);
    CHECK_EQ_INT(clocks[1].key, 2);
    CHECK(Script_Clocks(&script, 1, &count) == nullptr);
    CHECK_EQ_INT(count, 0);
    Script_Free(&script);
}

TEST(a_script_rejects_bad_dates_and_clocks)
{
    SCRIPT script;
    CHECK(!Result_Ignore(M_Read("date 0 2026-02-30\n", &script)));
    CHECK(!Result_Ignore(M_Read("date 0 bogus\n", &script)));
    CHECK(!Result_Ignore(
        M_Read("date 0 2026-01-01\ndate 0 2026-01-02\n", &script)));
    CHECK(!Result_Ignore(M_Read("clock 0 1 1\nclock 0 1 2\n", &script)));
}

TEST(dates_and_clocks_record_in_frame_order_and_write_back)
{
    SCRIPT script = {};
    Array_Init(&script.inputs, &g_HeapAllocator);
    Array_Init(&script.fps, &g_HeapAllocator);
    Array_Init(&script.dates, &g_HeapAllocator);
    Array_Init(&script.clocks, &g_HeapAllocator);
    Script_RecordDate(&script, 0, 20261127);
    Script_RecordDate(&script, 0, 20261128);
    CHECK_EQ_INT(script.dates.count, 1);
    CHECK_EQ_INT(Script_Date(&script, 0), 20261128);
    Script_RecordClock(&script, 2, 5, 100);
    Script_RecordClock(&script, 2, 3, 50);
    Script_RecordClock(&script, 2, 3, 60);
    uint64_t microseconds;
    CHECK(Script_Clock(&script, 2, 3, &microseconds));
    CHECK_EQ_INT(microseconds, 60);
    CHECK(Script_Clock(&script, 2, 5, &microseconds));
    CHECK_EQ_INT(microseconds, 100);
    char *const text = M_Write(&script);
    CHECK(strstr(text, "date 0 2026-11-28\n") != nullptr);
    CHECK(strstr(text, "clock 2 3 60\n") != nullptr);
    CHECK(strstr(text, "clock 2 5 100\n") != nullptr);
    Mem_Free(text);
    Script_Free(&script);
}
