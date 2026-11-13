#pragma once

#include "core/base/result.h"
#include "core/base/types.h"

// Describes how core/patch gets the files of a repository network. core
// holds no network code: it calls the PATCH_FETCH_FN that it was given.

// Holds one URL to fetch and, after a PATCH_FETCH_FN, its outcome.
typedef struct {
    const char *url;
    // Holds the contents. Mem_Free releases data. On failure, data is
    // nullptr and size is 0.
    uint8_t *data;
    size_t size;
    // Holds the outcome, which must be consumed, such as by
    // PatchFetch_Release.
    RESULT result;
} PATCH_FETCH;

// Reports the progress of a PATCH_FETCH_FN for each fetch. received comes
// as bytes arrive, with the bytes of fetch that have arrived and the size
// that the server announced, which is 0 while it is unknown. ended comes
// when a fetch ends with success, with its contents in place. Either may
// be nullptr.
typedef struct {
    void (*received)(
        void *context, const PATCH_FETCH *fetch, size_t received,
        size_t expected);
    void (*ended)(void *context, const PATCH_FETCH *fetch);
    void *context;
} PATCH_FETCH_PROGRESS;

// Fetches every URL of fetches, and sets the contents and the result of
// each. progress may be nullptr.
typedef void (*PATCH_FETCH_FN)(
    PATCH_FETCH *fetches, int32_t count, const PATCH_FETCH_PROGRESS *progress);

// Releases the contents of a fetch and consumes its result.
void PatchFetch_Release(PATCH_FETCH *fetch);
