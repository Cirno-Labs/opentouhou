#include "core/patch/language.h"

#include "core/base/string.h"

#include <stdio.h>
#include <string.h>

static char M_Normalize(const char c)
{
    if (c == '_') {
        return '-';
    }
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static bool M_IsAny(const char *const text, const char *const *const list)
{
    for (int32_t i = 0; list[i] != nullptr; i++) {
        if (strcmp(text, list[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool Patch_IsLanguagePack(const char *const id)
{
    return Str_StartsWith(id, PATCH_LANGUAGE_PREFIX)
        && id[strlen(PATCH_LANGUAGE_PREFIX)] != '\0';
}

int32_t Patch_GetLanguageTags(
    const char *const code,
    char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE])
{
    const size_t length = strlen(code);
    if (length == 0 || length >= PATCH_LANGUAGE_TAG_SIZE) {
        return 0;
    }
    char full[PATCH_LANGUAGE_TAG_SIZE];
    for (size_t i = 0; i <= length; i++) {
        full[i] = M_Normalize(code[i]);
    }
    const size_t primary = strcspn(full, "-");
    if (primary == 2 && strncmp(full, "zh-", 3) == 0) {
        static const char *const simplified[] = { "cn", "sg", nullptr };
        static const char *const traditional[] = { "tw", "hk", "mo", nullptr };
        if (M_IsAny(full + 3, simplified)) {
            snprintf(full, sizeof(full), "zh-hans");
        } else if (M_IsAny(full + 3, traditional)) {
            snprintf(full, sizeof(full), "zh-hant");
        }
    }
    snprintf(tags[0], PATCH_LANGUAGE_TAG_SIZE, "%s", full);
    if (full[primary] == '\0' || primary == 0) {
        return 1;
    }
    snprintf(tags[1], PATCH_LANGUAGE_TAG_SIZE, "%.*s", (int)primary, full);
    return 2;
}

void Patch_GetLocaleCode(
    const LOCALE locale, char code[PATCH_LANGUAGE_TAG_SIZE])
{
    snprintf(
        code, PATCH_LANGUAGE_TAG_SIZE, "%s%s%s", locale.language,
        locale.language[0] != '\0' && locale.country[0] != '\0' ? "-" : "",
        locale.language[0] != '\0' ? locale.country : "");
}

bool Patch_LanguageIs(const char *const pack_code, const char *const tag)
{
    size_t i = 0;
    for (; pack_code[i] != '\0' && tag[i] != '\0'; i++) {
        if (M_Normalize(pack_code[i]) != M_Normalize(tag[i])) {
            return false;
        }
    }
    return pack_code[i] == '\0' && tag[i] == '\0';
}

bool Patch_IsOriginalLanguage(const char *const tag)
{
    const size_t length = strlen(PATCH_LANGUAGE_ORIGINAL);
    return strncmp(tag, PATCH_LANGUAGE_ORIGINAL, length) == 0
        && (tag[length] == '\0' || tag[length] == '-');
}
