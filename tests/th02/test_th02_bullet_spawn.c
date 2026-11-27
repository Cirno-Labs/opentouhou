#include "tests/test.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_random_fixture.h"
#include "th02/bullets.h"
#include "th02/bullets/private.h"
#include "th02/bullets/spawn.c"
#include "th02/performance_private.h"
#include "th02/player.h"
#include "th02/random.h"

#include <string.h>

TEST(spawn_checks_keep_order_and_easy_slow_cycle)
{
    Bullets_Init();
    Player_Init();
    p_Performance->value = 0;
    m_Spawn.slow_cycle = 255;
    BulletSpawn_Init(0);
    CHECK_EQ_INT(m_Spawn.slow_cycle, 255);
    int16_t speed = 16;
    CHECK(!BulletSpawn_Tune(&speed));
    CHECK_EQ_INT(m_Spawn.slow_cycle, 0);
    CHECK_EQ_INT(speed, 16);
    CHECK(BulletSpawn_Tune(&speed));
    CHECK_EQ_INT(m_Spawn.slow_cycle, 1);
    CHECK_EQ_INT(speed, 6);
    CHECK(BulletSpawn_Skip(23, 10, false, &speed));
    m_Bomb.active = 1;
    CHECK(BulletSpawn_Skip(24, 10, false, &speed));
    m_Bomb.active = 0;
    CHECK(!BulletSpawn_Skip(24, 10, true, &speed));
    CHECK_EQ_INT(m_Spawn.slow_cycle, 1);
    CHECK_EQ_INT(speed, 6);
    BulletSpawn_Init(1);
    speed = 256;
    p_Performance->value = 256;
    CHECK(!BulletSpawn_Tune(&speed));
    CHECK_EQ_INT(speed, 256);
    speed = 100;
    p_Performance->value = -6;
    CHECK(!BulletSpawn_Tune(&speed));
    CHECK_EQ_INT(speed, 88);
}

TEST(spawn_bounds_and_layers_preserve_edges_and_partial_stops)
{
    CHECK(!BulletSpawn_Outside(24, 0));
    CHECK(!BulletSpawn_Outside(416, 383));
    CHECK(BulletSpawn_Outside(23, 0));
    CHECK(BulletSpawn_Outside(417, 383));
    CHECK(BulletSpawn_Outside(24, -1));
    CHECK(BulletSpawn_Outside(24, 384));
    Bullets_Init();
    BulletSpawn_Init(3);
    BulletSpawn_Layers(2);
    int16_t layer = 0;
    int16_t index = 17;
    int16_t speed = 33;
    CHECK(BulletSpawn_NextLayer(&layer, &index, &speed));
    CHECK_EQ_INT(layer, 1);
    CHECK_EQ_INT(index, 0);
    CHECK_EQ_INT(speed, 24);
    CHECK(BulletSpawn_NextLayer(&layer, &index, &speed));
    CHECK_EQ_INT(layer, 2);
    CHECK_EQ_INT(speed, 20);
    index = 7;
    CHECK(!BulletSpawn_NextLayer(&layer, &index, &speed));
    CHECK_EQ_INT(speed, 20);
    CHECK_EQ_INT(index, 7);
    layer = 0;
    speed = -3;
    CHECK(!BulletSpawn_NextLayer(&layer, &index, &speed));
    CHECK_EQ_INT(speed, 6);
    CHECK_EQ_INT(layer, 0);
    CHECK_EQ_INT(index, 7);
}

TEST(pattern_directions_preserve_order_mirroring_and_unknown_codes)
{
    const uint8_t angles[] = { 31, 1, 61, 227 };
    for (int16_t i = 0; i < 4; i++) {
        const BULLET_DIRECTION direction =
            BulletSpawn_Direction(BULLET_FAN4_WIDE_AIMED, i, 16, 48);
        CHECK_EQ_INT(direction.angle, angles[i]);
        CHECK(direction.aimed);
        CHECK(direction.last == (i == 3));
        CHECK_EQ_INT(direction.next_index, i + 1);
    }
    BULLET_DIRECTION direction =
        BulletSpawn_Direction(BULLET_MIRROR, 1, 30, 48);
    CHECK_EQ_INT(direction.angle, 98);
    CHECK(!direction.aimed);
    CHECK(direction.last);
    direction = BulletSpawn_Direction(BULLET_RING32, 31, 8, 48);
    CHECK_EQ_INT(direction.angle, 0);
    CHECK(direction.last);
    direction = BulletSpawn_Direction(BULLET_FAN3_WIDE, 4, 0, 48);
    CHECK_EQ_INT(direction.angle, 16);
    CHECK(!direction.last);
    direction = BulletSpawn_Direction(0x1234, INT16_MAX, 32, 48);
    CHECK(direction.aimed);
    CHECK(!direction.last);
    CHECK_EQ_INT(direction.angle, 32);
    CHECK_EQ_INT(direction.next_index, INT16_MIN);
}

TEST(random_direction_consumes_angle_then_speed_across_cursor_wrap)
{
    memset(p_Rng, 0, sizeof(*p_Rng));
    p_Rng->seed = 123;
    p_Rng->cursor = 255;
    p_Rng->ring[255] = 32;
    p_Rng->ring[0] = 255;
    const BULLET_DIRECTION direction =
        BulletSpawn_Direction(BULLET_RANDOM_SPEED, 250, 250, INT16_MAX);
    CHECK_EQ_INT(direction.angle, 26);
    CHECK_EQ_INT(direction.speed, -32738);
    CHECK_EQ_INT(direction.next_index, 251);
    CHECK(direction.last);
    CHECK(!direction.aimed);
    CHECK_EQ_INT(p_Rng->cursor, 1);
    CHECK_EQ_INT(p_Rng->seed, 123);
}
