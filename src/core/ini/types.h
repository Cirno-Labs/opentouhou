// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// Reads and writes INI files: our own settings format, one file per game in
// its user directory, such as "opentouhou/th06/config.ini". Several parts
// read and write sections of the same file (core's [video] and [audio],
// th06's [th06], core/input's [input.keyboard]); the app loads the file
// once, each part gets and sets its own keys, and the app saves once.
//
// Dialect:
// - "[section]" starts a section; "key = value" sets a key in the current
//   section, or in the section named "" before the first header.
// - ';' and '#' start a comment that runs to the end of the line.
// - The text is UTF-8. A byte order mark at the start is skipped.
// - Whitespace around a section name, a key and a value is trimmed. Values
//   have no quotes and no escapes, and cannot span more than one line.
// - Section and key names compare without regard to the case of ASCII
//   letters.
// - A key that appears more than once keeps every line; Ini_Get returns the
//   value of the last one.
//
// A line that is none of these is kept as it is and never fails parsing: it
// is written back unchanged, and Ini_Parse logs it as a warning with its
// line number. Parsing fails only on invalid UTF-8.
//
// An INI keeps every line of the text it was built from, so Ini_Write of an
// INI nobody has changed reproduces that text exactly, including comments,
// blank lines, spacing and the order of sections and keys. The only
// exception is the final line break: a file is written with one newline per
// line, even one that originally had none at its end, the moment anything
// is appended after it. Every line keeps its own line ending (LF or CRLF),
// and a line added by Ini_Set uses the ending already in use in the file,
// or LF for a file with none.

typedef struct INI INI;

// Releases ini and everything it owns.
void Ini_Free(INI *ini);
