#include "core/hash/sha1.h"

#include "test.h"

#include <string.h>

static void M_CheckHex(const SHA1_DIGEST digest, const char *const expected)
{
    char hex[SHA1_HEX_SIZE];
    Sha1_ToHex(digest, hex);
    CHECK_EQ_STR(hex, expected);
}

TEST(compute_matches_the_fips_180_vectors)
{
    M_CheckHex(Sha1_Compute("", 0), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    M_CheckHex(
        Sha1_Compute("abc", 3), "a9993e364706816aba3e25717850c26c9cd0d89d");
    const char *const text =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    M_CheckHex(
        Sha1_Compute(text, strlen(text)),
        "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}

TEST(update_in_parts_matches_one_call)
{
    char a[1000];
    memset(a, 'a', sizeof(a));
    SHA1 sha1;
    Sha1_Init(&sha1);
    for (int i = 0; i < 1000; i++) {
        Sha1_Update(&sha1, a, sizeof(a));
    }
    M_CheckHex(Sha1_Final(&sha1), "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}

TEST(update_handles_every_split_of_a_block_boundary)
{
    uint8_t data[200];
    for (size_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)i;
    }
    const SHA1_DIGEST whole = Sha1_Compute(data, sizeof(data));
    for (size_t split = 0; split <= sizeof(data); split++) {
        SHA1 sha1;
        Sha1_Init(&sha1);
        Sha1_Update(&sha1, data, split);
        Sha1_Update(&sha1, data + split, sizeof(data) - split);
        const SHA1_DIGEST parts = Sha1_Final(&sha1);
        CHECK(memcmp(parts.bytes, whole.bytes, SHA1_DIGEST_SIZE) == 0);
    }
}
