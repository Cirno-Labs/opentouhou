#pragma once

#include <stdint.h>

// Moves the cherry counters of g_GameManager.

// Adds amount to the cherry, up to its maximum. Outside the Supernatural
// Border, a gain also fills the plus, and a plus that reaches 50000
// starts the Border. A cherry that this fills shows the full popup.
void GameManager_AddCherry(int32_t amount);

// GameManager_AddCherry without the plus.
void GameManager_AddCherryNoPlus(int32_t amount);

// GameManager_AddCherry without the plus and the popup.
void GameManager_AddCherryQuiet(int32_t amount);

// Raises the cherry maximum by amount, up to 9,999,990.
void GameManager_RaiseCherryMax(int32_t amount);

// Takes amount from the cherry, down to 0.
void GameManager_SubCherry(int32_t amount);
