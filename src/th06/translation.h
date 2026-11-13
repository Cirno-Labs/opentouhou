// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/patch/stack.h"

#include <stdint.h>

// Holds the translations that thcrap patches give for th06's text outside
// its dialogue and endings: the strings of the executable (stringlocs.js
// and stringdefs.js, applied to g_Strings), the spell card names
// (spells.js), the stage titles (stages.js), the song titles (themes.js)
// and the music room comments (musiccmt.js). The strings of the executable
// are plain: thcrap's markup and translator notes are taken out. The other
// texts keep them, for Anm_RenderText to lay out.
//
// The translations only change what is drawn. Nothing that the simulation
// reads or that the score file stores changes, so the spell card records
// keep the names from the ECL files.

// Holds the number of spell cards that spells.js can name.
#define TRANSLATION_SPELL_COUNT 64
// Holds the number of music room tunes that musiccmt.js can comment on.
#define TRANSLATION_TUNE_COUNT 32
// Holds the number of comment rows of a music room tune.
#define TRANSLATION_COMMENT_ROWS 8

// Returns the font family that the patch settings name, from thcrap's
// "font" setting, or nullptr when they name none.
const char *Translation_GetFont(void);
// Returns the font files of the patches, the patch that applies last
// first, and sets *count to their number. The paths belong to the stack of
// Translation_Init.
const char *const *Translation_GetFontFiles(int32_t *count);

// Returns whether nmlgc/base_tsa is in the patch stack. That patch ships a
// text.anm with a different geometry and patches to the executable that
// draw by it. th06 then draws its nameplates and its bomb and spell card
// names as those executable patches do.
bool Translation_HasBaseTsa(void);

// Loads the translations of patches and applies the string tables to
// g_Strings. Never fails: a file that is missing or broken leaves the
// original text. A nullptr patches loads nothing.
void Translation_Init(const PATCH_STACK *patches);
// Releases the translations and gives g_Strings its defaults back.
void Translation_Shutdown(void);

// Returns the name of the spell card index, 0 to 63, from spells.js, or
// original.
const char *Translation_GetSpellName(int32_t index, const char *original);
// Returns the title of stage, 1 to 6 and 7 for the extra stage, from
// stages.js, or original.
const char *Translation_GetStageName(int32_t stage, const char *original);
// Returns the title of a song of stage from themes.js, or original. song 0
// is the stage's theme and 1 its boss theme, which are the tracks
// stage * 2 and stage * 2 + 1.
const char *Translation_GetSongName(
    int32_t stage, int32_t song, const char *original);
// Returns the title of the music room tune, counted from 0, from
// themes.js, or original.
const char *Translation_GetMusicTitle(int32_t tune, const char *original);
// Returns comment row of the music room tune from musiccmt.js, or nullptr
// when musiccmt.js has no comment for the tune. A row past the comment is
// empty. A row "@" is the tune's number and title, in the format of the
// string "Music Room Numbered Title", or the title alone without that
// string.
const char *Translation_GetMusicComment(int32_t tune, int32_t row);
