#pragma once

#include <stddef.h>
#include <stdint.h>

// Describes retail 1.02h. It has no relocations, so addresses are absolute.

// Patch sites, each followed by the value it holds.
#define RETAIL_TITLE_CALLBACK_SITE 0x0043a395
#define RETAIL_MAIN_MENU_ON_UPDATE 0x0043579f
#define RETAIL_PLAY_ON_UPDATE_2_SITE 0x0042a3d8
#define RETAIL_PLAY_ON_UPDATE_2 0x0042a510
#define RETAIL_RUN_CALC_CHAIN_CALL 0x00420853
#define RETAIL_RUN_CALC_CHAIN 0x0041ca10

// Restarts the update chain within the frame; returned by PlayOnUpdate2.
#define RETAIL_CHAIN_RESTART 6

#define RETAIL_MAIN_MENU_TITLE 1
#define RETAIL_MAIN_MENU_TITLE_KEY 2

#define RETAIL_SCENE_GAMEPLAY 2

typedef int(__cdecl *RETAIL_CALLBACK)(void *arg);

#define RETAIL_LOAD_FILE_DATA                                                  \
    ((void *(__cdecl *)(const char *path, int unused))0x0041e290)
#define RETAIL_VALIDATE_REPLAY                                                 \
    ((int(__cdecl *)(void *data, int size))0x0042a140)
#define RETAIL_FREE ((void(__cdecl *)(void *data))0x0045baed)
#define RETAIL_LAST_FILE_SIZE (*(int32_t *)0x0069d914)

// Rng's seed, g_Random at 0x69d8f8.
#define RETAIL_RNG_SEED (*(uint16_t *)0x0069d8f8)
// The held buttons, g_Buttons at 0x69d904.
#define RETAIL_BUTTONS (*(uint16_t *)0x0069d904)

typedef struct {
    uint8_t pad_0000[0x04];
    uint32_t score;
    uint8_t pad_0008[0x08];
    int32_t difficulty;
    uint8_t pad_0014[0x08];
    uint32_t is_in_replay;
    uint8_t pad_0020[0x17f0];
    uint16_t power;
    uint8_t pad_1812[0x08];
    int8_t lives;
    int8_t bombs;
    uint8_t pad_181c[0x01];
    uint8_t character;
    uint8_t shot_type;
    uint8_t pad_181f[0x02];
    uint8_t is_playing;
    uint8_t pad_1822[0x02];
    uint8_t is_in_demo;
    uint8_t pad_1825[0x07];
    char replay_file[0x200];
    uint8_t pad_1a2c[0x04];
    int32_t stage_frames;
    int32_t stage;
    uint8_t pad_1a38[0x38];
    int32_t rank;
} RETAIL_GAME_MANAGER;
static_assert(offsetof(RETAIL_GAME_MANAGER, score) == 0x04);
static_assert(offsetof(RETAIL_GAME_MANAGER, is_in_replay) == 0x1c);
static_assert(offsetof(RETAIL_GAME_MANAGER, power) == 0x1810);
static_assert(offsetof(RETAIL_GAME_MANAGER, lives) == 0x181a);
static_assert(offsetof(RETAIL_GAME_MANAGER, character) == 0x181d);
static_assert(offsetof(RETAIL_GAME_MANAGER, is_playing) == 0x1821);
static_assert(offsetof(RETAIL_GAME_MANAGER, is_in_demo) == 0x1824);
static_assert(offsetof(RETAIL_GAME_MANAGER, replay_file) == 0x182c);
static_assert(offsetof(RETAIL_GAME_MANAGER, stage_frames) == 0x1a30);
static_assert(offsetof(RETAIL_GAME_MANAGER, stage) == 0x1a34);
static_assert(offsetof(RETAIL_GAME_MANAGER, rank) == 0x1a70);

typedef struct {
    uint8_t pad_0000[0x18c];
    int32_t scene;
} RETAIL_APP;
static_assert(offsetof(RETAIL_APP, scene) == 0x18c);

typedef struct {
    uint8_t pad_0000[0x81f0];
    int32_t state;
} RETAIL_MAIN_MENU;
static_assert(offsetof(RETAIL_MAIN_MENU, state) == 0x81f0);

#define RETAIL_GAME_MANAGER_AT ((RETAIL_GAME_MANAGER *)0x0069bca0)
#define RETAIL_APP_AT ((RETAIL_APP *)0x006c6d18)

// stages[i] is the file offset of stage i's data, or 0; slot 0 is unused.
typedef struct {
    uint8_t pad_00[0x06];
    uint8_t shot_type;
    uint8_t difficulty;
    uint8_t pad_08[0x28];
    uint32_t stages[8];
} RETAIL_REPLAY_HEADER;
static_assert(offsetof(RETAIL_REPLAY_HEADER, stages) == 0x30);

typedef struct {
    uint8_t pad_00[0x09];
    uint8_t lives;
    uint8_t bombs;
} RETAIL_REPLAY_STAGE;
