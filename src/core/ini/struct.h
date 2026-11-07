#pragma once

#include "core/base/array.h"
#include "core/base/memory.h"
#include "core/ini.h"

// The representation of an INI, shared by the ini/ source files. It is not
// part of the public API: core/ini.h keeps INI opaque.

typedef enum {
    // A blank line, a comment, or a line that parsing could not make sense
    // of. raw is kept and written back unchanged.
    LINE_OTHER,
    // A "[section]" line. name holds the section's name.
    LINE_SECTION,
    // A "key = value" line. name holds the section it belongs to, key and
    // value hold the key's name and value.
    LINE_KEY,
} LINE_KIND;

typedef enum {
    NEWLINE_NONE,
    NEWLINE_LF,
    NEWLINE_CRLF,
} NEWLINE_KIND;

typedef struct {
    LINE_KIND kind;
    NEWLINE_KIND newline;
    char *raw;
    char *name;
    char *key;
    char *value;
} LINE;

typedef ARRAY(LINE) LINE_ARRAY;

struct INI {
    const ALLOCATOR *allocator;
    LINE_ARRAY lines;
    // The line ending that a line Ini_Set adds uses, taken from the first
    // line of the text that had one, or LF when none did.
    NEWLINE_KIND default_newline;
};

// Returns a copy of text from ini's allocator. Mem_Free does not release
// it; IniStruct_FreeStr does.
char *IniStruct_Dup(const INI *ini, const char *text);

// Releases a string that IniStruct_Dup returned, or does nothing for
// nullptr.
void IniStruct_FreeStr(const INI *ini, char *text);

// Releases the strings owned by a line, but not the line itself.
void IniStruct_FreeLine(const INI *ini, LINE *line);

// Returns whether a and b are the same section or key name, ignoring the
// case of ASCII letters.
bool IniStruct_NamesEqual(const char *a, const char *b);
