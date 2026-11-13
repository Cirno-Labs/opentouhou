#pragma once

#include "core/ini/types.h"

// Returns the value of key in section, or nullptr when the section or the
// key does not exist. The name comparisons ignore the case of ASCII
// letters. The returned pointer is owned by ini and stays valid until the
// key's value changes or ini is freed.
const char *Ini_Get(const INI *ini, const char *section, const char *key);

// Sets the value of key in section to a copy of value. An existing key's
// line is rewritten as "key = value"; a new key is added after the last key
// of its section, and a new section is added at the end of the file,
// preceded by a blank line when the file does not already end with one.
void Ini_Set(INI *ini, const char *section, const char *key, const char *value);

// Removes every line that sets key in section. Removing the section's last
// key leaves its header in place.
void Ini_Remove(INI *ini, const char *section, const char *key);

// Sets in target every key of changed whose value differs from its value in
// base, and every key of changed that target lacks, as Ini_Set sets it.
// Every other line of target stays as it is. A key that appears more than
// once counts with its last value.
void Ini_MergeChanges(INI *target, const INI *base, const INI *changed);
