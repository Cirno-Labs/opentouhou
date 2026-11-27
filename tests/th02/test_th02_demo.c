#include "tests/test.h"
#include "th02/demo.h"

#include "core/io/reader.h"

#include <string.h>

TEST(recording_is_owned_and_requires_exact_word_count)
{
    uint8_t bytes[DEMO_INPUT_COUNT * 2] = {};
    bytes[0] = 0x34;
    bytes[1] = 0x12;
    bytes[sizeof(bytes) - 1] = 0x80;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    DEMO_RECORD record;
    CHECK(Result_Ignore(Demo_Read(&reader, &record)));
    Reader_Close(&reader);
    memset(bytes, 0, sizeof(bytes));
    CHECK_EQ_INT(record.keys[0], 0x1234);
    CHECK_EQ_INT(record.keys[DEMO_INPUT_COUNT - 1], 0x8000);
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes) - 1, nullptr);
    CHECK(!Result_Ignore(Demo_Read(&reader, &record)));
    Reader_Close(&reader);
    CHECK_EQ_INT(record.keys[0], 0);
    CHECK_EQ_INT(record.keys[DEMO_INPUT_COUNT - 1], 0);
}

TEST(playback_clears_the_last_sample_and_live_input_without_overreading)
{
    DEMO_RECORD record = {};
    record.keys[0] = 0x1020;
    record.keys[DEMO_PLAY_COUNT - 1] = 0xffff;
    DEMO demo;
    Demo_Begin(&demo, &record);
    uint16_t keys = 0;
    CHECK(Demo_Input(&demo, &keys));
    CHECK_EQ_INT(keys, 0x1020);
    keys = 0x80;
    CHECK(!Demo_Input(&demo, &keys));
    CHECK_EQ_INT(keys, 0);
    CHECK_EQ_INT(demo.frame, 1);
    CHECK(!Demo_Input(&demo, &keys));
    CHECK_EQ_INT(demo.frame, 1);
    Demo_Begin(&demo, &record);
    for (unsigned i = 0; i < DEMO_PLAY_COUNT; i++) {
        keys = 0;
        CHECK_EQ_INT(Demo_Input(&demo, &keys), i + 1 < DEMO_PLAY_COUNT);
    }
    CHECK_EQ_INT(keys, 0);
    CHECK_EQ_INT(demo.frame, DEMO_PLAY_COUNT);
}

TEST(demo_session_selects_the_original_stage_shot_and_seed_frame)
{
    for (int i = 1; i <= 3; i++) {
        TH02_SESSION session = {
            .demo_num = i, .rank = 1, .score = 987, .start_lives = 2
        };
        Demo_ConfigureSession(&session);
        CHECK_EQ_INT(session.stage, 4 - i);
        CHECK_EQ_INT(session.shot_type, i == 1 ? 0 : i == 2 ? 2 : 1);
        CHECK_EQ_INT(session.frame, 18);
        CHECK_EQ_INT(session.score, 987);
        CHECK_EQ_INT(session.start_lives, 2);
    }
}
