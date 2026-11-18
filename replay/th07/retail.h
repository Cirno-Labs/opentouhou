#pragma once

#include <stddef.h>
#include <stdint.h>

// Describes retail 1.00b. It has no relocations, so addresses are absolute.

// Functions that hold a patch site, each with its size in bytes, and the
// value the site holds: a call target or an immediate.
#define RETAIL_RUN_FRAME 0x004346e0
#define RETAIL_RUN_FRAME_SIZE 855
#define RETAIL_RUN_CALC_CHAIN 0x0042fd60
#define RETAIL_SET_UP_TITLE_SCENE 0x0045c5d0
#define RETAIL_SET_UP_TITLE_SCENE_SIZE 273
#define RETAIL_MAIN_MENU_ON_UPDATE 0x004554d6
#define RETAIL_REGISTER_REPLAY_CHAIN 0x00443aa0
#define RETAIL_REGISTER_REPLAY_CHAIN_SIZE 655
#define RETAIL_PLAY_ON_UPDATE_2 0x00442e50
#define RETAIL_APP_ON_UPDATE 0x00437c70
#define RETAIL_APP_ON_UPDATE_SIZE 1707
#define RETAIL_READ_BUTTONS 0x00430b50

// Restarts the update chain within the frame; returned by PlayOnUpdate2.
#define RETAIL_CHAIN_RESTART 6

#define RETAIL_SCENE_GAMEPLAY 2
#define RETAIL_SCENE_GAME_RESULT 6
#define RETAIL_SCENE_ENDING 9
#define RETAIL_SCENE_RESULT 5

#define RETAIL_DIFFICULTY_PHANTASM 5
#define RETAIL_PHANTASM_START_STAGE 7

// GameManager::stateFlags bits.
#define RETAIL_STATE_DEMO 0x02
#define RETAIL_STATE_PLAYING 0x04
#define RETAIL_STATE_IN_REPLAY 0x08

#define RETAIL_REPLAY_PLAYING 1

// The update callbacks are fastcall.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
typedef int(__attribute__((fastcall)) * RETAIL_CALLBACK)(void *arg);

#define RETAIL_LOAD_FILE_DATA                                                  \
    ((void *(__attribute__((fastcall)) *)(const char *path,                    \
                                          int external))0x00431330)
// Frees data and returns a decoded copy, or nullptr.
#define RETAIL_VALIDATE_REPLAY                                                 \
    ((void *(__attribute__((fastcall)) *)(void *data, int size))0x004433b0)
#pragma GCC diagnostic pop
#define RETAIL_FREE ((void(__cdecl *)(void *data))0x0047d285)
#define RETAIL_LAST_FILE_SIZE (*(int32_t *)0x004b9e64)

// The buttons the game plays by, g_PlayButtons at 0x4b9e50.
#define RETAIL_BUTTONS (*(uint16_t *)0x004b9e4c)
#define RETAIL_PLAY_BUTTONS (*(uint16_t *)0x004b9e50)

// g_Rng at 0x49fe20.
typedef struct {
    uint16_t seed;
    uint8_t pad_02[0x02];
    uint32_t generated;
} RETAIL_RNG;
static_assert(offsetof(RETAIL_RNG, generated) == 0x04);
#define RETAIL_RNG_AT ((RETAIL_RNG *)0x0049fe20)

typedef struct {
    int32_t previous;
    float sub_frame;
    int32_t current;
} RETAIL_TIMER;
static_assert(sizeof(RETAIL_TIMER) == 0x0c);

typedef struct {
    float x;
    float y;
    float z;
} RETAIL_VEC3;

// What GameManager::stats points at. The counts are floats.
typedef struct {
    uint32_t gui_score;
    uint32_t score;
    uint8_t pad_0008[0x0c];
    int32_t graze;
    int32_t graze_total;
    int32_t spell_cards_captured;
    uint8_t continues_used;
    uint8_t pad_0021[0x03];
    int32_t point_items;
    int32_t point_items_total;
    uint8_t pad_002c[0x24];
    float deaths;
    uint8_t pad_0054[0x08];
    float lives;
    uint8_t pad_0060[0x08];
    float bombs;
    float bombs_used;
    uint8_t pad_0070[0x0c];
    float power;
    uint8_t pad_0080[0x08];
    int32_t graze_count;
} RETAIL_GAME_STATS;
static_assert(offsetof(RETAIL_GAME_STATS, graze) == 0x14);
static_assert(offsetof(RETAIL_GAME_STATS, continues_used) == 0x20);
static_assert(offsetof(RETAIL_GAME_STATS, point_items) == 0x24);
static_assert(offsetof(RETAIL_GAME_STATS, deaths) == 0x50);
static_assert(offsetof(RETAIL_GAME_STATS, lives) == 0x5c);
static_assert(offsetof(RETAIL_GAME_STATS, bombs) == 0x68);
static_assert(offsetof(RETAIL_GAME_STATS, power) == 0x7c);
static_assert(offsetof(RETAIL_GAME_STATS, graze_count) == 0x88);

