#pragma once

#include "core/image.h"
#include "core/io/reader.h"

RESULT AnmTexture_LoadSlot(READER *reader, int32_t slot);
// Returns the pixels that ANM keeps of the texture of slot, which uploads
// and text rendering update.
const IMAGE *AnmTexture_GetImage(int32_t slot);
