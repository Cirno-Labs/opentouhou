#pragma once

#include "core/base/result.h"
#include "core/base/types.h"

// Reads the file that a file:// URL names, as fetch.h describes. Mem_Free
// releases *data. On failure, *data is nullptr and *size is 0.
RESULT FetchFile_Read(const char *url, uint8_t **data, size_t *size);
