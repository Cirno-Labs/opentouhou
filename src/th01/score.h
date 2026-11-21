#pragma once

#include "th01/frame.h"
#include "th01/input.h"
#include "th01/palette.h"
#include "th01/pi.h"
#include "th01/score_data.h"
#include "th01/text.h"

typedef enum {
    SCORE_GAMEOVER,
    SCORE_ENDING,
} SCORE_CONTEXT;

typedef struct {
    PI_IMAGE gameover;
    GAME_TEXT_FONT font;
} SCORE_ASSETS;

typedef enum {
    SCORE_FADE_WAIT,
    SCORE_FADE_DELAY,
    SCORE_ENTRY_INPUT,
    SCORE_ENTRY_DELAY,
    SCORE_RELEASE,
    SCORE_DONE,
} SCORE_PHASE;

typedef struct {
    const SCORE_ASSETS *assets;
    VIDEO *video;
    SCORE_SAVES *saves;
    FRAME_VSYNC *vsync;
    GAME_INPUT *input;
    PALETTE_STATE palette;
    PALETTE_FADE fade;
    FRAME_WAIT wait;
    uint64_t delay_until;
    SCORE_CONTEXT context;
    SCORE_PHASE phase;
    int32_t rank, place, row, column, cursor, inputs;
    char slots[8][9];
} SCORE_SCENE;

// Loads GAME_O.GRP and all static, imported and saved table glyphs from rom
// before ticks. Storage belongs to allocator and must outlive the score scene.
RESULT Score_LoadAssets(
    VFS *vfs, const FONT_ROM *rom, const SCORE_SAVES *saves,
    const ALLOCATOR *allocator, SCORE_ASSETS *out);

// Borrows the world interrupt counters and input flags across registration.
// Game Over loads and fades its prepared background; the results screen retains
// the handed pages and palette. The results screen omits the last-slot jump and
// timeout. The results screen senses arrows only and resets count1 at
// four-frame wait entry; Game Over also senses keypad directions and resets
// count1 on wait exit. Entry stage uses source values: ordinary stage 1..20,
// clear 50 or 60. Begin and Tick allocate nothing and perform no IO. Completion
// queues a save; drain it outside simulation with ScoreData_WriteSaves.
void Score_Begin(
    SCORE_SCENE *scene, const SCORE_ASSETS *assets, VIDEO *video,
    SCORE_SAVES *saves, FRAME_VSYNC *vsync, GAME_INPUT *input,
    SCORE_CONTEXT context, int32_t rank, const SCORE_ENTRY *entry);

// Advances supplied VSync exactly once. The application advances audio
// separately once per host tick. Returns true after registration completes.
bool Score_Tick(
    SCORE_SCENE *scene, const TH01_INPUT *input, bool reference_edge,
    uint64_t host_tick);

// Applies one already-sensed source input, in direction/shot/strike order.
// Returns true for Enter. Exposed for exhaustive grid and input checks.
bool Score_OnInput(SCORE_SCENE *scene);
