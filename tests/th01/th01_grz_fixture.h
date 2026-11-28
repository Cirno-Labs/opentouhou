#pragma once

#include "th01/grz.h"

// Loads one HGRX image exactly as src/th01/grz.c's Grz_Load loads each of a
// container's images, but standalone: no caller outside tests needs it.
RESULT Grx_Load(READER *reader, const ALLOCATOR *allocator, GRX *out);
