#pragma once

#include "core/vfs.h"

// Reads ZUN's PF archive format, the th01-era precursor to PBG3, as a
// VFS source.

typedef enum {
    PF_TH01, // key 0x76
    PF_KEY_12, // key 0x12, tables and lookup as PF_TH01
} PF_GAME;

// Opens a PF archive as a read-only VFS source that owns the archive.
// Names are complemented CP932, and lookups ignore ASCII case. Contents
// are XORed with the game's key and may be packed with master.lib's RLE.
RESULT Pf_Open(
    READER *archive, PF_GAME game, const char *name, const ALLOCATOR *allocator,
    VFS_SOURCE **out);
