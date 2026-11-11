#include "th06/translation.h"

#include "th06/strings.h"

#include "core/base/arena.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/json.h"
#include "core/patch.h"

#include <stdio.h>
#include <string.h>

#define M_IMAGE_BASE 0x400000
#define M_STAGE_COUNT 7
// Holds the highest track that themes.js names for th06.
#define M_TRACK_COUNT 32
#define M_ARENA_CAPACITY (64 * 1024 * 1024)
#define M_NUMBERED_TITLE "Music Room Numbered Title"
#define M_KEY_SIZE 16
#define M_DEFAULT(id_, address_, text_) [STRING_##id_] = text_,
#define M_ADDRESS(id_, address_, text_) [STRING_##id_] = address_,

static char *m_SpellNames[TRANSLATION_SPELL_COUNT];
static char *m_StageNames[M_STAGE_COUNT + 1];
static char *m_Themes[M_TRACK_COUNT + 1];
static char *m_Comments[TRANSLATION_TUNE_COUNT][TRANSLATION_COMMENT_ROWS];
static bool m_HasComment[TRANSLATION_TUNE_COUNT];
static char *m_Font;
static const char **m_FontFiles;
static int32_t m_FontFileCount;
static char *m_Strings[STRING_COUNT];
static const char *const m_Defaults[STRING_COUNT] = { TH06_STRINGS(M_DEFAULT) };
static const uint32_t m_Addresses[STRING_COUNT] = { TH06_STRINGS(M_ADDRESS) };
// Holds the format of a numbered title when patches give none.
static const char m_DefaultNumberedTitle[] = "%d %s";

static char *M_Plain(const char *const text)
{
    return text != nullptr ? Patch_PlainText(text) : nullptr;
}

// Returns the text of key in object: a string, or the first line of an
// array of strings.
static const char *M_GetText(const JSON_VALUE *const object, const char *key)
{
    const JSON_VALUE *const value = Json_Get(object, key);
    if (value != nullptr && value->type == JSON_ARRAY) {
        return Json_GetString(Json_At(value, 0), nullptr);
    }
    return Json_GetString(value, nullptr);
}

static const char *M_GetTextAt(const JSON_VALUE *const object, const int32_t n)
{
    char key[M_KEY_SIZE];
    snprintf(key, sizeof(key), "%d", n);
    return M_GetText(object, key);
}

static void M_TranslateStrings(const PATCH_STRINGS *const strings)
{
    for (int32_t i = 0; i < STRING_COUNT; i++) {
        if (m_Addresses[i] == 0) {
            continue;
        }
        const char *const text =
            Patch_TranslateAt(strings, m_Addresses[i], m_Defaults[i]);
        if (text == m_Defaults[i]) {
            continue;
        }
        char *const plain = Patch_PlainText(text);
        if (!Patch_SameConversions(m_Defaults[i], plain)) {
            LOG_WARNING(
                "Rejected the translation of 0x%x: its markup changes its "
                "conversions",
                (unsigned)m_Addresses[i]);
            Mem_Free(plain);
            continue;
        }
        m_Strings[i] = plain;
        g_Strings[i] = plain;
    }
}

static void M_LoadComments(
    const JSON_VALUE *const comments, const PATCH_STRINGS *const strings)
{
    const char *const format =
        Patch_Translate(strings, M_NUMBERED_TITLE, m_DefaultNumberedTitle);
    char *const plain_format = Patch_PlainText(format);
    const bool numbered = format != m_DefaultNumberedTitle
        && Patch_SameConversions(plain_format, m_DefaultNumberedTitle);
    for (int32_t tune = 0; tune < TRANSLATION_TUNE_COUNT; tune++) {
        char key[M_KEY_SIZE];
        snprintf(key, sizeof(key), "%d", tune + 1);
        const JSON_VALUE *const rows = Json_Get(comments, key);
        if (rows == nullptr || rows->type != JSON_ARRAY) {
            continue;
        }
        m_HasComment[tune] = true;
        const char *const title =
            m_Themes[tune + 1] != nullptr ? m_Themes[tune + 1] : "";
        for (int32_t row = 0; row < TRANSLATION_COMMENT_ROWS; row++) {
            const char *const text = Json_GetString(Json_At(rows, row), "");
            if (strcmp(text, "@") != 0) {
                m_Comments[tune][row] = Patch_PlainText(text);
            } else if (numbered) {
                m_Comments[tune][row] =
                    Str_Format(plain_format, tune + 1, title);
            } else {
                m_Comments[tune][row] = Str_Dup(title);
            }
        }
    }
    Mem_Free(plain_format);
}

static void M_LoadFont(
    const PATCH_STACK *const patches, const ALLOCATOR *const allocator)
{
    const char *const font = Json_GetString(
        Json_Get(Patch_LoadSettings(patches, allocator), "font"), nullptr);
    m_Font = font != nullptr ? Str_Dup(font) : nullptr;
    int32_t count = 0;
    for (int32_t i = 0; i < Patch_GetCount(patches); i++) {
        count += Patch_Get(patches, i)->font_count;
    }
    m_FontFiles = Mem_AllocArr((size_t)(count > 0 ? count : 1), sizeof(char *));
    for (int32_t i = Patch_GetCount(patches) - 1; i >= 0; i--) {
        const PATCH_INFO *const info = Patch_Get(patches, i);
        for (int32_t j = 0; j < info->font_count; j++) {
            m_FontFiles[m_FontFileCount++] = info->fonts[j];
        }
    }
}

