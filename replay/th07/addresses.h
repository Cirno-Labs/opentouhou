#pragma once

#include <stddef.h>
#include <stdint.h>

// Describes retail 1.00b. It has no relocations, so addresses are absolute.

// Patch sites, each followed by the value it holds.
#define RETAIL_RUN_CALC_CHAIN_CALL 0x004347df
#define RETAIL_RUN_CALC_CHAIN 0x0042fd60
#define RETAIL_PLAY_ON_UPDATE_2_SITE 0x00443c8d
#define RETAIL_PLAY_ON_UPDATE_2 0x00442e50
#define RETAIL_MAIN_MENU_ON_UPDATE_SITE 0x0045c633
#define RETAIL_MAIN_MENU_ON_UPDATE 0x004554d6
// DrawFps's call of AddString with the frame rate, before the one with
// the replay's speed.
#define RETAIL_ADD_FPS_CALL 0x0043938c

// Restarts the update chain within the frame; returned by PlayOnUpdate2.
#define RETAIL_CHAIN_RESTART 6

// Functions, each followed by its size.
#define RETAIL_APP_ON_UPDATE 0x00437c70
#define RETAIL_APP_ON_UPDATE_SIZE 1707
#define RETAIL_RUN_FRAME 0x004346e0
#define RETAIL_RUN_FRAME_SIZE 855
#define RETAIL_MEASURE_REFRESH_RATE 0x004386f3
#define RETAIL_MEASURE_REFRESH_RATE_SIZE 659

// ReadButtons, a fastcall without arguments, which App::OnUpdate calls
// twice.
#define RETAIL_READ_BUTTONS 0x00430b50
// AsciiText::AddString, a thiscall of (pos, text).
#define RETAIL_ADD_STRING 0x00401f40

#define RETAIL_SCENE_TITLE 1
#define RETAIL_SCENE_GAMEPLAY 2

// MainMenu states and the title menu's sub-state once it takes input.
#define RETAIL_MAIN_MENU_TITLE 0
#define RETAIL_MAIN_MENU_TITLE_READY 1
#define RETAIL_MAIN_MENU_REPLAYS 7

// The buttons the game plays by, g_PlayButtons.
#define RETAIL_PLAY_BUTTONS (*(uint16_t *)0x004b9e50)

// g_Rng at 0x49fe20.
typedef struct {
    uint16_t seed;
    uint8_t pad_02[0x02];
    int32_t generated;
} RETAIL_RNG;
static_assert(offsetof(RETAIL_RNG, generated) == 0x04);
#define RETAIL_RNG_AT ((RETAIL_RNG *)0x0049fe20)

typedef struct {
    float x;
    float y;
    float z;
} RETAIL_VEC3;

typedef struct {
    int32_t previous;
    float sub_frame;
    int32_t current;
} RETAIL_TIMER;
static_assert(sizeof(RETAIL_TIMER) == 0x0c);

// What GameManager::stats points at. The counts are floats.
typedef struct {
    uint32_t gui_score;
    uint32_t score;
    uint8_t pad_0008[0x0c];
    int32_t graze;
    uint8_t pad_0018[0x0c];
    int32_t point_items;
    uint8_t pad_0028[0x08];
    // The point items of the next extend.
    int32_t next_extend_items;
    uint8_t pad_0034[0x28];
    float lives;
    uint8_t pad_0060[0x08];
    float bombs;
    uint8_t pad_006c[0x10];
    float power;
    uint8_t pad_0080[0x08];
    // Added to the cherry counters against tampering.
    int32_t cherry_offset;
} RETAIL_GAME_STATS;
static_assert(offsetof(RETAIL_GAME_STATS, graze) == 0x14);
static_assert(offsetof(RETAIL_GAME_STATS, point_items) == 0x24);
static_assert(offsetof(RETAIL_GAME_STATS, next_extend_items) == 0x30);
static_assert(offsetof(RETAIL_GAME_STATS, lives) == 0x5c);
static_assert(offsetof(RETAIL_GAME_STATS, bombs) == 0x68);
static_assert(offsetof(RETAIL_GAME_STATS, power) == 0x7c);
static_assert(offsetof(RETAIL_GAME_STATS, cherry_offset) == 0x88);

