#pragma once

#include "core/base/types.h"

// Computes SHA-1 digests, in one call or over data given in parts.

#define SHA1_DIGEST_SIZE 20

// Sets the size of a digest in hex: 40 lowercase digits and a NUL.
#define SHA1_HEX_SIZE 41

typedef struct {
    uint8_t bytes[SHA1_DIGEST_SIZE];
} SHA1_DIGEST;

// Holds the state of a digest over data given in parts. The fields are
// private to the Sha1_ functions.
typedef struct {
    uint32_t state[5];
    uint64_t size;
    uint8_t block[64];
    size_t block_size;
} SHA1;

// Starts a digest over no data.
void Sha1_Init(SHA1 *sha1);

// Adds size bytes of data to the digest.
void Sha1_Update(SHA1 *sha1, const void *data, size_t size);

// Returns the digest of all the data added since Sha1_Init. The state is
// then used up; Sha1_Init starts a new digest.
SHA1_DIGEST Sha1_Final(SHA1 *sha1);

// Returns the digest of size bytes of data.
SHA1_DIGEST Sha1_Compute(const void *data, size_t size);

// Writes the digest as lowercase hex.
void Sha1_ToHex(SHA1_DIGEST digest, char out[SHA1_HEX_SIZE]);
