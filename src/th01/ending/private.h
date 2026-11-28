#pragma once

#include "th01/ending/ending.h"

#define ENDING_OPERATION_CAPACITY 4096

typedef enum {
    ENDING_AUDIO_IRIS,
    ENDING_AUDIO_FADE,
    ENDING_AUDIO_STOP,
    ENDING_AUDIO_STAGE1,
} ENDING_AUDIO;

typedef enum {
    END_LOAD,
    END_PICTURE,
    END_TONE,
    END_WAIT,
    END_CLEAR,
    END_SCROLL,
    END_AUDIO,
    END_TEXT,
    END_RESULT_LINES,
    END_TITLE,
    END_INPUT,
    END_REGISTER,
} ENDING_OPERATION_KIND;

typedef struct {
    ENDING_OPERATION_KIND kind;
    int32_t value;
    int32_t left;
    int32_t top;
    int32_t style;
    const char *text;
} ENDING_OPERATION;

typedef struct {
    PALETTE_STATE palette;
    PALETTE_FADE switch_fade;
    VSYNC_WAIT switch_wait;
    ENDING_OPERATION operations[ENDING_OPERATION_CAPACITY];
    int32_t count;
    int32_t next;
    int32_t timeout;
    bool waiting;
    bool sensing;
    char result_lines[10][256];
} ENDING;

// Writable view of the ending sequence for tests.
extern ENDING *const p_Ending;