// g_GameManager at 0x626270.
typedef struct {
    uint8_t pad_0000[0x08];
    RETAIL_GAME_STATS *stats;
    uint8_t pad_000c[0x04];
    int32_t difficulty;
    uint8_t pad_0014[0x93c1];
    uint8_t character;
    uint8_t shot_type;
    uint8_t pad_93d7[0x211];
    int32_t stage_frames;
    int32_t stage;
    uint8_t pad_95f0[0x28];
    int32_t cherry_max;
    int32_t cherry;
    int32_t cherry_plus;
    uint8_t pad_9624[0x10];
    int32_t rank;
    uint8_t pad_9638[0x08];
    int32_t sub_rank;
} RETAIL_GAME_MANAGER;
static_assert(offsetof(RETAIL_GAME_MANAGER, difficulty) == 0x10);
static_assert(offsetof(RETAIL_GAME_MANAGER, character) == 0x93d5);
static_assert(offsetof(RETAIL_GAME_MANAGER, stage_frames) == 0x95e8);
static_assert(offsetof(RETAIL_GAME_MANAGER, cherry_max) == 0x9618);
static_assert(offsetof(RETAIL_GAME_MANAGER, rank) == 0x9634);
static_assert(offsetof(RETAIL_GAME_MANAGER, sub_rank) == 0x9640);
#define RETAIL_GAME_MANAGER_AT ((RETAIL_GAME_MANAGER *)0x00626270)

// g_App at 0x575950.
typedef struct {
    uint8_t pad_0000[0x154];
    int32_t cur_scene;
    uint8_t pad_0158[0x0c];
    // Counts the frames left that draw the whole screen.
    int32_t redraw_frames;
    uint8_t pad_0168[0x10];
    float game_speed;
} RETAIL_APP;
static_assert(offsetof(RETAIL_APP, cur_scene) == 0x154);
static_assert(offsetof(RETAIL_APP, redraw_frames) == 0x164);
static_assert(offsetof(RETAIL_APP, game_speed) == 0x178);
#define RETAIL_APP_AT ((RETAIL_APP *)0x00575950)

// The redraw frames that the interface sets as a scene starts.
#define RETAIL_REDRAW_FRAMES 2

// The MainMenu that SetUpTitleScene allocates.
typedef struct {
    uint8_t pad_0000[0x08];
    int32_t sub_state;
    uint8_t pad_000c[0xd0ec];
    int32_t state;
} RETAIL_MAIN_MENU;
static_assert(offsetof(RETAIL_MAIN_MENU, state) == 0xd0f8);

// Player::bullets[i], a shot of the player's own fire.
typedef struct {
    uint8_t pad_0000[0x24c];
    RETAIL_VEC3 pos;
    uint8_t pad_0258[0xcc];
    RETAIL_VEC3 velocity;
    uint8_t pad_0330[0x18];
    int16_t damage;
    int16_t state;
    uint8_t pad_034c[0x18];
} RETAIL_PLAYER_BULLET;
static_assert(offsetof(RETAIL_PLAYER_BULLET, velocity) == 0x324);
static_assert(offsetof(RETAIL_PLAYER_BULLET, damage) == 0x348);
static_assert(offsetof(RETAIL_PLAYER_BULLET, state) == 0x34a);
static_assert(sizeof(RETAIL_PLAYER_BULLET) == 0x364);

// g_Player at 0x4bdad8.
typedef struct {
    uint8_t pad_0000[0x930];
    RETAIL_VEC3 position;
    uint8_t pad_093c[0x1acc];
    int8_t player_state;
    uint8_t pad_2409[0x04];
    // Player::bombState: 1 while the Border runs, 2 while it waits.
    int8_t border_state;
    uint8_t pad_240e[0x36];
    RETAIL_PLAYER_BULLET bullets[0x60];
} RETAIL_PLAYER;
static_assert(offsetof(RETAIL_PLAYER, player_state) == 0x2408);
static_assert(offsetof(RETAIL_PLAYER, border_state) == 0x240d);
static_assert(offsetof(RETAIL_PLAYER, bullets) == 0x2444);
#define RETAIL_PLAYER_AT ((RETAIL_PLAYER *)0x004bdad8)

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
    uint8_t pad_0bd4[0x22];
    uint16_t flags;
    uint8_t pad_0bf8[0x04];
    uint16_t state;
    uint8_t pad_0bfe[0x03];
    uint8_t grazed;
    uint8_t pad_0c02[0x166];
} RETAIL_BULLET;
static_assert(offsetof(RETAIL_BULLET, pos) == 0xb8c);
static_assert(offsetof(RETAIL_BULLET, speed) == 0xbb0);
static_assert(offsetof(RETAIL_BULLET, angle) == 0xbbc);
static_assert(offsetof(RETAIL_BULLET, timer) == 0xbc8);
static_assert(offsetof(RETAIL_BULLET, flags) == 0xbf6);
static_assert(offsetof(RETAIL_BULLET, state) == 0xbfc);
static_assert(offsetof(RETAIL_BULLET, grazed) == 0xc01);
static_assert(sizeof(RETAIL_BULLET) == 0xd68);

