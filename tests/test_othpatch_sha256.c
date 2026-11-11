#include "othpatch/sha256.h"

#include "core/base/memory.h"

#include "test.h"

static void M_CheckHash(
    const void *const data, const size_t size, const char *const expected)
{
    char hex[SHA256_HEX_SIZE];
    Sha256_ComputeHex(data, size, hex);
    CHECK_EQ_STR(hex, expected);
}

TEST(empty_data_has_the_known_hash)
{
    M_CheckHash(
        "", 0,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(short_text_has_the_known_hash)
{
    M_CheckHash(
        "abc", 3,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(text_that_needs_a_second_padding_block_has_the_known_hash)
{
    const char *const text =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    M_CheckHash(
        text, strlen(text),
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(data_of_many_blocks_has_the_known_hash)
{
    const size_t size = 1000000;
    char *const data = Mem_Alloc(size);
    memset(data, 'a', size);
    M_CheckHash(
        data, size,
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    Mem_Free(data);
}
