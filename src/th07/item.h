#pragma once

#include "th07/anm.h"

#include "core/math/vec.h"
#include "stg/timer.h"

#include <stdint.h>

// Runs the items that enemies, cleared bullets and the player drop: moves
// them at g_GameSpeed, and collects those that touch the collect box, which
// changes the score, the power, the cherry, the bombs, the lives and the
// rank; point items also win extends. The player pulls items in during the
// Border, and above its shot type's item line at full power or in Extra and
// Phantasm. The bullet update runs the items, so the items have no chain
// elements of their own. The item state is module state. Drawing comes
// with graphics.

#define ITEM_COUNT 0x44c

// Selects the sprite of an item and what collecting it does. Enemy
// scripts select a type by number, so the numbers are fixed.
typedef enum {
    ITEM_POWER = 0,
    ITEM_POINT = 1, // scores more the higher it is collected
    ITEM_BIG_POWER = 2, // 8 power
    ITEM_BOMB = 3,
    ITEM_FULL_POWER = 4,
    ITEM_LIFE = 5, // a bomb at 8 lives
    ITEM_STAR = 6, // from a cleared bullet; scores by the graze, adds cherry
    // Replaces power items at full power; adds cherry by the captures, and
    // scores as a point item once the cherry is full.
    ITEM_CHERRY = 7,
    ITEM_BORDER_STAR = 8, // from a bullet that the Border's break clears
    ITEM_BOSS_STAR = 9, // from a bullet cleared by the last boss's dialogue
} ITEM_TYPE;

typedef enum {
    ITEM_FALLING = 0,
    ITEM_HOMING = 1, // flies to the player
    // Flies from where it dropped to a random place over 60 frames, then
    // falls. The player's death drops these.
    ITEM_THROWN = 2,
} ITEM_STATE;

// Holds one item slot. An inactive slot keeps its last values.
typedef struct {
    ANM_VM vm;
    VEC3 pos;
    // Holds the speed, except that an ITEM_THROWN item keeps where it
    // dropped until it lands.
    VEC3 velocity;
    VEC3 target;
    TIMER timer;
    int8_t type;
    bool active;
    bool on_screen;
    int8_t state;
    bool full_value; // pulled in by the Border: a point item scores in full
    bool drawn; // left by the last Item_Update for Item_Draw
} ITEM;

// Returns the ITEM_COUNT slots in slot order. The view stays valid for
// the life of the module and reflects later updates.
const ITEM *Item_GetSlots(void);

// Removes every item.
void Item_Reset(void);

// Moves the items and collects those that touch the collect box.
void Item_Update(void);

// Drops an item of type at pos in the first free slot from the one after
// the last used; at full power, power and big power items drop as
// ITEM_CHERRY. Does nothing when no slot is free.
void Item_Spawn(VEC3 pos, ITEM_TYPE type, ITEM_STATE state);

// Makes every item fly to the player, from a slow rise.
void Item_CollectAll(void);

// Makes every homing item fall again, from a slow rise.
void Item_DropPulled(void);

// Records the items left by the last Item_Update, and indicators above the
// play area for those above it.
void Item_Draw(void);
