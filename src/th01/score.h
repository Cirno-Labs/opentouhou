#pragma once

#include "th01/vsync_irq.h"
#include "th01/input.h"
#include "th01/palette.h"
#include "pc98/zun/pi.h"
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

// Loads score tables, GAME_O.GRP and their static and imported glyphs from rom
// before ticks. Storage belongs to allocator and must outlive the score scene.
RESULT Score_LoadAssets(
    VFS *vfs, VFS *user_files, const FONT_ROM *rom, const ALLOCATOR *allocator,
    SCORE_ASSETS *out);

// Starts score registration for Game Over or the results screen.
// Completion queues a save; drain it with Score_FlushSaves at the tick end.
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

// Copies the current rank's table high score into the live session.
void Score_SyncSession(void);

// Writes the queued score tables to user storage.
RESULT Score_FlushSaves(void);
