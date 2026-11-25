#pragma once

#include "th01/frame.h"
#include "th01/grc.h"
#include "th01/palette.h"
#include "th01/player.h"

#include "core/vfs.h"

typedef struct {
    GRC kuji[2];
} BOMB_ASSETS;

typedef enum {
    BOMB_UPDATE_WAIT,
    BOMB_UPDATE_RUNNING,
    BOMB_UPDATE_DONE,
} BOMB_UPDATE;

// Clears all state for a new gameplay session.
void Bomb_Init(void);

// Loads and validates KUZI1.GRC and KUZI2.GRC before simulation. Keep the
// caller's grouped allocator alive with these immutable images. A failed
// load leaves out empty; the allocator retains any intermediate storage.
RESULT Bomb_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, BOMB_ASSETS *out);
RESULT Bomb_ValidateAssets(const BOMB_ASSETS *assets);

// Retains state across bombs, including old kuji positions. Begin sets
// invincibility and runs to completion or a raw wait.
// frame is the signed low word of player.bomb_frame; neither function
// advances it. Required pellet and sound hooks execute in source order.
// All images and palettes must be loaded before ticks; updates allocate
// nothing. Keep borrowed assets stable while suspended. Stage palette is
// read at completion. No new Begin is allowed while waiting.
BOMB_UPDATE Bomb_BeginUpdate(
    const PTN *miko, const BOMB_ASSETS *assets, int16_t frame);

// Resume only after WAIT. Repeated polling before the next edge has no
// side effects. Scroll changes the display origin, not page pixels.
BOMB_UPDATE Bomb_ResumeUpdate(void);
bool Bomb_DealsDamage(int16_t remaining_lives, uint32_t frame);
