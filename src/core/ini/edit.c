#include "core/ini/struct.h"

#include "core/base/string.h"

#include <string.h>

static char *M_Format(const INI *const ini, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *const temp = Str_FormatV(fmt, args);
    va_end(args);
    char *const owned = IniStruct_Dup(ini, temp);
    Mem_Free(temp);
    return owned;
}

// Formats a key line. An empty value gives "key =", with no trailing space.
static char *M_FormatKey(
    const INI *const ini, const char *const key, const char *const value)
{
    return value[0] == '\0' ? M_Format(ini, "%s =", key)
                            : M_Format(ini, "%s = %s", key, value);
}

static LINE M_MakeKeyLine(
    const INI *const ini, const char *const section, const char *const key,
    const char *const value)
{
    return (LINE) {
        .kind = LINE_KEY,
        .newline = ini->default_newline,
        .raw = M_FormatKey(ini, key, value),
        .name = IniStruct_Dup(ini, section),
        .key = IniStruct_Dup(ini, key),
        .value = IniStruct_Dup(ini, value),
    };
}

static LINE M_MakeSectionLine(const INI *const ini, const char *const section)
{
    return (LINE) {
        .kind = LINE_SECTION,
        .newline = ini->default_newline,
        .raw = M_Format(ini, "[%s]", section),
        .name = IniStruct_Dup(ini, section),
    };
}

static LINE M_MakeBlankLine(const INI *const ini)
{
    return (LINE) {
        .kind = LINE_OTHER,
        .newline = ini->default_newline,
        .raw = IniStruct_Dup(ini, ""),
    };
}

void Ini_Set(
    INI *const ini, const char *const section, const char *const key,
    const char *const value)
{
    int32_t last_match = -1;
    for (int32_t i = 0; i < ini->lines.count; i++) {
        const LINE *const line = &ini->lines.items[i];
        if (line->kind == LINE_KEY && IniStruct_NamesEqual(line->name, section)
            && IniStruct_NamesEqual(line->key, key)) {
            last_match = i;
        }
    }
    if (last_match != -1) {
        LINE *const line = &ini->lines.items[last_match];
        IniStruct_FreeStr(ini, line->raw);
        IniStruct_FreeStr(ini, line->key);
        IniStruct_FreeStr(ini, line->value);
        line->raw = M_FormatKey(ini, key, value);
        line->key = IniStruct_Dup(ini, key);
        line->value = IniStruct_Dup(ini, value);
        return;
    }

    int32_t section_header = -1;
    const bool implicit_section = section[0] == '\0';
    if (!implicit_section) {
        for (int32_t i = 0; i < ini->lines.count; i++) {
            const LINE *const line = &ini->lines.items[i];
            if (line->kind == LINE_SECTION
                && IniStruct_NamesEqual(line->name, section)) {
                section_header = i;
            }
        }
    }

    if (implicit_section || section_header != -1) {
        int32_t last_key = -1;
        for (int32_t i = 0; i < ini->lines.count; i++) {
            const LINE *const line = &ini->lines.items[i];
            if (line->kind == LINE_KEY
                && IniStruct_NamesEqual(line->name, section)) {
                last_key = i;
            }
        }
        int32_t insert_at;
        if (last_key != -1) {
            insert_at = last_key + 1;
        } else if (!implicit_section) {
            insert_at = section_header + 1;
        } else {
            insert_at = 0;
        }
        Array_Insert(
            &ini->lines, insert_at, M_MakeKeyLine(ini, section, key, value));
        return;
    }

    if (ini->lines.count > 0) {
        const LINE *const last = &ini->lines.items[ini->lines.count - 1];
        const bool last_is_blank =
            last->kind == LINE_OTHER && last->raw[0] == '\0';
        if (!last_is_blank) {
            Array_Append(&ini->lines, M_MakeBlankLine(ini));
        }
    }
    Array_Append(&ini->lines, M_MakeSectionLine(ini, section));
    Array_Append(&ini->lines, M_MakeKeyLine(ini, section, key, value));
}

void Ini_Remove(
    INI *const ini, const char *const section, const char *const key)
{
    for (int32_t i = ini->lines.count - 1; i >= 0; i--) {
        LINE *const line = &ini->lines.items[i];
        if (line->kind == LINE_KEY && IniStruct_NamesEqual(line->name, section)
            && IniStruct_NamesEqual(line->key, key)) {
            IniStruct_FreeLine(ini, line);
            Array_OrderedRemove(&ini->lines, i);
        }
    }
}

void Ini_MergeChanges(
    INI *const target, const INI *const base, const INI *const changed)
{
    for (int32_t i = 0; i < changed->lines.count; i++) {
        const LINE *const line = &changed->lines.items[i];
        if (line->kind != LINE_KEY) {
            continue;
        }
        const char *const value = Ini_Get(changed, line->name, line->key);
        const char *const old = Ini_Get(base, line->name, line->key);
        if (old == nullptr || strcmp(old, value) != 0
            || Ini_Get(target, line->name, line->key) == nullptr) {
            Ini_Set(target, line->name, line->key, value);
        }
    }
}
