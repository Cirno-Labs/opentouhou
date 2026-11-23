#pragma once

#include <stddef.h>
#include <stdint.h>

// Keeps the replay files that playback reads, by path, so that a tick
// never opens a file.

// Takes data from the heap allocator in place of an earlier copy for the
// same path.
void ReplayFiles_Add(const char *path, uint8_t *data, size_t size);
void ReplayFiles_Remove(const char *path);
void ReplayFiles_Clear(void);

// Returns the bytes kept for path, or nullptr.
const uint8_t *ReplayFiles_Find(const char *path, size_t *size);
