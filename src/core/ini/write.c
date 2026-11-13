// SPDX-License-Identifier: GPL-3.0-only

#include "core/ini/struct.h"

#include <string.h>

static const char *M_NewlineText(const NEWLINE_KIND newline)
{
    switch (newline) {
    case NEWLINE_CRLF:
        return "\r\n";
    case NEWLINE_NONE:
        return "";
    case NEWLINE_LF:
        return "\n";
    }
    return "\n";
}

void Ini_Write(const INI *const ini, WRITER *const writer)
{
    for (int32_t i = 0; i < ini->lines.count; i++) {
        const LINE *const line = &ini->lines.items[i];
        Writer_WriteBytes(writer, line->raw, strlen(line->raw));
        NEWLINE_KIND newline = line->newline;
        if (newline == NEWLINE_NONE && i + 1 < ini->lines.count) {
            newline = ini->default_newline;
        }
        const char *const text = M_NewlineText(newline);
        Writer_WriteBytes(writer, text, strlen(text));
    }
}

RESULT Ini_Save(const INI *const ini, const char *const path)
{
    WRITER writer;
    MUST(Writer_OpenFile(&writer, path));
    Ini_Write(ini, &writer);
    return Writer_Close(&writer);
}
