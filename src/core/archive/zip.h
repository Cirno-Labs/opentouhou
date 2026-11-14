#pragma once

#include "core/archive.h"
#include "core/base/types.h"

// Reads zip archives as an ARCHIVE_FORMAT. Entries that are stored or
// compressed with deflate can be read, and each is checked against its
// CRC-32. ZIP64, encryption and archives of several parts are not
// supported. Names are looked up by the whole path, exactly. The Unix mode
// of an entry is the one the archive records when it was made on Unix, and
// 0 otherwise.

// Sets the largest unpacked size of an entry. An archive with a larger entry
// fails to open.
#define ZIP_MAX_ENTRY_SIZE ((size_t)256 * 1024 * 1024)

extern const ARCHIVE_FORMAT g_ZipFormat;
