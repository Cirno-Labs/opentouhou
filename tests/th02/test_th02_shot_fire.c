#include "tests/test.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_player_graphics.h"
#include "tests/th02/th02_random_fixture.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/shots.h"

#include <string.h>

static void M_Init(const uint8_t type, const uint8_t level)
{
    memset(&m_Rng, 0, sizeof(m_Rng));
    Player_Init();
    m_Player.shot_type = type;
    Random_Reset();
    m_Player.current = (ENTITY_POINT) { 100, 100 };
    m_Player.option[0] = (ENTITY_POINT) { 80, 116 };
    m_Shots.weapon.level = level;
    m_Shots.weapon.parameter[0] = 48;
    m_Shots.weapon.parameter[1] = 55;
}

TEST(stage_target_reset_keeps_the_type_c_cycle)
{
    Shots_Init();
    CHECK_EQ_INT(m_Shots.target.x, -1);
    CHECK_EQ_INT(m_Shots.target.y, -1);
    CHECK_EQ_INT(m_Shots.unused_target_x, -1);
    m_Shots.fire_cycle = 255;
    m_Shots.target = (ENTITY_POINT) { 20, 30 };
    Shots_ResetTarget();
    CHECK_EQ_INT(m_Shots.fire_cycle, 255);
    CHECK_EQ_INT(m_Shots.target.x, -1);
    CHECK_EQ_INT(m_Shots.target.y, -1);
}

TEST(type_a_uses_free_slots_without_resetting_front_positions_or_frames)
{
    M_Init(0, 9);
    for (size_t i = 0; i < SHOT_COUNT; i++) {
        m_Shots.slots[i].state = ENTITY_REMOVE;
    }
    m_Shots.slots[2].state = m_Shots.slots[30].state = ENTITY_FREE;
    m_Shots.slots[2].position[1] = (ENTITY_POINT) { 123, -456 };
    m_Shots.frames[2] = 255;
    m_Shots.option_active = 7;
    M_Emit(0);
    CHECK_EQ_INT(m_Shots.slots[2].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Shots.slots[30].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Shots.slots[2].position[0].x, 1600);
    CHECK_EQ_INT(m_Shots.slots[30].position[0].x, 1856);
    CHECK_EQ_INT(m_Shots.slots[2].position[1].x, 123);
    CHECK_EQ_INT(m_Shots.slots[2].position[1].y, -456);
    CHECK_EQ_INT(m_Shots.frames[2], 255);
    CHECK_EQ_INT(m_Shots.weapon.parameter[0], 48);
    CHECK_EQ_INT(m_Shots.option_active, 0);
    CHECK_EQ_INT(m_Rng.cursor, 0);
}

TEST(type_b_level_eight_uses_the_higher_option_velocities)
{
    M_Init(1, 8);
    M_Emit(0);
    CHECK_EQ_INT(m_Rng.cursor, 2);
    CHECK_EQ_INT(m_Shots.slots[4].velocity.y, -54);
    CHECK_EQ_INT(m_Shots.slots[6].velocity.x, -16);
    CHECK_EQ_INT(m_Shots.slots[6].velocity.y, -50);
    CHECK_EQ_INT(m_Shots.slots[8].velocity.x, -32);
    CHECK_EQ_INT(m_Shots.slots[8].velocity.y, -40);
    CHECK_EQ_INT(m_Shots.slots[9].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Shots.slots[10].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Shots.option_active, 1);
}

TEST(partial_options_do_not_latch_and_no_option_slot_keeps_the_old_target)
{
    M_Init(1, 2);
    for (size_t i = 1; i < SHOT_COUNT; i++) {
        m_Shots.slots[i].state = ENTITY_ACTIVE;
    }
    m_Shots.unused_target_x = 77;
    M_Emit(0);
    CHECK_EQ_INT(m_Shots.unused_target_x, 77);
    CHECK_EQ_INT(m_Shots.option_active, 0);
    m_Shots.slots[0].state = m_Shots.slots[37].state = ENTITY_FREE;
    M_Emit(0);
    CHECK_EQ_INT(m_Shots.unused_target_x, -1);
    CHECK_EQ_INT(m_Shots.slots[37].from_option, 1);
    CHECK_EQ_INT(m_Shots.slots[37].age, 1);
    CHECK_EQ_INT(m_Shots.slots[37].position[0].x, 80 * 16);
    CHECK_EQ_INT(m_Shots.option_active, 0);
}

