#pragma once

// Sets up th07 gameplay for tests: the real bullets, effects and enemies
// on test sprites. th07_play_setup.c defines the globals of the modules
// that a test links stand-ins for, and silent stand-ins for most of them;
// each test defines Player_AngleFrom, Item_Spawn, ScreenEffect_Shake and
// ScreenEffect_Flash.

#include <stdint.h>

// The first sprites of each height in the test etama.anm.
#define TH07_PLAY_SETUP_SPRITE_8 0x200
#define TH07_PLAY_SETUP_SPRITE_16 0x210
#define TH07_PLAY_SETUP_SPRITE_32 0x220
#define TH07_PLAY_SETUP_SPRITE_64 0x230

// Writes and opens the data directory of case_name, seeds g_Rng with seed,
// resets the game state and registers the bullets, effects and enemies.
// etama.anm holds 16 sprites each 8, 16, 32 and 64 high; the flying
// scripts of types 0, 1 to 6, 7 to 9 and 10 show the first sprite of each
// height. Every effect type runs a script that does not end. Returns the
// directory, valid until Th07PlaySetup_TearDown.
const char *Th07PlaySetup_SetUp(const char *case_name, uint16_t seed);

// Releases what Th07PlaySetup_SetUp set up.
void Th07PlaySetup_TearDown(void);
