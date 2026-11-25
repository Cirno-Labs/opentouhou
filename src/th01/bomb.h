#pragma once

#include "th01/vsync_irq.h"
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

// Loads KUZI1.GRC and KUZI2.GRC from the caller's grouped allocator.
// A failed load leaves out empty.
RESULT Bomb_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, BOMB_ASSETS *out);
RESULT Bomb_ValidateAssets(const BOMB_ASSETS *assets);

// Begin sets invincibility and runs the bomb until done or a wait.
// Keep the assets stable while suspended; never Begin during a wait.
BOMB_UPDATE Bomb_BeginUpdate(
    const PTN *miko, const BOMB_ASSETS *assets, int16_t frame);

// Resume only after WAIT. Repeated polling before the next edge has no
// side effects. Scroll changes the display origin, not page pixels.
BOMB_UPDATE Bomb_ResumeUpdate(void);
bool Bomb_DealsDamage(int16_t remaining_lives, uint32_t frame);
