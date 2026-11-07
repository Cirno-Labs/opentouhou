#include "core/config.h"

#include "core/base/log.h"
#include "core/base/string.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void M_WarnBadValue(
    const CONFIG_OPTION *const option, const char *const text)
{
    LOG_WARNING(
        "%s.%s: invalid value \"%s\", keeping the default", option->section,
        option->key, text);
}

static bool M_ParseBool(const char *const text, bool *const out)
{
    if (Str_EqualIgnoreCase(text, "true") || Str_EqualIgnoreCase(text, "yes")
        || Str_EqualIgnoreCase(text, "on") || strcmp(text, "1") == 0) {
        *out = true;
        return true;
    }
    if (Str_EqualIgnoreCase(text, "false") || Str_EqualIgnoreCase(text, "no")
        || Str_EqualIgnoreCase(text, "off") || strcmp(text, "0") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static bool M_ParseInt(const char *const text, int32_t *const out)
{
    if (text[0] == '\0') {
        return false;
    }
    char *end;
    errno = 0;
    const long long value = strtoll(text, &end, 10);
    if (*end != '\0' || errno == ERANGE || value < INT32_MIN
        || value > INT32_MAX) {
        return false;
    }
    *out = (int32_t)value;
    return true;
}

// Parses text locale-independently: the program never changes the "C"
// locale that strtof starts in, where '.' is the decimal point.
static bool M_ParseFloat(const char *const text, float *const out)
{
    if (text[0] == '\0') {
        return false;
    }
    char *end;
    const float value = strtof(text, &end);
    if (*end != '\0' || isnan(value) || isinf(value)) {
        return false;
    }
    *out = value;
    return true;
}

static int32_t M_FindEnumName(
    const char *const *const names, const char *const text)
{
    for (int32_t i = 0; names[i] != nullptr; i++) {
        if (Str_EqualIgnoreCase(names[i], text)) {
            return i;
        }
    }
    return -1;
}

// Writes the shortest text that strtof reads back as the same bits as
// value into buffer, which must hold at least 32 bytes.
static void M_FormatFloat(char *const buffer, const float value)
{
    uint32_t want;
    memcpy(&want, &value, sizeof(want));
    for (int precision = 6; precision <= 9; precision++) {
        snprintf(buffer, 32, "%.*g", precision, (double)value);
        const float parsed = strtof(buffer, nullptr);
        uint32_t got;
        memcpy(&got, &parsed, sizeof(got));
        if (got == want) {
            return;
        }
    }
}

static void *M_FieldOf(void *const settings, const CONFIG_OPTION *const option)
{
    return (uint8_t *)settings + option->offset;
}

static const void *M_ConstFieldOf(
    const void *const settings, const CONFIG_OPTION *const option)
{
    return (const uint8_t *)settings + option->offset;
}

void Config_SetDefaults(const CONFIG_SCHEMA *const schema, void *const settings)
{
    for (int32_t i = 0; i < schema->count; i++) {
        const CONFIG_OPTION *const option = &schema->options[i];
        void *const field = M_FieldOf(settings, option);
        switch (option->type) {
        case CONFIG_BOOL:
            *(bool *)field = option->fallback.boolean;
            break;
        case CONFIG_INT:
        case CONFIG_ENUM:
            *(int32_t *)field = option->fallback.integer;
            break;
        case CONFIG_FLOAT:
            *(float *)field = option->fallback.real;
            break;
        }
    }
}

void Config_Load(
    const CONFIG_SCHEMA *const schema, const INI *const ini,
    void *const settings)
{
    Config_SetDefaults(schema, settings);
    for (int32_t i = 0; i < schema->count; i++) {
        const CONFIG_OPTION *const option = &schema->options[i];
        const char *const text = Ini_Get(ini, option->section, option->key);
        if (text == nullptr) {
            continue;
        }
        void *const field = M_FieldOf(settings, option);
        switch (option->type) {
        case CONFIG_BOOL: {
            bool value;
            if (M_ParseBool(text, &value)) {
                *(bool *)field = value;
            } else {
                M_WarnBadValue(option, text);
            }
            break;
        }
        case CONFIG_INT: {
            int32_t value;
            if (M_ParseInt(text, &value) && value >= option->range.integer.min
                && value <= option->range.integer.max) {
                *(int32_t *)field = value;
            } else {
                M_WarnBadValue(option, text);
            }
            break;
        }
        case CONFIG_FLOAT: {
            float value;
            if (M_ParseFloat(text, &value) && value >= option->range.real.min
                && value <= option->range.real.max) {
                *(float *)field = value;
            } else {
                M_WarnBadValue(option, text);
            }
            break;
        }
        case CONFIG_ENUM: {
            const int32_t index = M_FindEnumName(option->range.names, text);
            if (index >= 0) {
                *(int32_t *)field = index;
            } else {
                M_WarnBadValue(option, text);
            }
            break;
        }
        }
    }
}

void Config_Save(
    const CONFIG_SCHEMA *const schema, INI *const ini,
    const void *const settings)
{
    for (int32_t i = 0; i < schema->count; i++) {
        const CONFIG_OPTION *const option = &schema->options[i];
        const void *const field = M_ConstFieldOf(settings, option);
        char buffer[32];
        switch (option->type) {
        case CONFIG_BOOL:
            Ini_Set(
                ini, option->section, option->key,
                *(const bool *)field ? "true" : "false");
            break;
        case CONFIG_INT:
            snprintf(buffer, sizeof(buffer), "%d", *(const int32_t *)field);
            Ini_Set(ini, option->section, option->key, buffer);
            break;
        case CONFIG_FLOAT:
            M_FormatFloat(buffer, *(const float *)field);
            Ini_Set(ini, option->section, option->key, buffer);
            break;
        case CONFIG_ENUM:
            Ini_Set(
                ini, option->section, option->key,
                option->range.names[*(const int32_t *)field]);
            break;
        }
    }
}
