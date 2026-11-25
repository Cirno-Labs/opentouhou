#pragma once

#include "th01/player.h"

#include "core/vfs.h"

#define SHOOTOUT_LASERS_COUNT 10

typedef struct {
    uint8_t masks[8][8][2];
} SHOOTOUT_LASERS_ASSETS;

// Clears all laser slots for a new gameplay session.
void ShootoutLasers_Init(void);

// Loads all eight widths and pre-shifts from laser_s.bmp into the caller's
// grouped asset allocator; leaves out null on error.
RESULT ShootoutLasers_LoadAssets(
    VFS *vfs, const ALLOCATOR *allocator, SHOOTOUT_LASERS_ASSETS **out);

// Spawns a laser of width 1..8 in free slot 0..9, in Q24.8 positions.
void ShootoutLasers_Spawn(
    int32_t slot, int16_t origin_left, int16_t origin_y, int16_t target_left,
    int16_t target_y, int16_t speed_times_eight, uint8_t color,
    int16_t detach_age, int16_t width);

// Updates slot 0..9 or all ten in order: extends, detaches, draws and hits.
// Invincibility or damaging=false suppresses hits; sliding does not.
void ShootoutLasers_Update(int32_t slot, const SHOOTOUT_LASERS_ASSETS *assets);
void ShootoutLasers_UpdateAll(const SHOOTOUT_LASERS_ASSETS *assets);

// Erases and kills the first count (0..10) lasers.
// Restores from the original's truncated low-16-bit start coordinates.
void ShootoutLasers_EraseAndReset(int32_t count);
