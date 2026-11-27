#include "tests/test.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_player_graphics.h"
#include "tests/th02/th02_score_fixture.h"
#include "tests/th02/th02_sound.h"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/bullets.h"
#include "th02/bullets/private.h"
#include "th02/items.h"
#include "th02/performance.c"
#include "th02/player.h"
#include "th02/sparks.h"

#include <string.h>

TEST(initialization_and_position_reset_preserve_their_distinct_scopes)
{
    memset(&m_Player, 0xff, sizeof(m_Player));
    Player_Init();
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Shots.option_frames_per_age, 4);
    CHECK_EQ_INT(m_Shots.fire_cycle, 0);
    CHECK_EQ_INT(m_Shots.target.x, -1);
    CHECK_EQ_INT(m_Shots.target.y, -1);
    CHECK_EQ_INT(m_Shots.unused_target_x, -1);
    CHECK_EQ_INT(m_Bomb.active, 0);
    CHECK_EQ_INT(m_Bomb.stage_bombs, 0);
    CHECK_EQ_INT(m_Miss.frame, 0);
    CHECK_EQ_INT(m_Miss.stage_misses, 0);
    CHECK_EQ_INT(m_Miss.item_gameover, 0);
    CHECK_EQ_INT(m_Player.power, 1);
    CHECK_EQ_INT(m_Player.option_sprite, 72);
    CHECK_EQ_INT(m_Shots.weapon.parameter[0], 64);
    CHECK_EQ_INT(m_Shots.weapon.parameter[1], 76);
    CHECK_EQ_INT(m_Shots.weapon.cycle[0], 255);
    m_Player.current = (ENTITY_POINT) { 1, 2 };
    m_Player.option[1] = (ENTITY_POINT) { 3, 4 };
    m_Player.hit = 255;
    m_Shots.slots[0].age = 3;
    m_Shots.fire_cycle = 123;
    m_Bomb.stage_bombs = 2;
    m_Miss.item_gameover = 1;
    Player_ResetPosition();
    CHECK_EQ_INT(m_Shots.slots[0].age, 3);
    CHECK_EQ_INT(m_Shots.fire_cycle, 123);
    CHECK_EQ_INT(m_Bomb.stage_bombs, 2);
    CHECK_EQ_INT(m_Miss.item_gameover, 1);
    CHECK_EQ_INT(m_Player.position[0].x, 208);
    CHECK_EQ_INT(m_Player.position[1].y, 336);
    CHECK_EQ_INT(m_Player.current.x, 1);
    CHECK_EQ_INT(m_Player.option[1].y, 4);
    CHECK_EQ_INT(m_Player.hit, 255);
    m_Player.position[1] = (ENTITY_POINT) { INT16_MAX, INT16_MIN };
    Player_Move(1, 1, -1);
    CHECK_EQ_INT(m_Player.current.x, 28);
    CHECK_EQ_INT(m_Player.current.y, 344);
    CHECK_EQ_INT(m_Player.position[0].x, 208);
}

TEST(keypad_priority_page_speed_and_clamp_effects_keep_option_positions)
{
    Player_Init();
    const PLAYFIELD_PAGES pages = { 1, 0, 0 };
    m_Player.position[1] = (ENTITY_POINT) { 28, 16 };
    m_Player.speed_x = m_Player.speed_y = 5;
    m_Player.diagonal_x = m_Player.diagonal_y = 4;
    Player_Controls(pages, GAME_KEY_UP_LEFT | GAME_KEY_DOWN | GAME_KEY_RIGHT);
    CHECK_EQ_INT(m_Player.current.x, 28);
    CHECK_EQ_INT(m_Player.current.y, 16);
    CHECK_EQ_INT(m_Player.sprite, 2);
    CHECK_EQ_INT(m_Player.option[1].x, 15);
    CHECK_EQ_INT(m_Player.option[1].y, 35);
    Player_Controls(
        pages, GAME_KEY_UP_LEFT | GAME_KEY_DOWN_RIGHT | GAME_KEY_RIGHT);
    CHECK_EQ_INT(m_Player.current.x, 32);
    CHECK_EQ_INT(m_Player.sprite, 3);
    CHECK_EQ_INT(m_Player.option[1].x, 12);
    Player_Controls(pages, GAME_KEY_UP | GAME_KEY_DOWN | GAME_KEY_LEFT);
    CHECK_EQ_INT(m_Player.current.x, 32);
    CHECK_EQ_INT(m_Player.sprite, 0);
    CHECK_EQ_INT(m_Player.option[1].x, 16);
}

