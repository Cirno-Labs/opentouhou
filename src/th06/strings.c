#include "th06/strings.h"

#define M_TEXT(id_, address_, text_) [STRING_##id_] = text_,

const char *const g_Strings[STRING_COUNT] = { TH06_STRINGS(M_TEXT) };
