#pragma once

#include "core/vfs.h"

#include "test.h"

#include <stdlib.h>

// Returns a VFS over the built-in sprite sheets in TEST_SPRITE_DIR.
// Vfs_Destroy releases it.
static inline VFS *Th01Test_Sprites(void)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    CHECK(IS_OK(Vfs_MountDir(vfs, "", getenv("TEST_SPRITE_DIR"), 0, false)));
    return vfs;
}
