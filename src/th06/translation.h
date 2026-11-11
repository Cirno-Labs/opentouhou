#pragma once

#include "core/patch/stack.h"

#include <stdint.h>

// Holds the translations that thcrap patches give for th06's text outside
// its dialogue and endings: the strings of the executable (stringlocs.js
// and stringdefs.js, applied to g_Strings), the spell card names
// (spells.js), the stage titles (stages.js), the song titles (themes.js)
// and the music room comments (musiccmt.js). Every text is plain: thcrap's
// markup and translator notes are taken out.
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
