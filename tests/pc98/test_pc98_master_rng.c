#include "tests/test.h"

#include "pc98/zun/master_rng.h"

TEST(random_matches_the_fixed_sequence_and_wrapping_state)
{
    static const uint32_t states[] = { 0x015a4e36, 0x8082a52f, 0xaae684bc,
                                       0x0442c2ed, 0xad889112, 0x9bcd84bb,
                                       0x44bb74b8, 0x190f3a19, 0x59a4a52e,
                                       0xf9963687, 0x232c6bf4, 0xb8deb185 };
    static const uint16_t values[] = {
        346,   130,  10982, 1090,  11656, 7117,
        17595, 6415, 22948, 31126, 9004,  14558
    };
    uint32_t state = 1;
    for (size_t i = 0; i < sizeof(states) / sizeof(*states); i++) {
        CHECK_EQ_INT(MasterRng_Next(&state), values[i]);
        CHECK_EQ_INT(state, states[i]);
    }
    state = 0;
    CHECK_EQ_INT(MasterRng_Next(&state), 0);
    CHECK_EQ_INT(state, 1);
    state = UINT32_MAX;
    CHECK_EQ_INT(MasterRng_Next(&state), 32421);
    CHECK_EQ_INT(state, UINT32_C(0xfea5b1cc));
}

TEST(random_state_is_independent_and_can_be_restored)
{
    uint32_t first = 1;
    uint32_t second = 0;
    uint32_t restored = 1;
    for (int32_t i = 0; i < 10000; i++) {
        const uint16_t next = MasterRng_Next(&first);
        CHECK_EQ_INT(next, MasterRng_Next(&restored));
        CHECK(next <= 32767);
    }
    CHECK_EQ_INT(first, restored);
    CHECK_EQ_INT(second, 0);
    CHECK_EQ_INT(MasterRng_Next(&second), 0);
    CHECK_EQ_INT(MasterRng_Next(&second), 346);
}
