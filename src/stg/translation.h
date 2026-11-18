#pragma once

#include "core/patch/stack.h"

#include <stdint.h>

// Holds the translations that thcrap patches give for a game's text
// outside its dialogue and endings: the strings of the executable
// (stringlocs.js and stringdefs.js, applied to the game's string table),
// the spell card names (spells.js), the stage titles (stages.js), the
// song titles (themes.js) and the music room comments (musiccmt.js). The
// strings of the executable are plain: thcrap's markup and translator
// notes are taken out. The other texts keep them, for Anm_RenderText to
// lay out.
//
// The translations only change what is drawn. Nothing that the simulation
// reads or that the score file stores changes, so the spell card records
// keep the names from the ECL files.

// Describes a game to Translation_Init: its thcrap id (the directory of
// its lang patches below thpatch/lang), the base address of its retail
// executable, its string table (defaults, retail addresses or stringdefs
// keys, and count) and its spell card and stage counts.
typedef struct {
    const char *id;
    uint32_t image_base;
    const char **strings; // the game's string table (mutable pointers);
                          // translations replace its entries
    const char *const *defaults;
    const uint32_t *addresses; // 0 where a string's address is not known
    // The stringdefs key that replaces a string's address, or nullptr.
    // nullptr for a game without keys.
    const char *const *string_keys;
    int32_t string_count;
    int32_t spell_count;
    int32_t stage_count;
    // The weight that base_tsa caches for this game; 0 keeps each request.
    int32_t font_cache_weight;
} TRANSLATION_GAME;

// Holds the number of music room tunes that musiccmt.js can comment on.
#define TRANSLATION_TUNE_COUNT 32
// Holds the number of comment rows of a music room tune.
#define TRANSLATION_COMMENT_ROWS 8
// Holds the highest track that themes.js names.
#define TRANSLATION_TRACK_COUNT 32

// Returns the font family that the patch settings name, from thcrap's
// "font" setting, or nullptr when they name none.
const char *Translation_GetFont(void);

// Returns base_tsa's cached font weight for the game, or requested when
// the patch or its font cache is absent.
int32_t Translation_GetFontWeight(int32_t requested);

// Returns the font files of the patches, the patch that applies last
// first, and sets *count to their number. The paths belong to the stack of
// Translation_Init.
const char *const *Translation_GetFontFiles(int32_t *count);

// Returns whether nmlgc/base_tsa is in the patch stack. A game then draws
// its nameplates and its bomb and spell card names as that patch's
// executable patches do.
bool Translation_HasBaseTsa(void);

// Loads the translations of patches and applies the string table of game
// to its strings. Never fails: a file that is missing or broken leaves the
// original text. A nullptr patches loads nothing.
void Translation_Init(const PATCH_STACK *patches, const TRANSLATION_GAME *game);

// Releases the translations and gives the game's strings their defaults
// back.
void Translation_Shutdown(void);

// Returns the name of the spell card index, 0 to game->spell_count - 1,
// from spells.js, or original.
const char *Translation_GetSpellName(int32_t index, const char *original);

// Returns the title of stage, 1 to game->stage_count, from stages.js, or
// original.
const char *Translation_GetStageName(int32_t stage, const char *original);

// Returns the title of track, 1 to TRANSLATION_TRACK_COUNT, from
// themes.js, or original.
const char *Translation_GetTheme(int32_t track, const char *original);

// Returns the title of the music room tune, counted from 0, from
// themes.js, or original.
const char *Translation_GetMusicTitle(int32_t tune, const char *original);

// Returns comment row of the music room tune from musiccmt.js, or nullptr
// when musiccmt.js has no comment for the tune. A row past the comment is
// empty. A row "@" is the tune's number and title, in the format of the
// string "Music Room Numbered Title", or the title alone without that
// string.
const char *Translation_GetMusicComment(int32_t tune, int32_t row);
