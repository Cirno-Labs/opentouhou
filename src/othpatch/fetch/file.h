#pragma once

#include "core/base/result.h"
#include "core/base/types.h"

// Reads the file that a file:// URL names, so that the tests need no
// server. The host must be empty or "localhost", percent escapes are
// decoded, and a query or fragment after '?' or '#' is ignored. Mem_Free
// releases *data. On failure, *data is nullptr and *size is 0.
RESULT FetchFile_Read(const char *url, uint8_t **data, size_t *size);
