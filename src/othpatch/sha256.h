#pragma once

#include "core/base/types.h"

// Computes SHA-256 hashes, which the release index gives for each
// package.

// Sets the size of a hash in bytes.
#define SHA256_SIZE 32

// Sets the size of a hash as lowercase hexadecimal text, NUL included.
#define SHA256_HEX_SIZE (2 * SHA256_SIZE + 1)

// Writes the SHA-256 hash of size bytes of data into hash.
void Sha256_Compute(const void *data, size_t size, uint8_t hash[SHA256_SIZE]);

// Writes the SHA-256 hash of size bytes of data into hex as lowercase
// hexadecimal text.
void Sha256_ComputeHex(
    const void *data, size_t size, char hex[SHA256_HEX_SIZE]);
