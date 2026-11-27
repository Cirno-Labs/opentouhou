#pragma once

#include "th01/input.h"
#include "pc98/zun/pi.h"
#include "th01/text.h"

#include "core/vfs.h"

#define OPENING_TRACK_COUNT 15

typedef struct {
    PI_IMAGE title;
    PI_IMAGE revealed;
    PI_IMAGE window;
    GAME_TEXT_FONT font;
} OPENING_ASSETS;

typedef enum {
    OPENING_FADE,
    OPENING_HOLD,
    OPENING_LINES,
    OPENING_PROMPT,
    OPENING_MAIN,
    OPENING_OPTIONS,
    OPENING_MUSIC,
    OPENING_WAIT_MAIN,
    OPENING_WAIT_OPTIONS,
    OPENING_EXIT_FADE,
    OPENING_QUIT,
} OPENING_PHASE;

typedef enum {
    OPENING_ACTION_NONE,
    OPENING_ACTION_START,
    OPENING_ACTION_CONTINUE,
    OPENING_ACTION_QUIT,
} OPENING_ACTION;

typedef enum {
    OPENING_AUDIO_NONE,
    OPENING_AUDIO_STOP,
    OPENING_AUDIO_PLAY,
    OPENING_AUDIO_FADE,
} OPENING_AUDIO_ACTION;

typedef struct {
    OPENING_ACTION action;
    OPENING_AUDIO_ACTION audio;
    int32_t track;
    bool clear_key_buffer;
} OPENING_EVENTS;

typedef struct {
    OPENING_PHASE phase;
    uint32_t random;
    uint32_t frame_rand;
    uint32_t track_starts;
    int32_t count1;
    int32_t track;
} OPENING;

extern const OPENING *const g_Opening;

// Opens the canonical UTF-8 archive name, with a literal CP932 fallback
// for POSIX data folders. Mounts its PF source above loose files.
RESULT Opening_MountArchive(VFS *vfs);

// Decodes all images and caches every menu glyph from rom into allocator.
// Discard the whole allocation group on failure.
RESULT Opening_LoadAssets(
    VFS *vfs, const FONT_ROM *rom, const ALLOCATOR *allocator,
    OPENING_ASSETS *out);

// Starts the title sequence with RNG seed 1. Assets, settings and video
// must outlive the scene.
OPENING_EVENTS Opening_Init(const OPENING_ASSETS *assets);

// Advances one reference VSync; any_key is a latched key event, cleared only
// by clear_key_buffer. Consume the returned events in the game driver:
// an unhandled scene action repeats on every following tick.
OPENING_EVENTS Opening_Tick(const TH01_INPUT *raw, bool any_key);

void Opening_Close(void);

// Counts the VSync latched by a program switch (VSyncIrq_SwitchProgram).
void Opening_CountSwitchedVSync(void);
