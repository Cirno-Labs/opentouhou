#include "core/ini/struct.h"

#include "core/base/log.h"
#include "core/io/file.h"
#include "core/io/reader.h"

#include <string.h>

// Returns a copy of the size bytes at text from ini's allocator, NUL
// terminated.
static char *M_DupRange(
    const INI *const ini, const char *const text, const size_t size)
{
    char *const copy = Allocator_Alloc(ini->allocator, size + 1);
    memcpy(copy, text, size);
    copy[size] = '\0';
    return copy;
}

// Checks that text[0..size) is well-formed UTF-8.
static RESULT M_ValidateUtf8(const char *const text, const size_t size)
{
    size_t i = 0;
    while (i < size) {
        const uint8_t lead = (uint8_t)text[i];
        size_t length;
        uint32_t min_code_point;
        uint8_t lead_mask;
        if (lead < 0x80) {
            i++;
            continue;
        } else if ((lead & 0xE0) == 0xC0) {
            length = 2;
            min_code_point = 0x80;
            lead_mask = 0x1F;
        } else if ((lead & 0xF0) == 0xE0) {
            length = 3;
            min_code_point = 0x800;
            lead_mask = 0x0F;
        } else if ((lead & 0xF8) == 0xF0) {
            length = 4;
            min_code_point = 0x10000;
            lead_mask = 0x07;
        } else {
            return FAIL("invalid UTF-8 at byte %zu", i);
        }
        FAIL_IF(i + length > size, "invalid UTF-8 at byte %zu", i);
        uint32_t code_point = lead & lead_mask;
        for (size_t j = 1; j < length; j++) {
            const uint8_t cont = (uint8_t)text[i + j];
            FAIL_IF((cont & 0xC0) != 0x80, "invalid UTF-8 at byte %zu", i);
            code_point = (code_point << 6) | (cont & 0x3F);
        }
        const bool surrogate = code_point >= 0xD800 && code_point <= 0xDFFF;
        FAIL_IF(
            code_point < min_code_point || code_point > 0x10FFFF || surrogate,
            "invalid UTF-8 at byte %zu", i);
        i += length;
    }
    return OK;
}

// Holds the position inside [start, end) of the trimmed, non-whitespace
// part of a line.
typedef struct {
    size_t start;
    size_t end;
} M_TRIMMED;

static M_TRIMMED M_TrimRange(const char *const text, size_t start, size_t end)
{
    while (start < end && (text[start] == ' ' || text[start] == '\t')) {
        start++;
    }
    while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t')) {
        end--;
    }
    return (M_TRIMMED) { start, end };
}

// Classifies one line of text and fills *line, other than its raw and
// newline fields. line_number is used only for the warnings this logs.
static void M_ClassifyLine(
    const INI *const ini, const char *const text, const size_t line_start,
    const size_t line_end, const int line_number, char **const section,
    LINE *const line)
{
    const M_TRIMMED trimmed = M_TrimRange(text, line_start, line_end);
    if (trimmed.start == trimmed.end) {
        line->kind = LINE_OTHER;
        return;
    }
    const char first = text[trimmed.start];
    if (first == ';' || first == '#') {
        line->kind = LINE_OTHER;
        return;
    }
    if (first == '[') {
        if (text[trimmed.end - 1] == ']') {
            const M_TRIMMED inner =
                M_TrimRange(text, trimmed.start + 1, trimmed.end - 1);
            line->kind = LINE_SECTION;
            line->name =
                M_DupRange(ini, text + inner.start, inner.end - inner.start);
            IniStruct_FreeStr(ini, *section);
            *section = IniStruct_Dup(ini, line->name);
            return;
        }
        LOG_WARNING("line %d: unterminated section header", line_number);
        line->kind = LINE_OTHER;
        return;
    }
    size_t eq = trimmed.start;
    while (eq < trimmed.end && text[eq] != '=') {
        eq++;
    }
    if (eq == trimmed.end) {
        LOG_WARNING("line %d: not a section, a key or a comment", line_number);
        line->kind = LINE_OTHER;
        return;
    }
    const M_TRIMMED key_range = M_TrimRange(text, trimmed.start, eq);
    if (key_range.start == key_range.end) {
        LOG_WARNING("line %d: key is empty", line_number);
        line->kind = LINE_OTHER;
        return;
    }
    const M_TRIMMED value_range = M_TrimRange(text, eq + 1, trimmed.end);
    line->kind = LINE_KEY;
    line->name = IniStruct_Dup(ini, *section);
    line->key = M_DupRange(
        ini, text + key_range.start, key_range.end - key_range.start);
    line->value = M_DupRange(
        ini, text + value_range.start, value_range.end - value_range.start);
}

RESULT Ini_Parse(
    const char *const text, const size_t size, const ALLOCATOR *const allocator,
    INI **const out)
{
    *out = nullptr;
    size_t start = 0;
    if (size >= 3 && (uint8_t)text[0] == 0xEF && (uint8_t)text[1] == 0xBB
        && (uint8_t)text[2] == 0xBF) {
        start = 3;
    }
    MUST(M_ValidateUtf8(text + start, size - start));

    INI *const ini = Allocator_Alloc(allocator, sizeof(*ini));
    *ini = (INI) { .allocator = allocator, .default_newline = NEWLINE_LF };
    Array_Init(&ini->lines, allocator);
    char *section = IniStruct_Dup(ini, "");
    bool found_default_newline = false;
    int line_number = 1;
    size_t i = start;
    while (i < size) {
        const size_t line_start = i;
        size_t newline_at = i;
        while (newline_at < size && text[newline_at] != '\n') {
            newline_at++;
        }
        size_t line_end = newline_at;
        NEWLINE_KIND newline;
        size_t next;
        if (newline_at < size) {
            next = newline_at + 1;
            if (line_end > line_start && text[line_end - 1] == '\r') {
                line_end--;
                newline = NEWLINE_CRLF;
            } else {
                newline = NEWLINE_LF;
            }
        } else {
            next = size;
            newline = NEWLINE_NONE;
        }

        LINE line = { .newline = newline };
        line.raw = M_DupRange(ini, text + line_start, line_end - line_start);
        M_ClassifyLine(
            ini, text, line_start, line_end, line_number, &section, &line);
        Array_Append(&ini->lines, line);

        if (!found_default_newline && newline != NEWLINE_NONE) {
            ini->default_newline = newline;
            found_default_newline = true;
        }
        i = next;
        line_number++;
    }
    IniStruct_FreeStr(ini, section);
    *out = ini;
    return OK;
}

RESULT Ini_Load(
    const char *const path, const ALLOCATOR *const allocator, INI **const out)
{
    *out = nullptr;
    if (!File_Exists(path)) {
        return Ini_Parse("", 0, allocator, out);
    }
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    const size_t size = Reader_Size(&reader);
    void *const data = Reader_ReadAlloc(&reader, size, &g_HeapAllocator);
    RESULT result = Reader_Result(&reader);
    Reader_Close(&reader);
    if (IS_OK(result)) {
        result = Ini_Parse(data, size, allocator, out);
    }
    Allocator_Free(&g_HeapAllocator, data, size);
    return result;
}
