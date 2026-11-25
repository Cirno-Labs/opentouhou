#pragma once

#include "th01/frame.h"
#include "th01/input.h"
#include "th01/palette.h"
#include "pc98/zun/pi.h"
#include "th01/score_data.h"
#include "th01/text.h"

// Holds the four score tables that the game shows and registers into.
extern SCORE_SAVES g_ScoreSaves;

typedef enum {
    SCORE_GAMEOVER,
    SCORE_ENDING,
} SCORE_CONTEXT;

typedef struct {
    PI_IMAGE gameover;
    GAME_TEXT_FONT font;
} SCORE_ASSETS;

// Loads GAME_O.GRP and all static, imported and g_ScoreSaves glyphs from rom
// before ticks. Storage belongs to allocator and must outlive the score scene.
RESULT Score_LoadAssets(
    VFS *vfs, const FONT_ROM *rom, const ALLOCATOR *allocator,
    SCORE_ASSETS *out);

// Shares g_VSync and g_GameInput with gameplay across registration.
// Game Over loads and fades its prepared background; the results screen retains
// the handed pages and palette. The results screen omits the last-slot jump and
// timeout. The results screen senses arrows only and resets count1 at
// four-frame wait entry; Game Over also senses keypad directions and resets
// count1 on wait exit. Entry stage uses source values: ordinary stage 1..20,
// clear 50 or 60. Begin and Tick allocate nothing and perform no IO. Completion
// queues a save; drain it at the root tick end with ScoreData_WriteSaves.
void Score_Begin(
    const SCORE_ASSETS *assets, SCORE_CONTEXT context, int32_t rank,
    const SCORE_ENTRY *entry);

// Advances g_VSync exactly once. The application advances audio
// separately once per host tick. Returns true after registration completes.
bool Score_Tick(const TH01_INPUT *input, bool reference_edge);

// Applies one already-sensed source input, in direction/shot/strike order.
// Returns true for Enter. Exposed for exhaustive grid and input checks.
bool Score_OnInput(void);

void Score_Close(void);
