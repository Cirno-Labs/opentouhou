#pragma once

#include "th07/anm/types.h"

#include "core/base/memory.h"
#include "core/base/result.h"
#include "stg/rng.h"

// Sets up empty tables, allocated with every loaded file from allocator.
void Anm_Init(const ALLOCATOR *allocator, RNG *rng);

// Releases every file and the tables.
void Anm_Shutdown(void);

// Loads the chain of ANMs in the file at path into the slots from slot,
// numbering each one's sprites and scripts after the previous one's highest,
// from first_sprite. A failed load leaves every slot it reached empty.
RESULT Anm_Load(int32_t slot, const char *path, int32_t first_sprite);

// Releases the ANM in slot and, for the first slot of a chain, the rest of
// the chain, even slots loaded again since, as the original does.
void Anm_Release(int32_t slot);

// Returns the loaded script at index, or nullptr when there is none.
const ANM_SCRIPT *Anm_GetScript(int32_t index);

// Puts script at index, or removes it for nullptr. The script stays the
// caller's and must outlive its use.
void Anm_SetScriptEntry(int32_t index, const ANM_SCRIPT *script);

// Returns the sprite at index, or nullptr when index is out of range.
const ANM_SPRITE *Anm_GetSpriteEntry(int32_t index);

// Replaces the sprite at index with a copy of sprite.
void Anm_SetSpriteEntry(int32_t index, const ANM_SPRITE *sprite);
