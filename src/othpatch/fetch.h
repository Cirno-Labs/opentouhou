#pragma once

// Fetches the contents of URLs into memory, several at a time, for
// core/patch and for the release checks: http:// and https:// through
// curl, file:// read locally so that the tests need no server.

#include "othpatch/fetch/curl.h"
#include "othpatch/fetch/file.h"