// g_GameManager at 0x626270.
typedef struct {
    uint8_t pad_0000[0x08];
    RETAIL_GAME_STATS *stats;
    int8_t is_frozen;
    uint8_t pad_000d[0x03];
    int32_t difficulty;
    uint8_t pad_0014[0x93c0];
    int8_t power_item_combo;
    uint8_t character;
    uint8_t shot_type;
    uint8_t shot_index;
    uint32_t state_flags;
    uint8_t is_paused;
    uint8_t is_in_continue_menu;
    uint8_t demo_index;
    uint8_t replay_menu_choice;
    uint8_t pad_93e0[0x04];
    char replay_file[0x200];
    uint8_t pad_95e4[0x04];
    int32_t stage_frames;
    int32_t stage;
    uint8_t pad_95f0[0x28];
    int32_t bonus_points_cap;
    int32_t bonus_points;
    int32_t invulnerability_bonus;
    uint8_t pad_9624[0x10];
    int32_t rank;
    uint8_t pad_9638[0x08];
    int32_t sub_rank;
} RETAIL_GAME_MANAGER;
static_assert(offsetof(RETAIL_GAME_MANAGER, difficulty) == 0x10);
static_assert(offsetof(RETAIL_GAME_MANAGER, power_item_combo) == 0x93d4);
static_assert(offsetof(RETAIL_GAME_MANAGER, state_flags) == 0x93d8);
static_assert(offsetof(RETAIL_GAME_MANAGER, replay_menu_choice) == 0x93df);
static_assert(offsetof(RETAIL_GAME_MANAGER, replay_file) == 0x93e4);
static_assert(offsetof(RETAIL_GAME_MANAGER, stage_frames) == 0x95e8);
static_assert(offsetof(RETAIL_GAME_MANAGER, bonus_points_cap) == 0x9618);
static_assert(offsetof(RETAIL_GAME_MANAGER, rank) == 0x9634);
static_assert(offsetof(RETAIL_GAME_MANAGER, sub_rank) == 0x9640);
static_assert(sizeof(RETAIL_GAME_MANAGER) == 0x9644);
#define RETAIL_GAME_MANAGER_AT ((RETAIL_GAME_MANAGER *)0x00626270)

// g_App at 0x575950.
typedef struct {
    uint8_t pad_0000[0x154];
    int32_t cur_scene;
    int32_t scene;
    uint8_t pad_015c[0x1c];
    float game_speed;
} RETAIL_APP;
static_assert(offsetof(RETAIL_APP, cur_scene) == 0x154);
static_assert(offsetof(RETAIL_APP, game_speed) == 0x178);
#define RETAIL_APP_AT ((RETAIL_APP *)0x00575950)

typedef struct {
    uint8_t pad_0000[0x08];
    int32_t sub_state;
    uint8_t pad_000c[0xd0ec];
    int32_t state;
} RETAIL_MAIN_MENU;
static_assert(offsetof(RETAIL_MAIN_MENU, state) == 0xd0f8);

// The title screen's menu state and sub-state.
#define RETAIL_MAIN_MENU_TITLE 0
#define RETAIL_MAIN_MENU_TITLE_SUB_STATE 1

typedef struct {
    uint16_t input;
    uint16_t events;
} RETAIL_REPLAY_FRAME;

typedef struct {
    uint8_t pad_00[0x2c];
    RETAIL_REPLAY_FRAME frames[];
} RETAIL_REPLAY_STAGE;

// In a file, stages[i] is the offset of stage i + 1's data, or 0; while
// a replay plays or records, a pointer. Phantasm takes the last slot.
typedef struct {
    uint8_t pad_00[0x1c];
    uintptr_t stages[7];
    uint8_t pad_38[0x1e];
    uint8_t shot_type;
    uint8_t difficulty;
} RETAIL_REPLAY_HEADER;
static_assert(offsetof(RETAIL_REPLAY_HEADER, stages) == 0x1c);
static_assert(offsetof(RETAIL_REPLAY_HEADER, shot_type) == 0x56);

