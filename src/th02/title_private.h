#pragma once

// title.c's own state, exposed for tests; nothing outside the title
// module sees this.

#include "th02/title.h"

typedef struct {
    const TITLE_FLASH_IMAGE *images;
    uint8_t frame;
    bool active;
} TITLE_FLASH;

typedef enum {
    TITLE_PREPARE,
    TITLE_MENU_BACKGROUND,
    TITLE_BACKGROUND,
    TITLE_DOOR,
    TITLE_HOLD,
    TITLE_FLASHING,
    TITLE_CREDITS,
    TITLE_FADE_BEGIN,
    TITLE_FADE,
    TITLE_DONE,
} TITLE_PHASE;

typedef struct {
    const TITLE_ASSETS *assets;
    uint8_t demo_num;
    bool midi_possible;
    bool midi_active;
    PALETTE_FADE fade;
    TITLE_PHASE phase;
    uint8_t door_step;
} TITLE;

// A writable view of title.c's private state, for tests.
extern TITLE *const p_Title;

// A writable view of title.c's flash state, for tests.
extern TITLE_FLASH *const p_TitleFlash;
