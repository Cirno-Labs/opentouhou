#pragma once

#include "core/vfs.h"

typedef enum {
    PF_TH01, // key 0x76
} PF_GAME;

// Opens a PF archive as a read-only VFS source that owns the archive.
// Names are complemented CP932, and lookups ignore ASCII case. Contents
// are XORed with the game's key and may be packed with master.lib's RLE.
RESULT Pf_Open(
    READER *archive, PF_GAME game, const char *name, const ALLOCATOR *allocator,
    VFS_SOURCE **out);
