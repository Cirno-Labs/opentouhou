#pragma once

#include "th01/gameplay_assets.h"

// Creates complete, synthetic assets for public construction and lifecycle
// tests. All bytes and mutable fixture backing belong to allocator.
GAMEPLAY_ASSETS *Th01TestAssets_Create(const ALLOCATOR *allocator);