void Translation_Init(const PATCH_STACK *const patches)
{
    if (patches == nullptr) {
        return;
    }
    ARENA arena;
    Arena_Init(&arena, "translation", M_ARENA_CAPACITY);
    const ALLOCATOR *const allocator = Arena_Allocator(&arena);
    PATCH_STRINGS *const strings = Patch_LoadStrings(patches, M_IMAGE_BASE);
    M_TranslateStrings(strings);
    M_LoadFont(patches, allocator);

    const JSON_VALUE *const spells =
        Patch_LoadJson(patches, PATCH_GAME_DIR, "spells.js", allocator);
    for (int32_t i = 0; i < TRANSLATION_SPELL_COUNT; i++) {
        m_SpellNames[i] = M_Plain(M_GetTextAt(spells, i));
    }
    const JSON_VALUE *const stages =
        Patch_LoadJson(patches, PATCH_GAME_DIR, "stages.js", allocator);
    for (int32_t i = 1; i <= M_STAGE_COUNT; i++) {
        m_StageNames[i] = M_Plain(M_GetTextAt(stages, i));
    }
    const JSON_VALUE *const themes =
        Patch_LoadJson(patches, PATCH_ROOT, "themes.js", allocator);
    for (int32_t i = 1; i <= M_TRACK_COUNT; i++) {
        char key[M_KEY_SIZE];
        snprintf(key, sizeof(key), "th06_%02d", i);
        m_Themes[i] = M_Plain(Json_GetString(Json_Get(themes, key), nullptr));
    }
    M_LoadComments(
        Patch_LoadJson(patches, PATCH_GAME_DIR, "musiccmt.js", allocator),
        strings);

    Patch_FreeStrings(strings);
    Arena_Free(&arena);
}

void Translation_Shutdown(void)
{
    Mem_Free(m_Font);
    m_Font = nullptr;
    Mem_Free(m_FontFiles);
    m_FontFiles = nullptr;
    m_FontFileCount = 0;
    for (int32_t i = 0; i < TRANSLATION_SPELL_COUNT; i++) {
        Mem_Free(m_SpellNames[i]);
        m_SpellNames[i] = nullptr;
    }
    for (int32_t i = 0; i <= M_STAGE_COUNT; i++) {
        Mem_Free(m_StageNames[i]);
        m_StageNames[i] = nullptr;
    }
    for (int32_t i = 0; i <= M_TRACK_COUNT; i++) {
        Mem_Free(m_Themes[i]);
        m_Themes[i] = nullptr;
    }
    for (int32_t tune = 0; tune < TRANSLATION_TUNE_COUNT; tune++) {
        for (int32_t row = 0; row < TRANSLATION_COMMENT_ROWS; row++) {
            Mem_Free(m_Comments[tune][row]);
            m_Comments[tune][row] = nullptr;
        }
        m_HasComment[tune] = false;
    }
    for (int32_t i = 0; i < STRING_COUNT; i++) {
        Mem_Free(m_Strings[i]);
        m_Strings[i] = nullptr;
        g_Strings[i] = m_Defaults[i];
    }
}

const char *Translation_GetSpellName(
    const int32_t index, const char *const original)
{
    if (index < 0 || index >= TRANSLATION_SPELL_COUNT
        || m_SpellNames[index] == nullptr) {
        return original;
    }
    return m_SpellNames[index];
}

const char *Translation_GetStageName(
    const int32_t stage, const char *const original)
{
    if (stage < 1 || stage > M_STAGE_COUNT || m_StageNames[stage] == nullptr) {
        return original;
    }
    return m_StageNames[stage];
}

const char *Translation_GetSongName(
    const int32_t stage, const int32_t song, const char *const original)
{
    if (song < 0 || song > 1) {
        return original;
    }
    const int32_t track = stage * 2 + song;
    if (track < 1 || track > M_TRACK_COUNT || m_Themes[track] == nullptr) {
        return original;
    }
    return m_Themes[track];
}

const char *Translation_GetMusicTitle(
    const int32_t tune, const char *const original)
{
    const int32_t track = tune + 1;
    if (track < 1 || track > M_TRACK_COUNT || m_Themes[track] == nullptr) {
        return original;
    }
    return m_Themes[track];
}

const char *Translation_GetMusicComment(const int32_t tune, const int32_t row)
{
    if (tune < 0 || tune >= TRANSLATION_TUNE_COUNT || row < 0
        || row >= TRANSLATION_COMMENT_ROWS || !m_HasComment[tune]) {
        return nullptr;
    }
    return m_Comments[tune][row];
}

const char *Translation_GetFont(void)
{
    return m_Font;
}

const char *const *Translation_GetFontFiles(int32_t *const count)
{
    *count = m_FontFileCount;
    return m_FontFiles;
}
