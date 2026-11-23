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

// Renews the presentation identity after copying a VM or teleporting it.
// Does not change animation state or consume random values.
void Anm_RenewPresentation(ANM_VM *vm);

// Sets vm up as new: zeroed, with no sprite.
void Anm_CreateVm(ANM_VM *vm);

// Resets the animation state of vm, keeping pos, the sprite and the script.
void Anm_InitVm(ANM_VM *vm);

// Shows sprite on vm. Does nothing when sprite is out of range or unused.
void Anm_SetSprite(ANM_VM *vm, int32_t sprite);

// Sets the alpha of vm's color, keeping its red, green and blue.
void Anm_SetAlpha(ANM_VM *vm, uint8_t alpha);

// Resets vm and starts script on it, running its first frame at once.
// Without a loaded script at that number, clears all of vm.
void Anm_StartScript(ANM_VM *vm, int32_t script);

// Like Anm_StartScript, after zeroing pos and pos_offset and setting
// font_width and font_height to 15.
void Anm_StartScriptAtOrigin(ANM_VM *vm, int32_t script);

// Runs vm's script for one frame at g_GameSpeed. Returns true once the
// script has ended.
bool Anm_Step(ANM_VM *vm);

// Steps each of vms whose script_index is not negative.
void Anm_StepVms(ANM_VM *vms, int32_t count);

// Starts script + i at the origin on vms[i] and makes its sprite its
// base_sprite.
void Anm_StartVmSeries(ANM_VM *vms, int32_t script, int32_t count);

// Sends interrupt to each of vms that shows a sprite of a loaded texture.
void Anm_SendInterrupt(ANM_VM *vms, int32_t count, int16_t interrupt);
