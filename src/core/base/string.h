#pragma once

#include "core/base/memory.h"
#include "core/base/utils.h"

#include <stdarg.h>
#include <stdint.h>

// Helpers for NUL-terminated UTF-8 strings. The functions that return a new
// string allocate it with core/base/memory; Mem_Free releases it.

// Returns a copy of text.
char *Str_Dup(const char *text);

// Returns a copy of text from allocator; Allocator_Free(allocator, copy,
// strlen(copy) + 1) releases it.
char *Str_DupWith(const ALLOCATOR *allocator, const char *text);

// Returns a new string formatted as printf formats it.
PRINTF_FORMAT(1, 2)
char *Str_Format(const char *fmt, ...);
PRINTF_FORMAT(1, 0)
char *Str_FormatV(const char *fmt, va_list args);

bool Str_StartsWith(const char *text, const char *prefix);
bool Str_EndsWith(const char *text, const char *suffix);

// Compares two strings, treating ASCII letters of either case as equal.
// Other bytes compare exactly.
bool Str_EqualIgnoreCase(const char *a, const char *b);

// Reads text, a whole decimal number, into *out. Fails on an empty text,
// a byte that is not a digit, or a number past UINT32_MAX, and then leaves
// *out unchanged.
bool Str_ParseU32(const char *text, uint32_t *out);
