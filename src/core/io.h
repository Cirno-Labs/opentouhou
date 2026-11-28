#pragma once

// Reads and writes files and byte streams, finds the user's locale and
// runs other programs: the platform-facing I/O every other module goes
// through.

#include "core/io/file.h"
#include "core/io/locale.h"
#include "core/io/path.h"
#include "core/io/process.h"
#include "core/io/reader.h"
#include "core/io/writer.h"
