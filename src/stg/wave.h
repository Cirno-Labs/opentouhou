#pragma once

#include "core/base/result.h"
#include "core/io/reader.h"

#include <stdint.h>

// Checks that reader holds a wave file with a format and a data chunk,
// and rewinds it. The chunk search stops 8 bytes short of the RIFF size,
// as the original's does, so an empty last chunk is not found.

RESULT Wave_Check(READER *reader, const char *path);

// Converts a DirectSound volume in hundredths of a decibel to a gain.
float Wave_Gain(int32_t hundredths_of_db);
