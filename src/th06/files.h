#pragma once

#include "core/base/result.h"
#include "core/io/reader.h"

// Loads th06's data files: the six archives, with the data directory
// below them for files no archive has.

// Mounts data_dir and 紅魔郷IN, MD, ST, TL, CM and ED.DAT from it. A file
// in more than one archive comes from the earliest in that order. An
// archive that is missing, or that has no ver0102.dat, is a failure.
RESULT Files_Init(const char *data_dir);
// Releases what Files_Init mounted.
void Files_Shutdown(void);
// Opens path in memory, from the archives or else the data directory.
// The archives find a file by the last component of its path.
RESULT Files_Load(const char *path, READER *out);