// What g_ReplayManager at 0x4b9e48 points at, or nullptr.
typedef struct {
    int32_t frame;
    RETAIL_REPLAY_HEADER *data;
    uint8_t pad_0008[0x3c];
    int32_t mode;
    uint8_t pad_0048[0x3c];
    RETAIL_REPLAY_FRAME *cursor;
} RETAIL_REPLAY_MANAGER;
static_assert(offsetof(RETAIL_REPLAY_MANAGER, mode) == 0x44);
static_assert(offsetof(RETAIL_REPLAY_MANAGER, cursor) == 0x84);
#define RETAIL_REPLAY_MANAGER_PTR (*(RETAIL_REPLAY_MANAGER **)0x004b9e48)

// Player::bullets[i], a shot of the player's own fire.
typedef struct {
    uint8_t pad_0000[0x24c];
    RETAIL_VEC3 pos;
    uint8_t pad_0258[0xcc];
    RETAIL_VEC3 velocity;
    uint8_t pad_0330[0x08];
    float angle;
    RETAIL_TIMER timer;
    int16_t damage;
    int16_t state;
    uint8_t pad_034c[0x18];
} RETAIL_PLAYER_BULLET;
static_assert(offsetof(RETAIL_PLAYER_BULLET, velocity) == 0x324);
static_assert(offsetof(RETAIL_PLAYER_BULLET, timer) == 0x33c);
static_assert(offsetof(RETAIL_PLAYER_BULLET, damage) == 0x348);
static_assert(offsetof(RETAIL_PLAYER_BULLET, state) == 0x34a);
static_assert(sizeof(RETAIL_PLAYER_BULLET) == 0x364);

// g_Player at 0x4bdad8.
typedef struct {
    uint8_t pad_0000[0x930];
    RETAIL_VEC3 position;
    uint8_t pad_093c[0x1acc];
    int8_t player_state;
    uint8_t pad_2409[0x3b];
    RETAIL_PLAYER_BULLET bullets[0x60];
} RETAIL_PLAYER;
static_assert(offsetof(RETAIL_PLAYER, player_state) == 0x2408);
static_assert(offsetof(RETAIL_PLAYER, bullets) == 0x2444);
#define RETAIL_PLAYER_AT ((RETAIL_PLAYER *)0x004bdad8)

// Bullet::ex[i], one running behaviour of a bullet.
typedef struct {
    RETAIL_TIMER timer;
    uint8_t pad_0c[0x1c];
    int32_t turn_count;
} RETAIL_BULLET_EX;
static_assert(offsetof(RETAIL_BULLET_EX, turn_count) == 0x28);
static_assert(sizeof(RETAIL_BULLET_EX) == 0x2c);

// BulletManager::bullets[i], one enemy bullet.
typedef struct {
    uint8_t pad_0000[0xb8c];
    RETAIL_VEC3 pos;
    RETAIL_VEC3 velocity;
    uint8_t pad_0ba4[0x0c];
    float speed;
    uint8_t pad_0bb4[0x08];
    float angle;
    uint8_t pad_0bc0[0x08];
    RETAIL_TIMER timer;
    uint8_t pad_0bd4[0x20];
    uint16_t active_flags;
    uint16_t flags;
    uint8_t pad_0bf8[0x04];
    uint16_t state;
    uint8_t pad_0bfe[0x03];
    uint8_t grazed;
    uint8_t pad_0c02[0x0e];
    int32_t command_index;
    uint8_t pad_0c14[0x78];
    RETAIL_BULLET_EX ex[5];
} RETAIL_BULLET;
static_assert(offsetof(RETAIL_BULLET, pos) == 0xb8c);
static_assert(offsetof(RETAIL_BULLET, speed) == 0xbb0);
static_assert(offsetof(RETAIL_BULLET, angle) == 0xbbc);
static_assert(offsetof(RETAIL_BULLET, timer) == 0xbc8);
static_assert(offsetof(RETAIL_BULLET, active_flags) == 0xbf4);
static_assert(offsetof(RETAIL_BULLET, state) == 0xbfc);
static_assert(offsetof(RETAIL_BULLET, grazed) == 0xc01);
static_assert(offsetof(RETAIL_BULLET, command_index) == 0xc10);
static_assert(offsetof(RETAIL_BULLET, ex) == 0xc8c);
static_assert(sizeof(RETAIL_BULLET) == 0xd68);