TEST(firing_emits_real_shots_before_bomb_and_skips_the_second_cycle)
{
    TEST_SOUND sound;
    TestSound_Init(&sound);
    Player_Init();
    Player_ResetPosition();
    m_Shots.weapon.cycle[1] = 0;
    Tram_Init();
    Score_Init();
    m_Score.bombs = 3;
    memset(&m_Performance, 0, sizeof(m_Performance));
    Bullets_Init();
    memset(p_Background, 0, sizeof(*p_Background));
    const PLAYFIELD_PAGES pages = { 0, 1, 0 };
    p_Bullets->slots[0] = (BULLET) {
        .state = ENTITY_ACTIVE,
        .kind = BULLET_PELLET,
        .position = { { 100 * 16, 100 * 16 }, { 120 * 16, 120 * 16 } },
    };
    Player_Controls(pages, GAME_KEY_SHOT | GAME_KEY_BOMB);
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Shots.slots[0].position[0].x, 216 * 16);
    CHECK_EQ_INT(m_Shots.slots[0].sprite, 0x34);
    CHECK_EQ_INT(m_Shots.slots[1].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Shots.weapon.parameter[0], 0x34);
    TestSound_Check(&sound, -1);
    SoundEffects_Update();
    TestSound_Check(&sound, 9);
    CHECK_EQ_INT(m_Bomb.active, 1);
    CHECK_EQ_INT(m_Player.bomb_invincible, 1);
    CHECK_EQ_INT(m_Bomb.stage_bombs, 1);
    CHECK_EQ_INT(m_Score.bombs, 2);
    CHECK_EQ_INT(m_Performance.total_bombs, 1);
    CHECK_EQ_INT(g_Bullets->slots[0].state, ENTITY_REMOVE);
    CHECK_EQ_INT(g_Bullets->slots[0].position[0].x, 120 * 16);
    CHECK_EQ_INT(p_Background->cache.columns[4], 1);
    CHECK_EQ_INT(m_Shots.weapon.cooldown[0], 8);
    CHECK_EQ_INT(m_Shots.weapon.cycle[0], 1);
    CHECK_EQ_INT(m_Shots.weapon.cooldown[1], 0);
    m_Shots.weapon.cycle[0] = 1;
    m_Shots.weapon.cooldown[0] = 3;
    m_Shots.weapon.cycle[1] = m_Shots.weapon.cooldown[1] = 0;
    m_Shots.weapon.level = 9;
    m_Shots.weapon.spread = 127;
    Player_Controls(pages, GAME_KEY_DOWN);
    CHECK_EQ_INT(m_Shots.slots[1].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Shots.slots[1].sprite, 0);
    CHECK_EQ_INT(m_Shots.slots[2].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Shots.slots[2].sprite, 0);
    CHECK_EQ_INT(m_Shots.slots[3].sprite, 52);
    CHECK_EQ_INT(m_Shots.slots[5].sprite, 54);
    CHECK_EQ_INT(m_Shots.weapon.parameter[0], 54);
    CHECK_EQ_INT(m_Shots.weapon.spread, -124);
    CHECK_EQ_INT(m_Shots.weapon.cycle[1], 1);
    Player_Controls(pages, 0);
    SoundEffects_Update();
    TestSound_Check(&sound, -1);
    TestSound_Free(&sound);
    CHECK_EQ_INT(m_Shots.weapon.cooldown[1], 7);
}

TEST(cleanup_marks_old_footprints_before_copying_only_the_player_position)
{
    TEST_PLAYER_GRAPHICS graphics;
    TestPlayerGraphics_Init(&graphics);
    Player_Init();
    m_Player.position[0] = (ENTITY_POINT) { 48, 32 };
    m_Player.position[1] = (ENTITY_POINT) { 144, 128 };
    m_Player.option[0] = (ENTITY_POINT) { 32760, -16 };
    m_Player.current = (ENTITY_POINT) { 5, 6 };
    m_Shots.slots[0] = (SHOT) {
        .state = ENTITY_REMOVE,
        .position = { { 96 * 16, 128 * 16 }, { 200 * 16, 200 * 16 } },
        .sprite = 52,
    };
    Player_Invalidate((PLAYFIELD_PAGES) { 0, 1, 0 }, false);
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_FREE);
    const TILE_CACHE expected = {
        .columns = { [1] = 1, [2] = 1, [4] = 1 },
        .dirty = { [1][2] = 1,
                   [1][3] = 1,
                   [1][4] = 1,
                   [2][2] = 1,
                   [2][3] = 1,
                   [2][4] = 1,
                   [4][8] = 1 },
    };
    CHECK(memcmp(&p_Background->cache, &expected, sizeof(expected)) == 0);
    CHECK_EQ_INT(m_Player.position[0].x, 144);
    CHECK_EQ_INT(m_Player.option[0].x, 32760);
    CHECK_EQ_INT(m_Player.current.x, 5);
    TestPlayerGraphics_Free(&graphics);
}

