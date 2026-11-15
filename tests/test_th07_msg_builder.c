#include "th07_msg_builder.h"

#include "test.h"

#include <string.h>

TEST(build_lays_out_the_count_the_offsets_and_the_scripts)
{
    MSG_BUILDER b;
    MsgBuilder_Init(&b);
    const int32_t first = MsgBuilder_BeginScript(&b);
    const int32_t second = MsgBuilder_BeginScript(&b);
    MsgBuilder_OpWho(&b, first, 3, 1, 1, -2);
    MsgBuilder_Op(&b, first, 4, 0);
    MsgBuilder_OpInt(&b, second, 0x102, 4, 0x01020304);
    MsgBuilder_OpByte(&b, second, 0, 13, 7);
    static uint8_t out[MSG_BUILDER_MAX_SIZE];
    const size_t size = MsgBuilder_Build(&b, out);
    // clang-format off
    const uint8_t expected[] = {
        2, 0, 0, 0, 12, 0, 0, 0, 24, 0, 0, 0,
        3, 0, 1, 4, 1, 0, 0xfe, 0xff,
        4, 0, 0, 0,
        0x02, 0x01, 4, 4, 0x04, 0x03, 0x02, 0x01,
        0, 0, 13, 4, 7, 0, 0, 0,
    };
    // clang-format on
    CHECK_EQ_INT(size, sizeof(expected));
    CHECK(memcmp(out, expected, sizeof(expected)) == 0);
}

TEST(text_is_cp932_with_its_nul)
{
    MSG_BUILDER b;
    MsgBuilder_Init(&b);
    const int32_t script = MsgBuilder_BeginScript(&b);
    MsgBuilder_OpText(
        &b, script, 0, 3, 0, 1,
        "\xe5\xa6\x96"
        "a");
    static uint8_t out[MSG_BUILDER_MAX_SIZE];
    const size_t size = MsgBuilder_Build(&b, out);
    // clang-format off
    const uint8_t expected[] = {
        1, 0, 0, 0, 8, 0, 0, 0,
        0, 0, 3, 8, 0, 0, 1, 0, 0x97, 0x64, 'a', 0,
    };
    // clang-format on
    CHECK_EQ_INT(size, sizeof(expected));
    CHECK(memcmp(out, expected, sizeof(expected)) == 0);
}

TEST(set_offset_replaces_the_stored_offset)
{
    MSG_BUILDER b;
    MsgBuilder_Init(&b);
    const int32_t script = MsgBuilder_BeginScript(&b);
    MsgBuilder_Op(&b, script, 0, 0);
    MsgBuilder_SetOffset(&b, script, 0x1000);
    static uint8_t out[MSG_BUILDER_MAX_SIZE];
    CHECK_EQ_INT(MsgBuilder_Build(&b, out), 12);
    CHECK_EQ_INT(out[4], 0);
    CHECK_EQ_INT(out[5], 0x10);
}
