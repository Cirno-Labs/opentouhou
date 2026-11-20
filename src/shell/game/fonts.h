#pragma once

#include "core/base/result.h"
#include "core/vfs/types.h"

// Adds the fonts that a game opens by family name to the font registry
// (core/text/font_registry.h). The built-in fonts are the files below
// src/shell/builtin that a game lists in its meson.build, which the build
// packs into a zip in that game's programs; the meson option font chooses
// whether they carry them. Only shell/game and the
// replay harness's native backend, which stands in for it, use this.

// Forgets every added font, as FontRegistry_Reset does, and adds the
// built-in fonts.
RESULT ShellFonts_Reset(bool use_system);

// Adds each .ttf, .ttc, .otf and .otc file directly below dir in files, in
// byte order of their names, as a font, and each .bdf, .bmp and .rom file
// as a font file under its name. Logs a file that does not add as a warning
// and leaves it out. A missing dir adds nothing.
void ShellFonts_AddDir(VFS *files, const char *dir);
