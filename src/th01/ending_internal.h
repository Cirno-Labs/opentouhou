#pragma once

#include "th01/ending.h"

#define ENDING_OPERATION_CAPACITY 4096

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
    int32_t value, left, top, style;
    const char *text;
} ENDING_OPERATION;

struct ENDING {
    const ENDING_ASSETS *assets;
    ENDING_AUDIO_CONSUMER audio;
    void *audio_context;
    END_RESULT result;
    PALETTE_STATE palette;
    PALETTE_FADE switch_fade;
    FRAME_WAIT switch_wait;
    ENDING_OPERATION operations[ENDING_OPERATION_CAPACITY];
    int32_t count, next, timeout;
    bool waiting, sensing;
    char result_lines[10][256];
};
