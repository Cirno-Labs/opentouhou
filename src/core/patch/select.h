#pragma once

#include "core/base/result.h"
#include "core/base/types.h"
#include "core/ini/types.h"
#include "core/io/locale.h"
#include "core/json/types.h"

// Decides which installed patches of a patches folder load, in which
// order, and why. The folder has the layout that core/patch/stack.h
// describes. A dependency "<repo>/<patch>" names that patch; a bare
// "<patch>" names the patch of that name in the same repository, or else
// in the first repository, in byte order, that has one, as in thcrap.
//
// Roles:
// - library: a patch that another installed patch depends on. It loads
//   only when a loaded patch needs it.
// - language: a language pack (core/patch/language.h) that no installed
//   patch depends on, and the chosen language pack.
// - mod: every other patch.
//
// The language is the pack that the language code matches. Without a code,
// it is the only top-level language pack, one that no other language pack
// depends on, or of several, the first that a preferred locale matches. A
// code for the original language, or one that matches no pack, chooses
// none.
//
// The chosen language pack and each mod load with their dependencies. A
// mod that needs a language pack outside the chosen pack and its
// dependencies does not load. The chosen language and its dependencies
// apply first, then the mods. A patch applies after the patches that it
// depends on; ties go in byte order of the ids.
//
// A patch does not load, and nor does a patch that needs it, when it is
// disabled, when its patch.js does not parse as an object, when its
// supported_games does not list the game, when a dependency is missing,
// or when its dependencies form a loop.

typedef struct PATCH_SELECTION PATCH_SELECTION;

typedef struct {
    // When false, no patch loads.
    bool enabled;
    // Lists patch ids separated by commas. nullptr lists none.
    const char *disabled;
    // Holds the language code, or nullptr or "" when it is not set.
    const char *language;
    // Holds the preferred locales, most preferred first.
    const LOCALE *locales;
    int32_t locale_count;
} PATCH_CHOICE;

// Reads enabled, disabled and language of the [patches] section of ini into
// choice, and leaves the locales as they are. The strings belong to ini.
// enabled is true unless it is false, no, off or 0; a value that is not a
// boolean is a warning.
void Patch_ReadChoice(const INI *ini, PATCH_CHOICE *choice);

typedef enum {
    PATCH_ROLE_LANGUAGE,
    PATCH_ROLE_LIBRARY,
    PATCH_ROLE_MOD,
} PATCH_ROLE;

// Returns "language", "library" or "mod".
const char *Patch_GetRoleName(PATCH_ROLE role);

typedef struct {
    // Holds "<repo>/<patch>".
    const char *id;
    // Holds the title from patch.js, or the id when patch.js has none.
    const char *title;
    // Ends with '/'.
    const char *dir;
    // Holds nullptr when patch.js does not parse as an object.
    const JSON_VALUE *patch_js;
    PATCH_ROLE role;
    bool load;
    // Holds the role and the reason as the description gives them.
    const char *reason;
    // Holds the entry indices of the installed dependencies.
    const int32_t *dependencies;
    int32_t dependency_count;
} PATCH_ENTRY;

// Selects the patches of dir for the game game_id. A nullptr game_id
// accepts every supported_games. A nullptr choice is enabled, with nothing
// disabled, no language code and no locale. A missing dir has no patches.
// Reports failure only when dir exists and cannot be listed.
RESULT Patch_Select(
    const char *dir, const char *game_id, const PATCH_CHOICE *choice,
    PATCH_SELECTION **out);

// Releases a selection. A nullptr selection does nothing.
void Patch_FreeSelection(PATCH_SELECTION *selection);

// Returns the installed patches, in byte order of the ids. An index out of
// range is fatal.
int32_t Patch_GetEntryCount(const PATCH_SELECTION *selection);
const PATCH_ENTRY *Patch_GetEntry(
    const PATCH_SELECTION *selection, int32_t index);

// Returns the patches that load, in load order. An index out of range is
// fatal.
int32_t Patch_GetLoadCount(const PATCH_SELECTION *selection);
const PATCH_ENTRY *Patch_GetLoad(
    const PATCH_SELECTION *selection, int32_t index);

// Returns whether the language code matches no usable language pack.
bool Patch_IsLanguageMissing(const PATCH_SELECTION *selection);

// Returns the description of the selection as lines that end with '\n'.
// Mem_Free releases it. It names dir, the language and why, then gives one
// line per installed patch and the load order:
//
// Patches in /home/user/.local/share/opentouhou/patches:
//   language: thpatch/lang_de ([patches] language = de)
//   load  thpatch/lang_de                  language
//   load  thpatch/lang_en                  library, needed by ...
//   skip  thpatch/lang_fr                  language, not chosen
//   skip  a/off                            disabled in [patches] disabled
// Load order: thpatch/lang_en, thpatch/lang_de
//
// The loaded patches come first, each before the patches that it needs:
// the chosen language, then the mods. The skipped patches follow by role:
// languages, libraries, mods, then the disabled patches.
char *Patch_DescribeSelection(const PATCH_SELECTION *selection);
