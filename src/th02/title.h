#pragma once

// The title screen's opening: the backgrounds, the text RAM door that
// opens while the tone falls, the flash over three images and the ZUN
// credit, stepped one event at a time.

#include "th02/draw.h"
#include "th02/palette.h"
#include "th02/sound_effects.h"
#include "th02/tram.h"

#define TITLE_FLASH_IMAGE_COUNT 3

typedef struct {
    const uint8_t *pixels;
    int16_t width;
    int16_t height;
} TITLE_FLASH_IMAGE;

typedef struct {
    SOUND_EFFECT force_effect;
    bool release_images;
    bool complete;
} TITLE_FLASH_EVENTS;

typedef struct {
    TITLE_FLASH_IMAGE view;
    GFX_COLOR palette[16];
} TITLE_IMAGE;

typedef struct {
    TITLE_IMAGE menu_background;
    TITLE_IMAGE title_background;
    TITLE_FLASH_IMAGE flash[TITLE_FLASH_IMAGE_COUNT];
    const GAIJI *glyphs;
    GFX_COLOR menu_palette[16];
} TITLE_ASSETS;

typedef enum {
    TITLE_LOAD_EFFECTS,
    TITLE_LOAD_MENU_SONG,
    TITLE_FORCE_EFFECT,
    TITLE_PLAY_SONG,
    TITLE_STOP_SONG,
} TITLE_AUDIO_ACTION;

typedef struct {
    TITLE_AUDIO_ACTION action;
    bool midi;
    uint8_t effect;
} TITLE_AUDIO_REQUEST;

typedef enum {
    TITLE_READY,
    TITLE_WAIT_FRAMES,
    TITLE_WAIT_VSYNC,
    TITLE_COMPLETE,
} TITLE_WAIT;

typedef struct {
    TITLE_WAIT wait;
    uint16_t count;
    TITLE_AUDIO_REQUEST audio[2];
    uint8_t audio_count;
    uint8_t release_images;
    uint8_t demo_num;
} TITLE_EVENTS;

// Starts the 18-frame title flash, requesting the pillar effect first.
TITLE_FLASH_EVENTS TitleFlash_Begin(
    const TITLE_FLASH_IMAGE images[TITLE_FLASH_IMAGE_COUNT]);

// Advances the flash one frame; completion restores page zero.
TITLE_FLASH_EVENTS TitleFlash_Advance(void);

// Clears the title state, once at startup.
void Title_Init(void);

// Starts the title screen; demo_num is the last demo shown, and the MIDI
// flags pick which driver plays the menu song.
void Title_Begin(
    const TITLE_ASSETS *assets, uint8_t demo_num, bool midi_possible,
    bool midi_active);

// Runs to the next audio event, frame wait or VSync wait.
TITLE_EVENTS Title_Step(void);
