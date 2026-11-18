#include "script.h"
#include "session.h"

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
        "OTINPUT 1\nframes 2\naction 2 ending 1",
        "OTINPUT 1\nframes 2\naction 1 ending 1\naction 0 ending 1",
        "OTINPUT 1\nframes 2\naction 0 ending",
        "OTINPUT 1\nframes 2\naction 0 ../ending 1",
        "OTINPUT 1\nframes 2\naction 0 ending -1",
        "OTINPUT 1\nframes 2\nfps 0 nan",
        "OTINPUT 1\nframes 2\nfps 0 inf",
        "OTINPUT 1\nframes 2\nfps 0 -1",
        "OTINPUT 1\nframes 2\nfps 2 60",
        "OTINPUT 1\nframes 2\nfps 0",
        "OTINPUT 1\nframes 2\nfps 0 60 extra",
        "OTINPUT 1\nframes 2\nfps 0 60\nfps 0 61",
        "OTINPUT 1\nframes 2\nfps 0 60\nfps 1 none",

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

TEST(headless_frames_keep_capture_points_without_waiting_for_pixels)
{
    CAPTURE_SESSION session = {};
    CHECK(Result_Ignore(
        M_Read("OTINPUT 1\nframes 2\ncapture 0\ncapture 1", &session.script)));
    CHECK(Script_Capture(&session.script, 0));
    CHECK(Result_Ignore(Session_Presented(&session, 0, 0, 0, 0)));
    CHECK(session.frame == 1 && !Session_Done(&session));
    CHECK(Script_Capture(&session.script, 1));
    CHECK(Result_Ignore(Session_Presented(&session, 0, 0, 0, 0)));
    CHECK(session.frame == 2 && Session_Done(&session));
    CHECK(!Result_Ignore(Session_Presented(&session, 0, 0, 0, 0)));
    Session_Close(&session);
}

static uint32_t m_ActionValues;

static RESULT M_Action(const uint32_t value)
{
    m_ActionValues = m_ActionValues * 100 + value;
    return OK;
}

TEST(actions_run_once_after_their_frame_in_source_order)
{
    CAPTURE_SESSION session = {};
    CHECK(Result_Ignore(M_Read(
        "OTINPUT 1\nframes 2\naction 0 mark 11\naction 0 mark 22\n"
        "action 1 mark 33\ncapture 0",
        &session.script)));
    const SESSION_ACTION actions[] = { { "mark", M_Action } };
    CHECK(Result_Ignore(Session_SetActions(&session, actions, 1)));
    m_ActionValues = 0;
    CHECK(Result_Ignore(Session_Presented(&session, 0, 0, 0, 0)));
    CHECK(m_ActionValues == 1122);
    CHECK(Result_Ignore(Session_Presented(&session, 0, 0, 0, 0)));
    CHECK(m_ActionValues == 112233);
    CHECK(Session_Done(&session));
    Session_Close(&session);
}

TEST(unknown_actions_fail_before_advancing_the_session)
{
    CAPTURE_SESSION session = {};
    CHECK(Result_Ignore(
        M_Read("OTINPUT 1\nframes 1\naction 0 unknown 1", &session.script)));
    CHECK(!Result_Ignore(Session_SetActions(&session, nullptr, 0)));
    CHECK(session.frame == 0);
    Session_Close(&session);
}

TEST(fps_observations_keep_availability_and_changes)
{
    INPUT_SCRIPT script = {};
    CHECK(IS_OK(M_Read(
        "OTINPUT 1\nframes 100\nfps 0 none\nfps 30 59.94\nfps 60 60.00",
        &script)));
    CHECK(!Script_Fps(&script, 0)->available);
    CHECK(!Script_Fps(&script, 29)->available);
    CHECK(Script_Fps(&script, 30)->available);
    CHECK(Script_Fps(&script, 59)->rate == 59.94f);
    CHECK(Script_Fps(&script, 60)->rate == 60.0f);
    CHECK(Script_Fps(&script, 99)->rate == 60.0f);
    CHECK(Script_Fps(&script, 100) == nullptr);
    Script_Free(&script);
}
