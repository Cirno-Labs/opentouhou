// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/types.h"

// Computes the CRC-32 of zlib, PNG and thcrap's files.js (the reflected
// polynomial 0xEDB88320), in one call or over data given in parts.

// Returns the CRC-32 of size bytes of data that follow data with the
// CRC-32 crc. A crc of 0 starts over no data, so Crc32_Update(0, ...)
// equals Crc32_Compute.
uint32_t Crc32_Update(uint32_t crc, const void *data, size_t size);

// Returns the CRC-32 of size bytes of data.
uint32_t Crc32_Compute(const void *data, size_t size);
