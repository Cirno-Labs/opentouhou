#include "tests/test.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/shots.h"
#include "th02/sparks.h"
#include "th02/sparks_private.h"

static SHOT M_Shot(const uint8_t sprite, const uint8_t option)
{
    return (SHOT) {
        .state = ENTITY_ACTIVE,
        .position = { { 100 * 16 + 7, 100 * 16 + 9 }, { 200, 300 } },
        .velocity = { -1, -9 },
        .sprite = sprite,
        .from_option = option,
    };
}

// Hits like the game's callers, which add the damage to the score.
static int16_t M_Hit(
    const SHOT_HIT_INPUT input, const int16_t left, const int16_t top,
    const int16_t width, const int16_t height)
{
    const int16_t damage = Shots_Hit(input, left, top, width, height);
    Score_AddDamage(damage);
    return damage;
}

TEST(body_hits_slow_velocity_shift_large_shots_and_keep_score_high_word)
{
    Player_Init();
    Sparks_Init();
    p_Sparks->next = 63;
    Score_Init();
    p_Score->value = 1234;
    p_Score->delta = 0x1234fffe;
    p_Shots->weapon.level = 2;
    const SHOT_HIT_INPUT input = {};
    p_Shots->slots[0] = M_Shot(50, 0);
    p_Shots->slots[1] = M_Shot(120, 0);
    CHECK_EQ_INT(M_Hit(input, 96, 96, 32, 32), 14);
    CHECK_EQ_INT(p_Score->delta, 0x1234000c);
    CHECK_EQ_INT(p_Score->value, 1234);
    CHECK_EQ_INT(p_Shots->slots[0].age, 2);
    CHECK_EQ_INT(p_Shots->slots[1].age, 1);
    CHECK_EQ_INT(p_Shots->slots[0].velocity.x, -1);
    CHECK_EQ_INT(p_Shots->slots[0].velocity.y, -2);
    CHECK_EQ_INT(p_Shots->slots[1].position[0].x, 92 * 16 + 7);
    CHECK_EQ_INT(p_Shots->slots[1].position[0].y, 92 * 16 + 9);
    CHECK_EQ_INT(p_Shots->slots[1].position[1].x, 200);
    CHECK_EQ_INT(p_Shots->slots[1].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(p_Sparks->next, 1);
    CHECK_EQ_INT(p_Sparks->slots[63].position[0].x, 100 * 16);
    CHECK_EQ_INT(p_Sparks->slots[0].position[0].x, 100 * 16);
    CHECK_EQ_INT(p_Sparks->slots[0].position[0].y, 92 * 16);
    CHECK_EQ_INT(p_Sparks->slots[0].velocity.x, 30);
    CHECK_EQ_INT(M_Hit(input, 96, 96, 32, 32), 0);
}

TEST(options_use_the_narrower_right_edge_and_keep_damaging_after_hit_animation)
{
    Player_Init();
    Sparks_Init();
    Score_Init();
    p_Shots->weapon.level = 0;
    const SHOT_HIT_INPUT input = {};
    p_Shots->slots[0] = M_Shot(0, 1);
    p_Shots->slots[0].age = 1;
    p_Shots->slots[1] = M_Shot(0, 255);
    p_Shots->slots[1].age = 255;
    p_Shots->slots[2] = M_Shot(49, 0);
    p_Shots->slots[2].position[0].x = 120 * 16;
    p_Shots->slots[3] = M_Shot(49, 1);
    p_Shots->slots[3].position[0].x = 120 * 16;
    CHECK_EQ_INT(M_Hit(input, 96, 96, 32, 32), 5);
    CHECK_EQ_INT(p_Shots->slots[0].age, 2);
    CHECK_EQ_INT(p_Shots->slots[1].age, 255);
    CHECK_EQ_INT(p_Shots->slots[2].age, 0);
    CHECK_EQ_INT(p_Shots->slots[3].age, 0);
    CHECK_EQ_INT(p_Sparks->next, 2);
    CHECK_EQ_INT(p_Sparks->slots[0].velocity.x, 40);
    CHECK_EQ_INT(M_Hit(input, 96, 96, 32, 32), 5);
    CHECK_EQ_INT(p_Sparks->next, 3);
}

TEST(strict_edges_last_three_slots_and_bomb_mask_preserve_their_distinct_rules)
{
    Player_Init();
    Sparks_Init();
    Score_Init();
    p_Score->delta = INT32_MAX;
    p_Shots->weapon.level = 9;
    p_Bomb->active = 7;
    SHOT_HIT_INPUT input = { .bomb_frame_mask = 1 };
    p_Shots->slots[0] = M_Shot(49, 0);
    p_Shots->slots[0].position[0].x = 88 * 16;
    p_Shots->slots[1] = M_Shot(49, 0);
    p_Shots->slots[1].position[0].y = 120 * 16;
    p_Shots->slots[35] = p_Shots->slots[36] = p_Shots->slots[37] =
        M_Shot(120, 0);
    CHECK_EQ_INT(M_Hit(input, 96, 96, 32, 32), 1);
    CHECK_EQ_INT(p_Score->delta, 0x7fff0000);
    CHECK_EQ_INT(p_Shots->slots[35].age, 0);
    CHECK_EQ_INT(p_Sparks->next, 0);
    input.frame = 0xffffff01u;
    CHECK_EQ_INT(M_Hit(input, 96, 96, 32, 32), 0);
    input.bomb_frame_mask = 0;
    CHECK_EQ_INT(M_Hit(input, 96, 96, 32, 32), 1);
    p_Bomb->active = 0;
    p_Shots->slots[34] = M_Shot(49, 0);
    CHECK_EQ_INT(M_Hit(input, 96, 96, 32, 32), 1);
    CHECK_EQ_INT(p_Sparks->next, 1);
}

static uint64_t M_HashValue(uint64_t hash, const uint32_t value)
{
    for (size_t byte = 0; byte < 4; byte++) {
        hash ^= (value >> (byte * 8)) & 255;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t M_HashState(uint64_t hash)
{
    hash = M_HashValue(hash, (uint32_t)p_Score->value);
    hash = M_HashValue(hash, (uint32_t)p_Score->delta);
    hash = M_HashValue(hash, p_Sparks->next);
    hash = M_HashValue(hash, p_Rng->seed);
    hash = M_HashValue(hash, p_Rng->cursor);
    for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
        hash = M_HashValue(hash, p_Rng->ring[i]);
    }
    for (size_t i = 0; i < SHOT_COUNT; i++) {
        const SHOT *const shot = &p_Shots->slots[i];
        const int32_t values[] = {
            shot->state,         shot->age,           shot->position[0].x,
            shot->position[0].y, shot->position[1].x, shot->position[1].y,
            shot->velocity.x,    shot->velocity.y,    shot->sprite,
            shot->from_option,   p_Shots->frames[i],
        };
        for (size_t j = 0; j < ARRAY_SIZE(values); j++) {
            hash = M_HashValue(hash, (uint32_t)values[j]);
        }
    }
    for (size_t i = 0; i < SPARK_COUNT; i++) {
        const SPARK *const spark = &p_Sparks->slots[i];
        const int32_t values[] = {
            spark->state,         spark->age,           spark->position[0].x,
            spark->position[0].y, spark->position[1].x, spark->position[1].y,
            spark->velocity.x,    spark->velocity.y,    spark->drawing,
            spark->reserved_1,    spark->angle,         spark->base_speed,
            spark->reserved_2,    spark->preferred,
        };
        for (size_t j = 0; j < ARRAY_SIZE(values); j++) {
            hash = M_HashValue(hash, (uint32_t)values[j]);
        }
    }
    return hash;
}

TEST(hit_levels_pages_masks_and_bounds_preserve_state_and_rng)
{
    static const uint32_t frames[] = { 0, 1, 0xffffff00u, 0xffffff01u };
    static const uint8_t masks[] = { 0, 1, 3 };
    static const uint8_t sprites[] = { 49, 50, 119, 120, 124 };
    static const int16_t bounds[][4] = {
        { 96, 96, 32, 32 },
        { 96, 96, 24, 24 },
        { INT16_MIN + 8, 96, INT16_MIN, 32 },
    };
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint8_t level = 0; level < 10; level++) {
        for (uint8_t back = 0; back < 2; back++) {
            for (uint8_t bombing = 0; bombing < 2; bombing++) {
                for (size_t frame = 0; frame < ARRAY_SIZE(frames); frame++) {
                    for (size_t mask = 0; mask < ARRAY_SIZE(masks); mask++) {
                        for (size_t box = 0; box < ARRAY_SIZE(bounds); box++) {
                            Player_Init();
                            Sparks_Init();
                            Score_Init();
                            Random_Reset();
                            Random_Seed(0x23456789);
                            p_Rng->cursor = 255;
                            for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
                                p_Rng->ring[i] = (uint8_t)(i ^ 0x5a);
                            }
                            p_Shots->weapon.level = level;
                            p_Bomb->active = bombing != 0 ? 7 : 0;
                            p_Score->value = 1234;
                            p_Score->delta = 0x1234ffff;
                            p_Sparks->next = 63;
                            for (size_t i = 0; i < SPARK_COUNT; i++) {
                                p_Sparks->slots[i].angle = (uint8_t)(i * 9);
                                p_Sparks->slots[i].base_speed =
                                    (uint8_t)(13 + i);
                                p_Sparks->slots[i].preferred =
                                    i % 2 != 0 ? 14 : 0;
                            }
                            for (size_t i = 0; i < SHOT_COUNT; i++) {
                                SHOT *const shot = &p_Shots->slots[i];
                                *shot = M_Shot(
                                    sprites[i % ARRAY_SIZE(sprites)],
                                    (uint8_t)(i % 3 == 0));
                                shot->state =
                                    i % 7 == 0 ? ENTITY_REMOVE : ENTITY_ACTIVE;
                                shot->age = (uint8_t)(i % 4);
                                shot->position[0] = (ENTITY_POINT) {
                                    (int16_t)((80 + i % 7 * 8) * 16 + 7),
                                    (int16_t)((80 + i % 5 * 8) * 16 + 9),
                                };
                                shot->position[1] = (ENTITY_POINT) {
                                    (int16_t)(shot->position[0].x + 24 * 16),
                                    (int16_t)(shot->position[0].y + 16 * 16),
                                };
                                shot->velocity =
                                    (ENTITY_POINT) { -32767, INT16_MIN + 3 };
                            }
                            const SHOT_HIT_INPUT input = {
                                .back = back,
                                .frame = frames[frame],
                                .bomb_frame_mask = masks[mask],
                            };
                            for (size_t repeat = 0; repeat < 2; repeat++) {
                                const int16_t damage = M_Hit(
                                    input, bounds[box][0], bounds[box][1],
                                    bounds[box][2], bounds[box][3]);
                                hash = M_HashValue(hash, (uint32_t)damage);
                                hash = M_HashState(hash);
                            }
                        }
                    }
                }
            }
        }
    }
    CHECK(hash == UINT64_C(2304759412348653157));
}
