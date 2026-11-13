#pragma once

#include "core/base/types.h"
#include "core/io/locale.h"

// Maps languages to the names of thpatch's language packs, such as
// "lang_pt-br".

// Sets the size of a language pack name, NUL included.
#define LANGUAGE_NAME_SIZE 32

// Sets how many names Language_FromLocale writes at most.
#define LANGUAGE_MAX_CANDIDATES 2

// Writes the names of the language packs for a locale into names, best
// first, and returns their number. A locale ll_CC gives "lang_ll-cc" and
// then "lang_ll". Chinese of China and Singapore gives "lang_zh-hans", and
// Chinese of Taiwan, Hong Kong and Macao gives "lang_zh-hant". An unknown
// locale gives no name.
int32_t Language_FromLocale(
    LOCALE locale, char names[LANGUAGE_MAX_CANDIDATES][LANGUAGE_NAME_SIZE]);

// Returns the patch name that a name from the command line stands for. A
// language code, two or three ASCII letters with optional subtags after '-'
// or '_', such as "en" or "pt_BR", stands for its language pack, "lang_en"
// or "lang_pt-br", which is written into buffer. Any other name, and a code
// too long for buffer, stands for itself and is returned as it is.
const char *Language_FromArgument(
    const char *name, char buffer[LANGUAGE_NAME_SIZE]);