TEST(aimed_type_b_preserves_zero_vector_angle_and_ring_wrap)
{
    M_Init(1, 9);
    m_Shots.target = (ENTITY_POINT) { 100, 132 };
    m_Rng.cursor = 255;
    m_Rng.ring[255] = m_Rng.ring[0] = 3;
    m_Shots.option_active = 1;
    M_Emit(0);
    CHECK_EQ_INT(m_Shots.slots[2].velocity.x, 192);
    CHECK_EQ_INT(m_Shots.slots[2].velocity.y, 0);
    CHECK_EQ_INT(m_Shots.slots[3].velocity.x, -192);
    CHECK_EQ_INT(m_Shots.slots[3].velocity.y, 0);
    CHECK_EQ_INT(m_Rng.cursor, 1);
    CHECK_EQ_INT(m_Shots.weapon.parameter[0], 50);
    CHECK_EQ_INT(m_Shots.unused_target_x, 100);
    CHECK_EQ_INT(m_Shots.slots[4].state, ENTITY_FREE);
}

TEST(type_c_advances_with_a_full_pool_and_wrap_selects_the_six_option_volley)
{
    M_Init(2, 9);
    for (size_t i = 0; i < SHOT_COUNT; i++) {
        m_Shots.slots[i].state = ENTITY_ACTIVE;
    }
    m_Shots.fire_cycle = 255;
    M_Emit(0);
    CHECK_EQ_INT(m_Shots.fire_cycle, 0);
    CHECK_EQ_INT(m_Shots.weapon.parameter[0], 124);
    Shots_FreeAll();
    m_Shots.fire_cycle = 255;
    M_Emit(0);
    CHECK_EQ_INT(m_Shots.slots[0].sprite, 124);
    CHECK_EQ_INT(m_Shots.slots[1].sprite, 49);
    CHECK_EQ_INT(m_Shots.slots[3].position[0].y, 116 * 16);
    CHECK_EQ_INT(m_Shots.slots[5].from_option, 1);
    CHECK_EQ_INT(m_Shots.slots[5].velocity.y, -160);
    CHECK_EQ_INT(m_Shots.slots[10].velocity.y, -120);
    CHECK_EQ_INT(m_Shots.slots[11].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Shots.option_active, 1);
    CHECK_EQ_INT(m_Shots.weapon.parameter[0], 49);
}

TEST(aimed_body_vectors_accumulate_after_alternating_page_cleanup)
{
    TEST_PLAYER_GRAPHICS graphics;
    TestPlayerGraphics_Init(&graphics);
    M_Init(1, 9);
    m_Shots.target = (ENTITY_POINT) { 100, 132 };
    m_Shots.option_active = 1;
    M_Emit(0);
    for (uint8_t frame = 0; frame < 9; frame++) {
        const uint8_t back = frame & 1;
        const uint8_t front = back ^ 1;
        if (frame != 0) {
            Shots_Invalidate(
                (PLAYFIELD_PAGES) { back, front, (int16_t)(frame * 97 % 400) },
                false);
        }
        Shots_Update(
            (PLAYFIELD_PAGES) { back, front, (int16_t)(frame * 97 % 400) },
            false, frame, &graphics.patterns);
        CHECK_EQ_INT(
            m_Shots.slots[2].position[back].x, 100 * 16 + (frame + 1) * 191);
        CHECK_EQ_INT(
            m_Shots.slots[3].position[back].x, (116 - (frame + 1) * 12) * 16);
        CHECK_EQ_INT(
            m_Shots.slots[2].position[back].y, 132 * 16 - (frame + 1) * 15);
        CHECK_EQ_INT(
            m_Shots.slots[3].position[back].y, 132 * 16 + (frame + 1) * 14);
        CHECK_EQ_INT(m_Shots.slots[2].state, ENTITY_ACTIVE);
        CHECK_EQ_INT(
            m_Shots.slots[3].state, frame == 8 ? ENTITY_REMOVE : ENTITY_ACTIVE);
    }
    Shots_Invalidate((PLAYFIELD_PAGES) { 1, 0, 0 }, false);
    CHECK_EQ_INT(m_Shots.slots[3].state, ENTITY_FREE);
    TestPlayerGraphics_Free(&graphics);
}

