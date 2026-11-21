#pragma once

#include "th01/input.h"
#include "th01/palette.h"
#include "th01/pi.h"
#include "th01/settings.h"
#include "th01/text.h"

#include "core/vfs.h"

#define OPENING_TRACK_COUNT 15

typedef struct {
    PI_IMAGE title, revealed, window;
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
    VIDEO *video;
    TH01_SETTINGS *settings;
    const OPENING_ASSETS *assets;
    PALETTE_STATE palette;
    PALETTE_FADE fade;
    MENU_INPUT input;
    OPENING_PHASE phase;
    OPENING_ACTION exit_action;
    uint32_t random, frame_rand;
    int32_t count1, line_count, line_y[2], prompt_frame;
    int32_t selection, track, option_item_count, music_mode, wait;
    bool drawn_rows[VIDEO_HEIGHT];
    bool entering, left_locked[2], right_locked[2];
} OPENING;

// Opens the canonical UTF-8 archive name, with a literal CP932 fallback
// for POSIX data folders. Mounts its PF source above loose files.
RESULT Opening_MountArchive(VFS *vfs);

// Decodes all images and caches every menu glyph from rom before ticks.
// Storage belongs to allocator. Discard the complete asset allocation group on
// failure; partially decoded images can remain in out. Close the VFS after
// loading; no reader is retained.
RESULT Opening_LoadAssets(
    VFS *vfs, const FONT_ROM *rom, const ALLOCATOR *allocator,
    OPENING_ASSETS *out);

// Returns a static MDT filename for a track from 0 through 14.
const char *Opening_TrackFile(int32_t track);

// Starts OP's title sequence with a separate default RNG seed of 1.
// Assets, settings and initialized video must outlive the scene. Emits the
// unconditional initial reimu play request, even with music set to OFF.
OPENING_EVENTS Opening_Init(
    OPENING *opening, VIDEO *video, TH01_SETTINGS *settings,
    const OPENING_ASSETS *assets);

// Advances one reference VSync, without allocation, I/O, platform calls or
// audio feedback. Uses deterministic zero-time asset loading: the sixteen
// fade VSyncs count toward the title's shared 100-VSync delay. The white
// lines run for 100 VSyncs before the prompt. any_key is a latched BIOS-like
// keyboard event, cleared only when clear_key_buffer is emitted. Bound
// menu controls are read directly, including held confirmation across menus.
// Start/Continue request the music fade, leave frame_rand unchanged, stop
// count1, fade the palette to black over the next sixteen ticks, and emit a
// scene boundary action on the last of them without inventing gameplay or
// resident state.
// Consume these events in the game driver. If unhandled, the action repeats
// on every following tick.
// Audio events are control requests only: PLAY means stop/load/play, FADE
// means the source's nonblocking fade request. No PCM/device is connected.
OPENING_EVENTS Opening_Tick(
    OPENING *opening, const TH01_INPUT *raw, bool any_key);
