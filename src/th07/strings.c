#include "th07/strings.h"

#define M_TEXT(id_, address_, text_) [STRING_##id_] = text_,
#define M_ADDRESS(id_, address_, text_) [STRING_##id_] = address_,

const char *g_Strings[STRING_COUNT] = { TH07_STRINGS(M_TEXT) };
static const char *const m_Defaults[STRING_COUNT] = { TH07_STRINGS(M_TEXT) };
static const uint32_t m_Addresses[STRING_COUNT] = { TH07_STRINGS(M_ADDRESS) };

const TRANSLATION_GAME g_TranslationGame = {
    .id = "th07",
    .image_base = 0x400000,
    .font_cache_weight = 700,
    .strings = g_Strings,
    .defaults = m_Defaults,
    .addresses = m_Addresses,
    .string_count = STRING_COUNT,
    .spell_count = 141,
    .stage_count = 8,
};