static uint64_t M_HashValue(uint64_t hash, const uint32_t value)
{
    for (size_t byte = 0; byte < 4; byte++) {
        hash ^= (value >> (byte * 8)) & 255;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t M_HashVolley(uint64_t hash)
{
    const int32_t fields[] = {
        m_Shots.weapon.parameter[0],
        m_Shots.weapon.parameter[1],
        m_Shots.weapon.cycle[0],
        m_Shots.weapon.cycle[1],
        m_Shots.weapon.cooldown[0],
        m_Shots.weapon.cooldown[1],
        m_Shots.fire_cycle,
        m_Shots.target.x,
        m_Shots.target.y,
        m_Shots.unused_target_x,
        m_Shots.option_active,
        m_Shots.option_frames_per_age,
        m_Rng.cursor,
    };
    for (size_t i = 0; i < ARRAY_SIZE(fields); i++) {
        hash = M_HashValue(hash, (uint32_t)fields[i]);
    }
    hash = M_HashValue(hash, m_Rng.seed);
    for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
        hash = M_HashValue(hash, m_Rng.ring[i]);
    }
    for (size_t i = 0; i < SHOT_COUNT; i++) {
        const SHOT *const shot = &m_Shots.slots[i];
        const int32_t values[] = {
            shot->state,         shot->age,           shot->position[0].x,
            shot->position[0].y, shot->position[1].x, shot->position[1].y,
            shot->velocity.x,    shot->velocity.y,    shot->sprite,
            shot->from_option,   m_Shots.frames[i],
        };
        for (size_t j = 0; j < ARRAY_SIZE(values); j++) {
            hash = M_HashValue(hash, (uint32_t)values[j]);
        }
    }
    return hash;
}

TEST(all_shot_types_keep_partial_volley_state_and_random_consumption)
{
    static const uint8_t cycles[] = { 0, 254, 255 };
    static const uint64_t expected[] = {
        UINT64_C(14766980882693354605),
        UINT64_C(16179374348838834049),
        UINT64_C(5578610974768516269),
    };
    for (uint8_t type = 0; type < 3; type++) {
        uint64_t hash = UINT64_C(14695981039346656037);
        for (uint8_t level = 0; level < 10; level++) {
            for (uint8_t back = 0; back < 2; back++) {
                for (uint8_t layout = 0; layout < 4; layout++) {
                    for (size_t cycle = 0; cycle < ARRAY_SIZE(cycles);
                         cycle++) {
                        for (uint8_t aimed = 0; aimed < 2; aimed++) {
                            M_Init(type, level);
                            Random_Seed(0x12345678);
                            Random_Fill();
                            m_Rng.cursor = 255;
                            m_Shots.weapon.spread = layout == 0 ? -124 : 26;
                            m_Shots.fire_cycle = cycles[cycle];
                            m_Shots.unused_target_x = 77;
                            m_Shots.target = aimed != 0
                                ? (ENTITY_POINT) { 188, 240 }
                                : (ENTITY_POINT) { -1, -1 };
                            m_Shots.option_active = layout & 1;
                            for (size_t i = 0; i < SHOT_COUNT; i++) {
                                const bool free = layout == 0
                                    || (layout == 1 && i == SHOT_COUNT - 1)
                                    || (layout == 2 && i % 3 == 1);
                                m_Shots.slots[i] = (SHOT) {
                                    .state = free ? ENTITY_FREE : ENTITY_ACTIVE,
                                    .age = (uint8_t)(i % 6),
                                    .position = { { -123, 456 },
                                                  { 789, -987 } },
                                    .velocity = { -17, 31 },
                                    .sprite = 53,
                                    .from_option = (uint8_t)(i & 1),
                                };
                                m_Shots.frames[i] = (uint8_t)(255 - i * 7);
                            }
                            M_Emit(back);
                            hash = M_HashVolley(hash);
                            M_Emit(back ^ 1);
                            hash = M_HashVolley(hash);
                        }
                    }
                }
            }
        }
        CHECK(hash == expected[type]);
    }
}