TEST(update_uses_post_decrement_blinking_and_gameover_skips_normal_drawing)
{
    TEST_PLAYER_GRAPHICS graphics;
    TestPlayerGraphics_Init(&graphics);
    Player_Init();
    m_Player.current = (ENTITY_POINT) { 200, 352 };
    m_Player.position[0] = m_Player.position[1] = m_Player.current;
    m_Player.option[0] = (ENTITY_POINT) { 184, 368 };
    m_Player.invincibility = 4;
    m_Player.hit = 1;
    m_Player.power = 8;
    m_Shots.slots[0] = (SHOT) {
        .state = ENTITY_ACTIVE,
        .position = { { 100 * 16, 100 * 16 } },
        .velocity = { 16, -16 },
        .sprite = 52,
    };
    Score_Init();
    memset(&m_Performance, 0, sizeof(m_Performance));
    Sparks_Init();
    Items_Init();
    CHECK(!Player_Update(
        (PLAYFIELD_PAGES) { 0, 0, 399 }, 0, false, &graphics.patterns, 0));
    CHECK_EQ_INT(m_Player.hit, 0);
    CHECK_EQ_INT(m_Player.invincibility, 3);
    CHECK_EQ_INT(m_Shots.slots[0].position[0].x, 101 * 16);
    CHECK_EQ_INT(g_Video.pages[0][98 * VIDEO_WIDTH + 101], 9);
    CHECK_EQ_INT(g_Video.pages[0][367 * VIDEO_WIDTH + 184], 11);
    CHECK_EQ_INT(g_Video.pages[0][367 * VIDEO_WIDTH + 232], 11);
    CHECK_EQ_INT(g_Video.pages[0][351 * VIDEO_WIDTH + 200], 0);
    CHECK(!Player_Update(
        (PLAYFIELD_PAGES) { 0, 0, 399 }, 0, false, &graphics.patterns, 0));
    CHECK_EQ_INT(g_Video.pages[0][351 * VIDEO_WIDTH + 200], 1);
    m_Player.invincibility = 0;
    m_Player.hit = m_Miss.active = 1;
    m_Miss.frame = 23;
    CHECK(Player_Update(
        (PLAYFIELD_PAGES) { 0, 0, 399 }, 0, false, &graphics.patterns, 0));
    CHECK_EQ_INT(m_Player.hit, 0);
    CHECK_EQ_INT(m_Miss.frame, 0);
    CHECK_EQ_INT(g_Sparks->next, 32);
    CHECK_EQ_INT(g_Items->slots[0].kind, ITEM_BIG_POWER);
    CHECK_EQ_INT(g_Items->slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Shots.slots[0].position[0].x, 103 * 16);
    TestPlayerGraphics_Free(&graphics);
}

TEST(continuous_controls_keep_motion_across_page_swaps_and_clamps)
{
    TEST_PLAYER_GRAPHICS graphics;
    TestPlayerGraphics_Init(&graphics);
    Player_Init();
    Player_ResetPosition();
    m_Player.speed_x = m_Player.speed_y = 5;
    m_Player.diagonal_x = m_Player.diagonal_y = 3;
    const uint16_t keys[] = {
        GAME_KEY_UP_RIGHT, GAME_KEY_DOWN_LEFT, GAME_KEY_RIGHT,
        GAME_KEY_DOWN,     GAME_KEY_LEFT,      GAME_KEY_UP,
    };
    const ENTITY_POINT checkpoints[] = {
        { 368, 176 }, { 208, 336 }, { 390, 336 },
        { 390, 344 }, { 28, 344 },  { 28, 16 },
    };
    for (size_t leg = 0; leg < ARRAY_SIZE(keys); leg++) {
        for (uint16_t frame = 0; frame < 64 + (leg >= 2) * 64; frame++) {
            const uint8_t back = frame & 1;
            const uint8_t front = back ^ 1;
            const ENTITY_POINT previous = m_Player.position[front];
            const PLAYFIELD_PAGES pages = { back, front,
                                            (int16_t)(frame * 7 % 400) };
            Player_Invalidate(pages, false);
            Player_Controls(pages, keys[leg]);
            CHECK_EQ_INT(m_Player.position[front].x, previous.x);
            CHECK_EQ_INT(m_Player.position[front].y, previous.y);
            CHECK_EQ_INT(m_Player.position[back].x, m_Player.current.x);
            CHECK_EQ_INT(m_Player.position[back].y, m_Player.current.y);
            CHECK(!Player_Update(pages, frame, false, &graphics.patterns, 0));
        }
        CHECK_EQ_INT(m_Player.current.x, checkpoints[leg].x);
        CHECK_EQ_INT(m_Player.current.y, checkpoints[leg].y);
    }
    TestPlayerGraphics_Free(&graphics);
}
