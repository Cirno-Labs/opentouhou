#include "othpatch/language.h"

#include <stdio.h>
#include <string.h>

static bool M_IsLetter(const char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool M_IsAlnum(const char c)
{
    return M_IsLetter(c) || (c >= '0' && c <= '9');
}

static char M_Lower(const char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static bool M_IsLanguageCode(const char *const name)
{
    size_t letters = 0;
    while (M_IsLetter(name[letters])) {
        letters++;
    }
    if (letters < 2 || letters > 3) {
        return false;
    }
    for (const char *c = name + letters; *c != '\0';) {
        if (*c != '-' && *c != '_') {
            return false;
        }
        c++;
        if (!M_IsAlnum(*c)) {
            return false;
        }
        while (M_IsAlnum(*c)) {
            c++;
        }
    }
    return true;
}

int32_t Language_FromLocale(
    const LOCALE locale,
    char names[LANGUAGE_MAX_CANDIDATES][LANGUAGE_NAME_SIZE])
{
    if (locale.language[0] == '\0') {
        return 0;
    }
    if (strcmp(locale.language, "zh") == 0) {
        static const char *const simplified[] = { "CN", "SG" };
        static const char *const traditional[] = { "TW", "HK", "MO" };
        for (size_t i = 0; i < 2; i++) {
            if (strcmp(locale.country, simplified[i]) == 0) {
                snprintf(names[0], LANGUAGE_NAME_SIZE, "lang_zh-hans");
                return 1;
            }
        }
        for (size_t i = 0; i < 3; i++) {
            if (strcmp(locale.country, traditional[i]) == 0) {
                snprintf(names[0], LANGUAGE_NAME_SIZE, "lang_zh-hant");
                return 1;
            }
        }
    }
    int32_t count = 0;
    if (locale.country[0] != '\0') {
        char country[LOCALE_CODE_SIZE];
        for (size_t i = 0; i < LOCALE_CODE_SIZE; i++) {
            country[i] = M_Lower(locale.country[i]);
        }
        snprintf(
            names[count++], LANGUAGE_NAME_SIZE, "lang_%s-%s", locale.language,
            country);
    }
    snprintf(names[count++], LANGUAGE_NAME_SIZE, "lang_%s", locale.language);
    return count;
}

const char *Language_FromArgument(
    const char *const name, char buffer[LANGUAGE_NAME_SIZE])
{
    const size_t length = strlen(name);
    if (!M_IsLanguageCode(name)
        || length + strlen("lang_") >= LANGUAGE_NAME_SIZE) {
        return name;
    }
    memcpy(buffer, "lang_", strlen("lang_"));
    char *const code = buffer + strlen("lang_");
    for (size_t i = 0; i <= length; i++) {
        code[i] = name[i] == '_' ? '-' : M_Lower(name[i]);
    }
    return buffer;
}
