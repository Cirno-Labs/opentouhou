#include "script.h"

#include "harness_game.h"
#include "test.h"

#include "core/base/memory.h"
#include "core/base/string.h"

static RESULT M_Read(const char *const text, SCRIPT *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, text, strlen(text), "inputs.txt");
    const RESULT result = Script_Read(&reader, &m_TestGame, out);
    Reader_Close(&reader);
    return result;
}

static char *M_Write(const SCRIPT *const script)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Script_Write(&writer, &m_TestGame, script);
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