// BulletManager::lasers[i], one enemy laser.
typedef struct {
    uint8_t pad_0000[0x498];
    RETAIL_VEC3 pos;
    float angle;
    float start_offset;
    float end_offset;
    uint8_t pad_04b0[0x04];
    float width;
    uint8_t pad_04b8[0x1c];
    int32_t active;
    RETAIL_TIMER timer;
    uint16_t flags;
    uint8_t pad_04e6[0x02];
    uint8_t state;
    uint8_t pad_04e9[0x03];
} RETAIL_LASER;
static_assert(offsetof(RETAIL_LASER, angle) == 0x4a4);
static_assert(offsetof(RETAIL_LASER, width) == 0x4b4);
static_assert(offsetof(RETAIL_LASER, active) == 0x4d4);
static_assert(offsetof(RETAIL_LASER, timer) == 0x4d8);
static_assert(offsetof(RETAIL_LASER, flags) == 0x4e4);
static_assert(offsetof(RETAIL_LASER, state) == 0x4e8);
static_assert(sizeof(RETAIL_LASER) == 0x4ec);

// g_BulletManager at 0x62f958. The last bullet is a spare that marks the
// end of the slots.
typedef struct {
    uint8_t pad_0000[0xb8c0];
    RETAIL_BULLET bullets[0x401];
    RETAIL_LASER lasers[0x40];
} RETAIL_BULLET_MANAGER;
static_assert(offsetof(RETAIL_BULLET_MANAGER, lasers) == 0x366628);
#define RETAIL_BULLET_MANAGER_AT ((RETAIL_BULLET_MANAGER *)0x0062f958)

// EnemyFlags::active, in the first byte of Enemy::flags.
#define RETAIL_ENEMY_FLAGS_ACTIVE 0x80

// EnemyManager::enemies[i], one enemy.
typedef struct {
    uint8_t pad_0000[0x6e8];
    RETAIL_TIMER script_timer;
    uint8_t pad_06f4[0x2418];
    RETAIL_VEC3 pos;
    uint8_t pad_2b18[0x3c];
    float angle;
    uint8_t pad_2b58[0x0c];
    float speed;
    uint8_t pad_2b68[0x50];
    int32_t life;
    uint8_t pad_2bbc[0x26c];
    uint8_t flags;
    uint8_t pad_2e29[0x211f];
} RETAIL_ENEMY;
static_assert(offsetof(RETAIL_ENEMY, script_timer) == 0x6e8);
static_assert(offsetof(RETAIL_ENEMY, pos) == 0x2b0c);
static_assert(offsetof(RETAIL_ENEMY, angle) == 0x2b54);
static_assert(offsetof(RETAIL_ENEMY, speed) == 0x2b64);
static_assert(offsetof(RETAIL_ENEMY, life) == 0x2bb8);
static_assert(offsetof(RETAIL_ENEMY, flags) == 0x2e28);
static_assert(sizeof(RETAIL_ENEMY) == 0x4f48);

// g_EnemyManager at 0x9a9b00.
typedef struct {
    uint8_t pad_0000[0x4f50];
    RETAIL_ENEMY enemies[0x1e1];
    uint8_t pad_954598[0x2c];
    int32_t spell_card_capture;
    int32_t spell_card_active;
    int32_t spell_card_bonus;
    uint8_t pad_9545d0[0x08];
    int32_t spell_card_index;
} RETAIL_ENEMY_MANAGER;
static_assert(offsetof(RETAIL_ENEMY_MANAGER, spell_card_capture) == 0x9545c4);
static_assert(offsetof(RETAIL_ENEMY_MANAGER, spell_card_index) == 0x9545d8);
#define RETAIL_ENEMY_MANAGER_AT ((RETAIL_ENEMY_MANAGER *)0x009a9b00)

// ItemManager::items[i], one item.
typedef struct {
    uint8_t pad_0000[0x24c];
    RETAIL_VEC3 pos;
    RETAIL_VEC3 velocity;
    uint8_t pad_0264[0x0c];
    RETAIL_TIMER timer;
    uint8_t type;
    uint8_t active;
    uint8_t pad_027e[0x01];
    uint8_t state;
    uint8_t pad_0280[0x08];
} RETAIL_ITEM;
static_assert(offsetof(RETAIL_ITEM, velocity) == 0x258);
static_assert(offsetof(RETAIL_ITEM, timer) == 0x270);
static_assert(offsetof(RETAIL_ITEM, type) == 0x27c);
static_assert(offsetof(RETAIL_ITEM, state) == 0x27f);
static_assert(sizeof(RETAIL_ITEM) == 0x288);

// g_ItemManager at 0x575c70. The last item is a spare that SpawnItem
// returns when the others are taken.
typedef struct {
    RETAIL_ITEM items[0x44d];
} RETAIL_ITEM_MANAGER;
#define RETAIL_ITEM_MANAGER_AT ((RETAIL_ITEM_MANAGER *)0x00575c70)
