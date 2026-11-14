#pragma once

#include "core/image.h"
#include "core/io/reader.h"

// Creates the texture of slot from the ANM whose header starts reader.
RESULT AnmTexture_LoadSlot(READER *reader, int32_t slot);

// Returns the pixels that ANM keeps of the texture of slot.
const IMAGE *AnmTexture_GetImage(int32_t slot);
