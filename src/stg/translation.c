#include "stg/translation.h"

#include "core/base/arena.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/json.h"
#include "core/patch.h"

#include <stdio.h>
#include <string.h>

#define M_ARENA_CAPACITY (64 * 1024 * 1024)
#define M_NUMBERED_TITLE "Music Room Numbered Title"
#define M_KEY_SIZE 16
#define M_BASE_TSA_PATCH "nmlgc/base_tsa"
#define M_THEME_KEY_SIZE 32

static const TRANSLATION_GAME *m_Game;
static char **m_SpellNames; // m_Game->spell_count entries
static char **m_StageNames; // m_Game->stage_count + 1 entries
static char *m_Themes[TRANSLATION_TRACK_COUNT + 1];
static char *m_Comments[TRANSLATION_TUNE_COUNT][TRANSLATION_COMMENT_ROWS];
static bool m_HasComment[TRANSLATION_TUNE_COUNT];
static char *m_Font;
static bool m_HasBaseTsa;
static const char **m_FontFiles;
static int32_t m_FontFileCount;
static char **m_Strings; // m_Game->string_count entries
// Holds the format of a numbered title when patches give none.
static const char m_DefaultNumberedTitle[] = "%d %s";

static char *M_Copy(const char *const text)
{
    return text != nullptr ? Str_Dup(text) : nullptr;
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
    for (int32_t i = 0; i < m_Game->string_count; i++) {
        if (m_Game->addresses[i] == 0) {
            continue;
        }
        const char *const text = Patch_TranslateAt(
            strings, m_Game->addresses[i], m_Game->defaults[i]);
        if (text == m_Game->defaults[i]) {
            continue;
        }
        char *const plain = Patch_PlainText(text);
        if (!Patch_SameConversions(m_Game->defaults[i], plain)) {
            LOG_WARNING(
                "Rejected the translation of 0x%x: its markup changes its "
                "conversions",
                (unsigned)m_Game->addresses[i]);
            Mem_Free(plain);
            continue;
        }
        m_Strings[i] = plain;
        m_Game->strings[i] = plain;
    }
}

static void M_LoadComments(
    const JSON_VALUE *const comments, const PATCH_STRINGS *const strings)
{
    const char *const format =
        Patch_Translate(strings, M_NUMBERED_TITLE, m_DefaultNumberedTitle);
    const bool numbered = format != m_DefaultNumberedTitle;
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
                m_Comments[tune][row] = Str_Dup(text);
            } else if (numbered) {
                m_Comments[tune][row] = Str_Format(format, tune + 1, title);
            } else {
                m_Comments[tune][row] = Str_Dup(title);
            }
        }
    }
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

void Translation_Init(
    const PATCH_STACK *const patches, const TRANSLATION_GAME *const game)
{
    if (patches == nullptr) {
        return;
    }
    m_Game = game;
    m_SpellNames = Mem_AllocArrZ((size_t)game->spell_count, sizeof(char *));
    m_StageNames = Mem_AllocArrZ((size_t)game->stage_count + 1, sizeof(char *));
    m_Strings = Mem_AllocArrZ((size_t)game->string_count, sizeof(char *));
    ARENA arena;
    Arena_Init(&arena, "translation", M_ARENA_CAPACITY);
    const ALLOCATOR *const allocator = Arena_Allocator(&arena);
    PATCH_STRINGS *const strings = Patch_LoadStrings(patches, game->image_base);
    M_TranslateStrings(strings);
    M_LoadFont(patches, allocator);
    for (int32_t i = 0; i < Patch_GetCount(patches); i++) {
        if (strcmp(Patch_Get(patches, i)->id, M_BASE_TSA_PATCH) == 0) {
            m_HasBaseTsa = true;
        }
    }

    const JSON_VALUE *const spells =
        Patch_LoadJson(patches, PATCH_GAME_DIR, "spells.js", allocator);
    for (int32_t i = 0; i < game->spell_count; i++) {
        m_SpellNames[i] = M_Copy(M_GetTextAt(spells, i));
    }
    const JSON_VALUE *const stages =
        Patch_LoadJson(patches, PATCH_GAME_DIR, "stages.js", allocator);
    for (int32_t i = 1; i <= game->stage_count; i++) {
        m_StageNames[i] = M_Copy(M_GetTextAt(stages, i));
    }
    const JSON_VALUE *const themes =
        Patch_LoadJson(patches, PATCH_ROOT, "themes.js", allocator);
    for (int32_t i = 1; i <= TRANSLATION_TRACK_COUNT; i++) {
        char key[M_THEME_KEY_SIZE];
        snprintf(key, sizeof(key), "%s_%02d", game->id, i);
        m_Themes[i] = M_Copy(Json_GetString(Json_Get(themes, key), nullptr));
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
    m_HasBaseTsa = false;
    Mem_Free(m_FontFiles);
    m_FontFiles = nullptr;
    m_FontFileCount = 0;
    if (m_Game == nullptr) {
        return;
    }
    for (int32_t i = 0; i < m_Game->spell_count; i++) {
        Mem_Free(m_SpellNames[i]);
    }
    Mem_Free(m_SpellNames);
    m_SpellNames = nullptr;
    for (int32_t i = 0; i <= m_Game->stage_count; i++) {
        Mem_Free(m_StageNames[i]);
    }
    Mem_Free(m_StageNames);
    m_StageNames = nullptr;
    for (int32_t i = 0; i <= TRANSLATION_TRACK_COUNT; i++) {
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
    for (int32_t i = 0; i < m_Game->string_count; i++) {
        Mem_Free(m_Strings[i]);
        m_Game->strings[i] = m_Game->defaults[i];
    }
    Mem_Free(m_Strings);
    m_Strings = nullptr;
    m_Game = nullptr;
}

const char *Translation_GetSpellName(
    const int32_t index, const char *const original)
{
    if (m_Game == nullptr || index < 0 || index >= m_Game->spell_count
        || m_SpellNames[index] == nullptr) {
        return original;
    }
    return m_SpellNames[index];
}

const char *Translation_GetStageName(
    const int32_t stage, const char *const original)
{
    if (m_Game == nullptr || stage < 1 || stage > m_Game->stage_count
        || m_StageNames[stage] == nullptr) {
        return original;
    }
    return m_StageNames[stage];
}

const char *Translation_GetTheme(
    const int32_t track, const char *const original)
{
    if (track < 1 || track > TRANSLATION_TRACK_COUNT
        || m_Themes[track] == nullptr) {
        return original;
    }
    return m_Themes[track];
}

const char *Translation_GetMusicTitle(
    const int32_t tune, const char *const original)
{
    const int32_t track = tune + 1;
    if (track < 1 || track > TRANSLATION_TRACK_COUNT
        || m_Themes[track] == nullptr) {
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

int32_t Translation_GetFontWeight(const int32_t requested)
{
    return m_HasBaseTsa && m_Game != nullptr && m_Game->font_cache_weight > 0
        ? m_Game->font_cache_weight
        : requested;
}

const char *const *Translation_GetFontFiles(int32_t *const count)
{
    *count = m_FontFileCount;
    return m_FontFiles;
}

bool Translation_HasBaseTsa(void)
{
    return m_HasBaseTsa;
}
