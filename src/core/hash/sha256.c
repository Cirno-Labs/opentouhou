#include "core/hash/sha256.h"

#include <stdio.h>
#include <string.h>

#define M_BLOCK_SIZE 64

static const uint32_t m_RoundConstants[64] = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1,
    0x923F82A4, 0xAB1C5ED5, 0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174, 0xE49B69C1, 0xEFBE4786,
    0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147,
    0x06CA6351, 0x14292967, 0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85, 0xA2BFE8A1, 0xA81A664B,
    0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A,
    0x5B9CCA4F, 0x682E6FF3, 0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};

static uint32_t M_Rotate(const uint32_t value, const int bits)
{
    return (value >> bits) | (value << (32 - bits));
}

static void M_Compress(uint32_t state[8], const uint8_t block[M_BLOCK_SIZE])
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = (uint32_t)block[4 * i] << 24 | (uint32_t)block[4 * i + 1] << 16
            | (uint32_t)block[4 * i + 2] << 8 | (uint32_t)block[4 * i + 3];
    }
    for (int i = 16; i < 64; i++) {
        const uint32_t s0 =
            M_Rotate(w[i - 15], 7) ^ M_Rotate(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 =
            M_Rotate(w[i - 2], 17) ^ M_Rotate(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t v[8];
    memcpy(v, state, sizeof(v));
    for (int i = 0; i < 64; i++) {
        const uint32_t s1 =
            M_Rotate(v[4], 6) ^ M_Rotate(v[4], 11) ^ M_Rotate(v[4], 25);
        const uint32_t choice = (v[4] & v[5]) ^ (~v[4] & v[6]);
        const uint32_t t1 = v[7] + s1 + choice + m_RoundConstants[i] + w[i];
        const uint32_t s0 =
            M_Rotate(v[0], 2) ^ M_Rotate(v[0], 13) ^ M_Rotate(v[0], 22);
        const uint32_t majority = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
        const uint32_t t2 = s0 + majority;
        memmove(&v[1], &v[0], 7 * sizeof(v[0]));
        v[4] += t1;
        v[0] = t1 + t2;
    }
    for (int i = 0; i < 8; i++) {
        state[i] += v[i];
    }
}

void Sha256_Compute(
    const void *const data, const size_t size, uint8_t hash[SHA256_SIZE])
{
    uint32_t state[8] = {
        0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
        0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19,
    };
    const uint8_t *const bytes = data;
    size_t done = 0;
    for (; size - done >= M_BLOCK_SIZE; done += M_BLOCK_SIZE) {
        M_Compress(state, bytes + done);
    }
    uint8_t tail[2 * M_BLOCK_SIZE] = {};
    const size_t rest = size - done;
    if (rest > 0) {
        memcpy(tail, bytes + done, rest);
    }
    tail[rest] = 0x80;
    const size_t tail_size =
        rest < M_BLOCK_SIZE - 8 ? M_BLOCK_SIZE : 2 * M_BLOCK_SIZE;
    const uint64_t bits = (uint64_t)size * 8;
    for (int i = 0; i < 8; i++) {
        tail[tail_size - 1 - (size_t)i] = (uint8_t)(bits >> (8 * i));
    }
    for (size_t i = 0; i < tail_size; i += M_BLOCK_SIZE) {
        M_Compress(state, tail + i);
    }
    for (int i = 0; i < 8; i++) {
        hash[4 * i] = (uint8_t)(state[i] >> 24);
        hash[4 * i + 1] = (uint8_t)(state[i] >> 16);
        hash[4 * i + 2] = (uint8_t)(state[i] >> 8);
        hash[4 * i + 3] = (uint8_t)state[i];
    }
}

void Sha256_ComputeHex(
    const void *const data, const size_t size, char hex[SHA256_HEX_SIZE])
{
    uint8_t hash[SHA256_SIZE];
    Sha256_Compute(data, size, hash);
    for (int i = 0; i < SHA256_SIZE; i++) {
        snprintf(hex + 2 * i, 3, "%02x", hash[i]);
    }
}
