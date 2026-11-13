// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/result.h"
#include "core/ini/types.h"
#include "core/io/writer.h"

// Writes ini to path, atomically, through Writer_OpenFile.
RESULT Ini_Save(const INI *ini, const char *path);

// Writes every line of ini to writer, in order.
void Ini_Write(const INI *ini, WRITER *writer);
