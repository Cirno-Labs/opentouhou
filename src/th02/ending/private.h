#pragma once

// ending.c's own state, exposed for tests; nothing outside the ending/
// module sees this. Umbrella for the ending/ module's private parts.

#include "th02/ending/ending.h"
#include "th02/ending/private_verdict.h"
#include "th02/ending/story.h"
#include "th02/wait.h"

typedef enum {
    ENDING_PREPARE,
    ENDING_STORY_SCENE,
    ENDING_CREDITS,
    ENDING_VERDICT,
    ENDING_FINISH_IN,
    ENDING_FINISH_HOLD,
    ENDING_FINISH_KEY,
    ENDING_FINISH_OUT,
    ENDING_DONE,
} ENDING_PHASE;

typedef struct ENDING {
    const ENDING_ASSETS *assets;
    const TH02_SESSION *session;
    STORY_AUDIO music;
    void *music_context;
    bool extra_unlocked;
    WAIT wait;
    ENDING_PHASE phase;
    ENDING_STORY route;
} ENDING;

// A writable view of ending.c's private state, for tests.
extern ENDING *const p_Ending;
