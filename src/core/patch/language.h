#pragma once

#include "core/base/types.h"
#include "core/io/locale.h"

// Maps language codes to thpatch's language packs. A language pack is a
// patch whose id is "thpatch/lang_<code>", such as "thpatch/lang_pt-br".
//
// A code is a language tag, such as "de", "pt_BR" or "zh-Hant". Codes
// compare without regard to the case of ASCII letters, and '_' equals '-'.
// A code matches its full tag first and then its primary subtag, so that
// "pt_BR" matches "pt-br" and then "pt". Chinese of China and Singapore
// matches "zh-hans" instead of its full tag, and Chinese of Taiwan, Hong
// Kong and Macao matches "zh-hant".

#define PATCH_LANGUAGE_PREFIX "thpatch/lang_"

// Stands for the original language, which has no language pack.
#define PATCH_LANGUAGE_ORIGINAL "ja"

// Sets the size of a tag, NUL included.
#define PATCH_LANGUAGE_TAG_SIZE 32

#define PATCH_LANGUAGE_MAX_TAGS 2

// Returns whether id is the id of a language pack.
bool Patch_IsLanguagePack(const char *id);

// Writes the tags that code matches into tags, best first, in lowercase
// and with '-', and returns their number. An empty code, and a code too
// long for a tag, give no tag.
int32_t Patch_GetLanguageTags(
    const char *code,
    char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE]);

// Writes the code of locale into code, such as "pt-BR", or an empty string
// for an unknown locale.
void Patch_GetLocaleCode(LOCALE locale, char code[PATCH_LANGUAGE_TAG_SIZE]);

// Returns whether the code of a language pack, the part of its name after
// "lang_", is tag. The comparison ignores case, and '_' equals '-'.
bool Patch_LanguageIs(const char *pack_code, const char *tag);

// Returns whether tag, as Patch_GetLanguageTags writes it, stands for the
// original language: its primary subtag is PATCH_LANGUAGE_ORIGINAL.
bool Patch_IsOriginalLanguage(const char *tag);
