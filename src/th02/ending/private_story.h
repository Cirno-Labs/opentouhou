#pragma once

// story.c's own state, exposed for tests; nothing outside the ending/
// module sees this.

#include "th02/ending/story.h"
#include "th02/ending/text.h"
#include "th02/wait.h"

typedef enum {
    STORY_BEGIN,
    STORY_INTRO_HOLD,
    STORY_INTRO_PICTURE,
    STORY_INTRO_BLACK_OUT,
    STORY_BEFORE_LINE,
    STORY_LINE_BEGIN,
    STORY_LINE,
    STORY_AFTER_LINE,
    STORY_CHANGE_PICTURE,
    STORY_SCROLL,
    STORY_WHITE_OUT,
    STORY_CREDITS_MUSIC,
    STORY_MEASURE_BEGIN,
    STORY_MEASURE,
    STORY_SHIFT,
    STORY_DELAY,
    STORY_FADE,
    STORY_DONE,
} STORY_PHASE;

typedef struct {
    const ENDING_ASSETS *assets;
    STORY_AUDIO music;
    void *music_context;
    WAIT wait;
    ENDING_TEXT text;
    STORY_PHASE phase;
    STORY_PHASE after_wait;
    ENDING_STORY route;
    int16_t left;
    uint8_t shot;
    uint8_t line;
    uint8_t scroll;
} STORY;

// A writable view of story.c's private state, for tests.
extern STORY *const p_Story;
