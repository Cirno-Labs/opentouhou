#pragma once

#include "core/archive.h"

// Reads a PC-98 hard disk image in Anex86's .hdi format as an archive: the
// first FAT12 or FAT16 partition of its partition table, read-only. Names
// are the 8.3 names in lower case, with '/' separators.

extern const ARCHIVE_FORMAT g_HdiFormat;
