#pragma once

#include "th02/high_scores/format.h"

#include "core/vfs.h"

// Loads the user scores, or without a native config imports the originals
// read-only. Corrupt user scores are removed and fail with out unchanged.
RESULT HighScoresFile_Load(
    VFS *original_files, VFS *user_files, HIGH_SCORES *out);

// Writes HUUHI.DAT to the user files, replacing it atomically.
RESULT HighScoresFile_Save(VFS *user_files, const HIGH_SCORES *scores);
