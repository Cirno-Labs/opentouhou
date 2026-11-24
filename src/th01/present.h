#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"

// Requires the allocator group through Close.
RESULT Present_Init(const ALLOCATOR *allocator);

// Draws the visible graphics page before the optional text plane.
void Present_Frame(bool text_visible);

void Present_Close(void);
