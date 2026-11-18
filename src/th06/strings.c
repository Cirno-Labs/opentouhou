#include "th06/strings.h"

#define M_TEXT(id_, address_, text_) [STRING_##id_] = text_,
#define M_ADDRESS(id_, address_, text_) [STRING_##id_] = address_,

const char *g_Strings[STRING_COUNT] = { TH06_STRINGS(M_TEXT) };
static const char *const m_Defaults[STRING_COUNT] = { TH06_STRINGS(M_TEXT) };
static const uint32_t m_Addresses[STRING_COUNT] = { TH06_STRINGS(M_ADDRESS) };
static const char *const m_StringKeys[STRING_COUNT] = {
    [STRING_RESULT_CLEAR_LABEL] = "th06_ascii_result_clear",
};

const TRANSLATION_GAME g_TranslationGame = {
    .id = "th06",
    .image_base = 0x400000,
    .font_cache_weight = 700,
    .strings = g_Strings,
    .defaults = m_Defaults,
    .addresses = m_Addresses,
    .string_keys = m_StringKeys,
    .string_count = STRING_COUNT,
    .spell_count = 64,
    .stage_count = 7,
};
