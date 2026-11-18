#include "session.h"

#include <stdio.h>
#include <string.h>

RESULT Session_Open(
    CAPTURE_SESSION *const session, const char *const path,
    const SCRIPT_BINDING *const bindings, const size_t binding_count)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    INPUT_SCRIPT script;
    const RESULT result =
        Script_Read(&reader, bindings, binding_count, &script);
    Reader_Close(&reader);
    MUST(result);
    memset(session, 0, sizeof(*session));
    session->script = script;
    return OK;
}

RESULT Session_Presented(
    CAPTURE_SESSION *const session, const int x, const int y, const int width,
    const int height)
{
    FAIL_IF(Session_Done(session), "presentation after the input script ended");
    if (Script_Capture(&session->script, session->frame)) {
        FAIL_IF(width <= 0 || height <= 0, "invalid capture rectangle");
        printf(
            "SESSION capture %u %d %d %d %d\n", session->frame, x, y, width,
            height);
        FAIL_IF(fflush(stdout) != 0, "cannot notify the capture controller");
        char acknowledgement[64];
        FAIL_IF(
            fgets(acknowledgement, sizeof(acknowledgement), stdin) == nullptr,
            "capture controller disconnected");
        char expected[64];
        snprintf(expected, sizeof(expected), "CAPTURED %u\n", session->frame);
        FAIL_IF(
            strcmp(acknowledgement, expected) != 0,
            "wrong capture acknowledgement for frame %u", session->frame);
    }
    session->frame++;
    if (Session_Done(session)) {
        printf("SESSION end %u\n", session->frame);
        FAIL_IF(fflush(stdout) != 0, "cannot notify the capture controller");
    }
    return OK;
}

bool Session_Done(const CAPTURE_SESSION *const session)
{
    return session->frame >= session->script.frames;
}

void Session_Close(CAPTURE_SESSION *const session)
{
    Script_Free(&session->script);
    session->frame = 0;
}
