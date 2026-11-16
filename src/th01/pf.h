#pragma once

#include "core/vfs.h"

// Opens TH01's PAR archive as a VFS source, matching VFS_OPENER. Takes
// ownership of archive and closes it on failure or when the source closes.
// Names are complemented CP932 in a directory of up to 64 entries. Lookups
// ignore ASCII case. Entry contents use TH01's XOR key 76h and optional RLE.
// Each open returns an independent owning reader, also for VFS_STREAM.
// Source metadata, names and returned buffers come from allocator. It must
// remain valid until the source and its readers close. Duplicate names
// select the first directory entry. Invalid directories allocate nothing
// from allocator.
//
// Preserves RLE's repeated-run and discarded-lookahead rules. Some original
// entries finish a run beyond their packed range, in the next entry's bytes.
// Reads stay within the archive and output stops at the recorded size, so
// those entries load without reproducing the DOS heap overflow. Packed and
// decoded sizes must fit the original 16-bit byte counts.
RESULT Pf_Open(
    READER *archive, const char *name, const ALLOCATOR *allocator,
    VFS_SOURCE **out);
