#include "tests/test.h"
#include "tests/th02/th02_random_fixture.h"
#include "th02/random.h"

#include <string.h>

TEST(reset_and_direct_generation_match_master_lib)
{
    memset(p_Rng, 0xa5, sizeof(*p_Rng));
    Random_Reset();
    CHECK_EQ_INT(p_Rng->seed, 1);
    CHECK_EQ_INT(p_Rng->cursor, 0);
    for (size_t i = 0; i < RANDOM_RING_SIZE; i++) {
        CHECK_EQ_INT(p_Rng->ring[i], 0);
    }
    const uint16_t expected[] = { 346, 130, 10982, 1090, 11656, 7117 };
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        CHECK_EQ_INT(Random_U15(), expected[i]);
    }
    CHECK_EQ_INT(p_Rng->seed, UINT32_C(0x9bcd84bb));
    Random_Seed(0);
    CHECK_EQ_INT(Random_U15(), 0);
    CHECK_EQ_INT(p_Rng->seed, 1);
    Random_Seed(UINT32_MAX);
    CHECK_EQ_INT(Random_U15(), 32421);
    CHECK_EQ_INT(p_Rng->seed, UINT32_C(0xfea5b1cc));
    CHECK_EQ_INT(p_Rng->cursor, 0);
    CHECK_EQ_INT(p_Rng->ring[0], 0);
}

TEST(refill_advances_the_generator_but_preserves_the_cursor)
{
    Random_Reset();
    p_Rng->cursor = 173;
    Random_Fill();
    CHECK_EQ_INT(p_Rng->cursor, 173);
    CHECK_EQ_INT(p_Rng->ring[0], 0x5a);
    CHECK_EQ_INT(p_Rng->ring[1], 0x82);
    CHECK_EQ_INT(p_Rng->ring[2], 0xe6);
    CHECK_EQ_INT(p_Rng->ring[3], 0x42);
    const uint32_t first_seed = p_Rng->seed;
    uint8_t first_ring[RANDOM_RING_SIZE];
    memcpy(first_ring, p_Rng->ring, sizeof(first_ring));
    Random_Seed(1);
    CHECK(memcmp(first_ring, p_Rng->ring, sizeof(first_ring)) == 0);
    CHECK_EQ_INT(p_Rng->cursor, 173);
    Random_Fill();
    CHECK_EQ_INT(p_Rng->seed, first_seed);
    CHECK(memcmp(first_ring, p_Rng->ring, sizeof(first_ring)) == 0);
    Random_Fill();
    CHECK_EQ_INT(p_Rng->cursor, 173);
    CHECK(memcmp(first_ring, p_Rng->ring, sizeof(first_ring)) != 0);
}

TEST(word_reads_overlap_and_the_last_high_byte_is_the_wrapped_cursor)
{
    Random_Reset();
    p_Rng->ring[0] = 0xaa;
    p_Rng->ring[1] = 0xbb;
    p_Rng->ring[2] = 0xcc;
    p_Rng->ring[254] = 0xdd;
    p_Rng->ring[255] = 0xee;
    CHECK_EQ_INT(Random_RingU16(), 0xbbaa);
    CHECK_EQ_INT(Random_RingU16(), 0xccbb);
    CHECK_EQ_INT(p_Rng->cursor, 2);
    p_Rng->cursor = 254;
    CHECK_EQ_INT(Random_RingU16(), 0xeedd);
    CHECK_EQ_INT(Random_RingU16(), 0x00ee);
    CHECK_EQ_INT(p_Rng->cursor, 0);
    CHECK_EQ_INT(Random_RingU8And(0x0f), 0x0a);
    CHECK_EQ_INT(Random_RingU8(), 0xbb);
    CHECK_EQ_INT(p_Rng->cursor, 2);
    CHECK_EQ_INT(p_Rng->seed, 1);
}

TEST(range_operations_consume_one_byte_cursor_step)
{
    Random_Reset();
    memset(p_Rng->ring, 0xff, sizeof(p_Rng->ring));
    CHECK_EQ_INT(Random_RingU8Range(10, 18), 17);
    CHECK_EQ_INT(Random_RingU8Range(10, 20), 15);
    CHECK_EQ_INT(Random_RingU8Range(200, 200), 199);
    CHECK_EQ_INT(Random_RingU8Range(200, 100), 255);
    CHECK_EQ_INT(Random_RingU16Range(1000, 1008), 1007);
    CHECK_EQ_INT(Random_RingU16Range(1000, 1010), 1005);
    CHECK_EQ_INT(Random_RingU16Range(1000, 1000), 999);
    CHECK_EQ_INT(Random_RingU16Range(65530, 4), 65535);
    CHECK_EQ_INT(p_Rng->cursor, 8);
    p_Rng->cursor = 255;
    CHECK_EQ_INT(Random_RingU16Range(1000, 2000), 1255);
    CHECK_EQ_INT(p_Rng->cursor, 0);
    CHECK_EQ_INT(p_Rng->seed, 1);
}

TEST(reset_reproduces_the_complete_mixed_stream)
{
    uint16_t expected[3000];
    Random_Reset();
    Random_Fill();
    for (size_t i = 0; i < 1000; i++) {
        expected[i * 3] = Random_RingU16();
        expected[i * 3 + 1] = Random_RingU8And(31);
        expected[i * 3 + 2] = Random_U15();
    }
    CHECK_EQ_INT(p_Rng->cursor, 208);
    Random_Reset();
    Random_Fill();
    for (size_t i = 0; i < 1000; i++) {
        CHECK_EQ_INT(Random_RingU16(), expected[i * 3]);
        CHECK_EQ_INT(Random_RingU8And(31), expected[i * 3 + 1]);
        CHECK_EQ_INT(Random_U15(), expected[i * 3 + 2]);
    }
    CHECK_EQ_INT(p_Rng->cursor, 208);
}
