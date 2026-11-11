#pragma once

#include "th06/anm/types.h"

#include "core/base/memory.h"
#include "core/base/result.h"
#include "stg/rng.h"

// Renews the presentation identity after copying a VM or teleporting it.
// Does not change animation state or consume random values.
void Anm_RenewPresentation(ANM_VM *vm);

// Sets up empty sprite, script and file tables. They and every file
// Anm_Load parses come from allocator. Opcode 16 draws from rng.
void Anm_Init(const ALLOCATOR *allocator, RNG *rng);
// Releases every file and the tables.
void Anm_Shutdown(void);

// Releases slot, then loads the ANM file at path into it, numbering its
// sprites and scripts from first_sprite. Reports failure, naming path,
// when the file cannot be read, when a sprite or script number does not
// fit the tables, or when a script is malformed: an instruction runs past
// the file, has more than 16 bytes of arguments, or jumps to a position
// that is not an instruction. A failed load leaves slot empty. An image
// that a patch replaces keeps its own alpha when the patch does not also
// replace its alpha mask, as thcrap does.
RESULT Anm_Load(int32_t slot, const char *path, int32_t first_sprite);
// Releases the ANM file loaded into slot. Does nothing when slot is not
// loaded.
void Anm_Release(int32_t slot);
// Forgets scripts 0x100 to 0x179, the title menu's.
void Anm_ClearMenuScripts(void);
// Returns the loaded script at index, or nullptr when there is none.
const ANM_SCRIPT *Anm_GetScript(int32_t index);

// Sets vm up as new: zeroed, with no sprite and every TIMER started.
void Anm_CreateVm(ANM_VM *vm);
// Resets the rotation, scale, color, flags, interpolations and
// script_time of vm. Keeps pos, the sprite and the script.
void Anm_InitVm(ANM_VM *vm);
// Shows sprite on vm. Does nothing when sprite is out of range or unused.
void Anm_SetSprite(ANM_VM *vm, int32_t sprite);
// Resets vm and starts script on it, running its first frame at once.
// With no script loaded at that number, vm runs no script.
void Anm_StartScript(ANM_VM *vm, int32_t script);
// Like Anm_StartScript, after zeroing pos and pos_offset and setting
// font_width and font_height to 15.
void Anm_StartScriptAtOrigin(ANM_VM *vm, int32_t script);
// Runs vm's script for this frame: the instructions due (jumping to a
// pending interrupt's label first), then the rotation, scale, alpha and
// position interpolations they started. Returns true once the script has
// ended (opcode 0 or 15), without running the interpolations that frame.
bool Anm_Step(ANM_VM *vm);
