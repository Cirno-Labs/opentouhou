#include "session.h"

#include <stdio.h>
#include <string.h>
#include <SDL3/SDL_timer.h>

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
    session->screenshots = true;
    return OK;
}

RESULT Session_SetActions(
    CAPTURE_SESSION *const session, const SESSION_ACTION *const actions,
    const size_t count)
{
    for (int32_t i = 0; i < session->script.actions.count; i++) {
        const char *const name = session->script.actions.items[i].name;
        bool found = false;
        for (size_t j = 0; j < count; j++) {
            if (strcmp(name, actions[j].name) == 0
                && actions[j].apply != nullptr) {
                found = true;
                break;
            }
        }
        FAIL_IF(!found, "unknown session action %s", name);
    }
    session->actions = actions;
    session->action_count = count;
    return OK;
}

RESULT Session_Presented(
    CAPTURE_SESSION *const session, const int x, const int y, const int width,
    const int height)
{
    FAIL_IF(Session_Done(session), "presentation after the input script ended");
    if (session->screenshots
        && Script_Capture(&session->script, session->frame)) {
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
    while (session->next_action < session->script.actions.count
           && session->script.actions.items[session->next_action].frame
               == session->frame) {
        const SCRIPT_ACTION *const action =
            &session->script.actions.items[session->next_action];
        bool applied = false;
        for (size_t i = 0; i < session->action_count; i++) {
            if (strcmp(action->name, session->actions[i].name) == 0
                && session->actions[i].apply != nullptr) {
                MUST(session->actions[i].apply(action->value));
                applied = true;
                break;
            }
        }
        FAIL_IF(!applied, "session action %s has no binding", action->name);
        session->next_action++;
    }
    const uint64_t now = SDL_GetTicksNS();
    const uint64_t elapsed = now - session->last_present;
    if (!session->fast && session->last_present != 0
        && elapsed < SDL_NS_PER_SECOND / 60) {
        SDL_DelayNS(SDL_NS_PER_SECOND / 60 - elapsed);
    }
    session->last_present = SDL_GetTicksNS();
    session->frame++;
    if (Session_Done(session)) {
        printf("SESSION end %u\n", session->frame);
        FAIL_IF(fflush(stdout) != 0, "cannot notify the capture controller");
    } else if (session->frame % 600 == 0) {
        printf("SESSION progress %u\n", session->frame);
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
