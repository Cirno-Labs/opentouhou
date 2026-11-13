// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/image/pixels.h"
#include "core/io/reader.h"

// Reads the rest of reader and decodes it. The format comes from the data,
// not from the file name.
RESULT Image_Decode(READER *reader, const ALLOCATOR *allocator, IMAGE *out);
