#include "script.h"

#include "core/base/utils.h"
#include "test.h"

#include <string.h>

static const SCRIPT_BINDING m_Bindings[] = {
    { "shoot", 1 },
    { "bomb", 2 },
    { "left", 0x40 },
};

static RESULT M_Read(const char *const text, INPUT_SCRIPT *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, text, strlen(text), "test inputs");
    const RESULT result =
        Script_Read(&reader, m_Bindings, ARRAY_SIZE(m_Bindings), out);
    Reader_Close(&reader);
    return result;
}

TEST(buttons_hold_across_frames_and_release_explicitly)
{
    INPUT_SCRIPT script;
    CHECK(Result_Ignore(M_Read(
        "OTINPUT 1\nframes 100\ninput 5 shoot+left\ninput 10 bomb\ninput 11 "
        "none\n",
        &script)));
    CHECK(script.frames == 100);
    CHECK(Script_Buttons(&script, 0) == 0);
    CHECK(Script_Buttons(&script, 4) == 0);
    CHECK(Script_Buttons(&script, 5) == 0x41);
    CHECK(Script_Buttons(&script, 9) == 0x41);
    CHECK(Script_Buttons(&script, 10) == 2);
    CHECK(Script_Buttons(&script, 11) == 0);
    CHECK(Script_Buttons(&script, 100) == 0);
    Script_Free(&script);
}

TEST(captures_can_coincide_with_input_changes)
{
    INPUT_SCRIPT script;
    CHECK(Result_Ignore(M_Read(
        "# title\r\nOTINPUT 1\r\nframes 20\ninput 0 shoot # press\ncapture "
        "0\ninput 1 none\ncapture 19",
        &script)));
    CHECK(Script_Buttons(&script, 0) == 1);
    CHECK(Script_Capture(&script, 0));
    CHECK(!Script_Capture(&script, 1));
    CHECK(Script_Capture(&script, 19));
    CHECK(!Script_Capture(&script, 20));
    Script_Free(&script);
    CHECK(script.frames == 0 && script.inputs.items == nullptr);
}

TEST(an_empty_input_list_keeps_every_button_released)
{
    INPUT_SCRIPT script;
    CHECK(Result_Ignore(M_Read("OTINPUT 1\nframes 1", &script)));
    CHECK(Script_Buttons(&script, 0) == 0);
    CHECK(!Script_Capture(&script, 0));
    Script_Free(&script);
}

TEST(invalid_scripts_leave_the_output_unchanged)
{
    const char *const invalid[] = {
        "",
        "frames 1",
        "OTINPUT 2\nframes 1",
        "OTINPUT 1 extra\nframes 1",
        "OTINPUT 1\n",
        "OTINPUT 1\nframes 0",
        "OTINPUT 1\nframes -1",
        "OTINPUT 1\nframes 2147483648",
        "OTINPUT 1\nframes 999999999999999999999999",
        "OTINPUT 1\nframes 1a",
        "OTINPUT 1\nframes 1\nframes 2",
        "OTINPUT 1\nframes 1\ncapture 1",
        "OTINPUT 1\nframes 1\ninput 1 shoot",
        "OTINPUT 1\nframes 2\ninput 0 shoot\ninput 0 bomb",
        "OTINPUT 1\nframes 2\ncapture 1\ncapture 0",
        "OTINPUT 1\nframes 2\ncapture 0\ncapture 0",
        "OTINPUT 1\nframes 2\ninput 0 laser",
        "OTINPUT 1\nframes 2\ninput 0 shoot+shoot",
        "OTINPUT 1\nframes 2\ninput 0 +shoot",
        "OTINPUT 1\nframes 2\ninput 0 shoot+",
        "OTINPUT 1\nframes 2\ninput 0 none+shoot",
        "OTINPUT 1\nframes 2\ninput 0",
        "OTINPUT 1\nframes 2\ncapture 0 extra",
        "OTINPUT 1\nframes 2\nunknown 0",
    };
    for (size_t i = 0; i < ARRAY_SIZE(invalid); i++) {
        INPUT_SCRIPT script = { .frames = 123 };
        const RESULT result = M_Read(invalid[i], &script);
        CHECK(!IS_OK(result));
        Result_Ignore(result);
        CHECK(script.frames == 123 && script.inputs.items == nullptr);
    }
}

TEST(embedded_nuls_and_overlong_lines_are_rejected)
{
    const char bytes[] = "OTINPUT 1\nframes 1\n\0capture 0";
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes) - 1, nullptr);
    INPUT_SCRIPT script = {};
    CHECK(!Result_Ignore(
        Script_Read(&reader, m_Bindings, ARRAY_SIZE(m_Bindings), &script)));
    Reader_Close(&reader);
    char line[600];
    memset(line, ' ', sizeof(line));
    line[sizeof(line) - 1] = '\0';
    CHECK(!Result_Ignore(M_Read(line, &script)));
}

TEST(sparse_scripts_do_not_allocate_per_frame)
{
    INPUT_SCRIPT script;
    CHECK(Result_Ignore(M_Read(
        "OTINPUT 1\nframes 2147483647\ninput 0 shoot\ncapture 2147483646",
        &script)));
    CHECK(script.inputs.count == 1 && script.captures.count == 1);
    CHECK(Script_Buttons(&script, 2147483646) == 1);
    CHECK(Script_Capture(&script, 2147483646));
    Script_Free(&script);
}
