#include "core/ini/struct.h"

#include "core/base/string.h"

#include <string.h>

char *IniStruct_Dup(const INI *const ini, const char *const text)
{
    const size_t size = strlen(text) + 1;
    char *const copy = Allocator_Alloc(ini->allocator, size);
    memcpy(copy, text, size);
    return copy;
}

void IniStruct_FreeStr(const INI *const ini, char *const text)
{
    if (text != nullptr) {
        Allocator_Free(ini->allocator, text, strlen(text) + 1);
    }
}

void IniStruct_FreeLine(const INI *const ini, LINE *const line)
{
    IniStruct_FreeStr(ini, line->raw);
    IniStruct_FreeStr(ini, line->name);
    IniStruct_FreeStr(ini, line->key);
    IniStruct_FreeStr(ini, line->value);
}

bool IniStruct_NamesEqual(const char *const a, const char *const b)
{
    return Str_EqualIgnoreCase(a, b);
}

const char *Ini_Get(
    const INI *const ini, const char *const section, const char *const key)
{
    const char *value = nullptr;
    for (int32_t i = 0; i < ini->lines.count; i++) {
        const LINE *const line = &ini->lines.items[i];
        if (line->kind == LINE_KEY && IniStruct_NamesEqual(line->name, section)
            && IniStruct_NamesEqual(line->key, key)) {
            value = line->value;
        }
    }
    return value;
}

void Ini_Free(INI *const ini)
{
    if (ini == nullptr) {
        return;
    }
    for (int32_t i = 0; i < ini->lines.count; i++) {
        IniStruct_FreeLine(ini, &ini->lines.items[i]);
    }
    const ALLOCATOR *const allocator = ini->allocator;
    Array_Free(&ini->lines);
    Allocator_Free(allocator, ini, sizeof(*ini));
}
