#pragma once

#include "core/math/vec.h"

// Runs the items that enemies, cleared bullets and the player drop: moves
// them, and collects those that touch the graze box, which changes the
// score, the power, the bombs, the lives and the rank. While the player is
// at full power above y 128, every falling item flies to it. The bullet
// update runs the items, so the items have no chain elements of their own.
// The item state is module state. Drawing comes with graphics.

#define ITEM_COUNT 0x200

// Selects the sprite of an item and what collecting it does. Enemy
// scripts select a type by number, so the numbers are fixed.
typedef enum {
    ITEM_POWER = 0,
    ITEM_POINT = 1, // scores more the higher it is collected
    ITEM_BIG_POWER = 2, // 8 power
    ITEM_BOMB = 3,
    ITEM_FULL_POWER = 4,
    ITEM_LIFE = 5,
    ITEM_STAR = 6, // from a cleared bullet; scores by the graze, 100 in a bomb
} ITEM_TYPE;

typedef enum {
    ITEM_FALLING = 0,
    ITEM_HOMING = 1, // flies to the player
    // Flies from where it dropped to a random place over 60 frames, then
    // falls. The player's death drops these.
    ITEM_THROWN = 2,
} ITEM_STATE;

// Removes every item.
void Item_Reset(void);
// Moves the items and collects those that touch the graze box.
void Item_Update(void);
// Drops an item of type at pos in the first free slot from the one after
// the last used. Does nothing when no slot is free.
void Item_Spawn(VEC3 pos, ITEM_TYPE type, ITEM_STATE state);
// Makes every item fly to the player.
void Item_CollectAll(void);
