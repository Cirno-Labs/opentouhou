// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/types.h"
#include "core/patch/stack.h"

// Translates the strings of an original executable as thcrap does.
// stringlocs.js in the game directory of a patch maps an address in the
// executable to a string id, and stringdefs.js in the patch directory maps
// a string id to its text. The files of every patch merge as Patch_LoadJson
// merges them. A stringlocs key "Rx<hex>", in any case, is an address
// relative to the image base; a key "0x<hex>" is absolute. A key of any
// other form is left out, with a log line.
//
// Example:
// PATCH_STRINGS *const strings = Patch_LoadStrings(stack, 0x400000);
// const char *const text = Patch_TranslateAt(strings, 0x46a98c, "♪%s");
// ...
// Patch_FreeStrings(strings);

typedef struct PATCH_STRINGS PATCH_STRINGS;

// Loads the string tables of the stack. A nullptr stack gives empty tables.
PATCH_STRINGS *Patch_LoadStrings(const PATCH_STACK *stack, uint32_t image_base);

// Releases the tables and every translation they returned. A nullptr
// strings does nothing.
void Patch_FreeStrings(PATCH_STRINGS *strings);

// Returns the translation of the string at the absolute address, or
// original when no patch translates it. A translation whose conversions
// differ from those of original is rejected, with a log line, and original
// is returned, so that a printf format keeps the arguments it takes. A
// nullptr strings returns original.
const char *Patch_TranslateAt(
    const PATCH_STRINGS *strings, uint32_t address, const char *original);

// Returns the translation of the string id from stringdefs.js, or original,
// with the same check as Patch_TranslateAt.
const char *Patch_Translate(
    const PATCH_STRINGS *strings, const char *id, const char *original);

// Returns whether two printf formats take the same arguments: the same
// conversions in the same order, each with the same length modifier and the
// same number of '*' fields. Flags, widths, precisions and the text between
// the conversions may differ, and "%%" is text. A format with a malformed
// conversion matches only a format identical to it.
bool Patch_SameConversions(const char *a, const char *b);

// Returns text as plain text: thcrap's layout markup "<cmd$text$...>"
// becomes its text, or nothing for a command with 's', and a translator
// note, from U+0014 to the end, is cut off. Markup without its closing '>'
// stays as it is. Mem_Free releases the result.
char *Patch_PlainText(const char *text);
