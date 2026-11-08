#include "core/base/sha1.h"

#include <string.h>

static uint32_t M_RotateLeft(const uint32_t value, const int bits)
{
    return (value << bits) | (value >> (32 - bits));
}

static void M_ProcessBlock(uint32_t state[5], const uint8_t block[64])
{
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24)
            | ((uint32_t)block[i * 4 + 1] << 16)
            | ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
    }
    for (int i = 16; i < 80; i++) {
        w[i] = M_RotateLeft(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f;
        uint32_t k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5a827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdc;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6;
        }
        const uint32_t temp = M_RotateLeft(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = M_RotateLeft(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void Sha1_Init(SHA1 *const sha1)
{
    *sha1 = (SHA1) {
        .state = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 },
    };
}

void Sha1_Update(SHA1 *const sha1, const void *const data, size_t size)
{
    const uint8_t *bytes = data;
    sha1->size += size;
    while (size > 0) {
        size_t part = sizeof(sha1->block) - sha1->block_size;
        if (part > size) {
            part = size;
        }
        memcpy(sha1->block + sha1->block_size, bytes, part);
        sha1->block_size += part;
        bytes += part;
        size -= part;
        if (sha1->block_size == sizeof(sha1->block)) {
            M_ProcessBlock(sha1->state, sha1->block);
            sha1->block_size = 0;
        }
    }
}

SHA1_DIGEST Sha1_Final(SHA1 *const sha1)
{
    const uint64_t bits = sha1->size * 8;
    sha1->block[sha1->block_size++] = 0x80;
    if (sha1->block_size > 56) {
        memset(
            sha1->block + sha1->block_size, 0,
            sizeof(sha1->block) - sha1->block_size);
        M_ProcessBlock(sha1->state, sha1->block);
        sha1->block_size = 0;
    }
    memset(sha1->block + sha1->block_size, 0, 56 - sha1->block_size);
    for (int i = 0; i < 8; i++) {
        sha1->block[56 + i] = (uint8_t)(bits >> ((7 - i) * 8));
    }
    M_ProcessBlock(sha1->state, sha1->block);

    SHA1_DIGEST digest;
    for (int i = 0; i < 5; i++) {
        digest.bytes[i * 4] = (uint8_t)(sha1->state[i] >> 24);
        digest.bytes[i * 4 + 1] = (uint8_t)(sha1->state[i] >> 16);
        digest.bytes[i * 4 + 2] = (uint8_t)(sha1->state[i] >> 8);
        digest.bytes[i * 4 + 3] = (uint8_t)sha1->state[i];
    }
    return digest;
}

SHA1_DIGEST Sha1_Compute(const void *const data, const size_t size)
{
    SHA1 sha1;
    Sha1_Init(&sha1);
    Sha1_Update(&sha1, data, size);
    return Sha1_Final(&sha1);
}

void Sha1_ToHex(const SHA1_DIGEST digest, char out[SHA1_HEX_SIZE])
{
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < SHA1_DIGEST_SIZE; i++) {
        out[i * 2] = hex[digest.bytes[i] >> 4];
        out[i * 2 + 1] = hex[digest.bytes[i] & 0xf];
    }
    out[SHA1_DIGEST_SIZE * 2] = '\0';
}