// BulletManager::lasers[i], one enemy laser.
typedef struct {
    uint8_t pad_0000[0x498];
    RETAIL_VEC3 pos;
    float angle;
    float start_offset;
    float end_offset;
    uint8_t pad_04b0[0x24];
    int32_t active;
    RETAIL_TIMER timer;
    uint8_t pad_04e4[0x04];
    uint8_t state;
    uint8_t pad_04e9[0x03];
} RETAIL_LASER;
static_assert(offsetof(RETAIL_LASER, angle) == 0x4a4);
static_assert(offsetof(RETAIL_LASER, active) == 0x4d4);
static_assert(offsetof(RETAIL_LASER, timer) == 0x4d8);
static_assert(offsetof(RETAIL_LASER, state) == 0x4e8);
static_assert(sizeof(RETAIL_LASER) == 0x4ec);

// g_BulletManager at 0x62f958. The last bullet is a spare outside the
// slots.
typedef struct {
    uint8_t pad_0000[0xb8c0];
    RETAIL_BULLET bullets[0x401];
    RETAIL_LASER lasers[0x40];
} RETAIL_BULLET_MANAGER;
static_assert(offsetof(RETAIL_BULLET_MANAGER, lasers) == 0x366628);
#define RETAIL_BULLET_MANAGER_AT ((RETAIL_BULLET_MANAGER *)0x0062f958)
#define RETAIL_BULLET_COUNT 0x400

// EnemyFlags::active, in the first byte of Enemy::flags.
#define RETAIL_ENEMY_FLAGS_ACTIVE 0x80

// EnemyManager::enemies[i], one enemy.
typedef struct {
    uint8_t pad_0000[0x2b0c];
    RETAIL_VEC3 pos;
    uint8_t pad_2b18[0xa0];
    int32_t life;
    uint8_t pad_2bbc[0x26c];
    uint8_t flags;
    uint8_t pad_2e29[0x211f];
} RETAIL_ENEMY;
static_assert(offsetof(RETAIL_ENEMY, pos) == 0x2b0c);
static_assert(offsetof(RETAIL_ENEMY, life) == 0x2bb8);
static_assert(offsetof(RETAIL_ENEMY, flags) == 0x2e28);
static_assert(sizeof(RETAIL_ENEMY) == 0x4f48);

// g_EnemyManager at 0x9a9b00. The last enemy is a spare outside the
// slots.
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
#define RETAIL_ENEMY_COUNT 0x1e0

// ItemManager::items[i], one item.
typedef struct {
    uint8_t pad_0000[0x24c];
    RETAIL_VEC3 pos;
    uint8_t pad_0258[0x24];
    uint8_t type;
    uint8_t active;
    uint8_t pad_027e[0x01];
    uint8_t state;
    uint8_t pad_0280[0x08];
} RETAIL_ITEM;
static_assert(offsetof(RETAIL_ITEM, type) == 0x27c);
static_assert(offsetof(RETAIL_ITEM, state) == 0x27f);
static_assert(sizeof(RETAIL_ITEM) == 0x288);

// g_ItemManager at 0x575c70. The last item is a spare that SpawnItem
// returns when the others are taken.
typedef struct {
    RETAIL_ITEM items[0x44d];
} RETAIL_ITEM_MANAGER;
#define RETAIL_ITEM_MANAGER_AT ((RETAIL_ITEM_MANAGER *)0x00575c70)
#define RETAIL_ITEM_COUNT 0x44c

// The text that DrawFps formats the frame rate into.
#define RETAIL_FPS_TEXT ((const char *)0x0135e0f0)
