#pragma once

#include "othpatch/release.h"

#include "core/base/arena.h"
#include "core/base/result.h"
#include "core/base/types.h"

// Installs a game and othpatch from a game package, the zip archive of a
// release, into the folder of othpatch, and installs othpatch from its own
// release. Only the game and othpatch are unpacked; the other files of the
// package and the user's files stay as they are.
//
// Each new file or bundle is written beside the old one as "<name>.new"
// first, so that a failure leaves the old one in place. Then it replaces
// the old one in one of two ways:
// - Rename: "<name>.new" is renamed over "<name>".
// - Move aside: "<name>" is renamed to "<name>.old", "<name>.new" is
//   renamed to "<name>", and "<name>.old" is removed. A running program on
//   Windows can be renamed but not removed, so its "<name>.old" stays until
//   Install_RemoveLeftovers runs on the next start.

// Describes the packages of one platform and how its files are replaced.
typedef struct {
    // Holds the last part of the package names: "Linux", "Windows" or
    // "macOS".
    const char *name;
    // Holds the part of a game's name after "o<game>": ".AppImage", ".exe"
    // or ".app".
    const char *game_suffix;
    // Holds the name of othpatch in the package and beside the game.
    const char *othpatch;
    // Tells whether a game is a bundle folder, whose executable is
    // "Contents/MacOS/o<game>" in it.
    bool bundle;
    // Tells whether files are moved aside instead of renamed over.
    bool move_aside;
} INSTALL_PLATFORM;

extern const INSTALL_PLATFORM g_InstallLinux;
extern const INSTALL_PLATFORM g_InstallWindows;
extern const INSTALL_PLATFORM g_InstallMacOs;

// Returns the platform that othpatch was built for.
const INSTALL_PLATFORM *Install_GetPlatform(void);

// Returns the path of the executable of game in dir, such as
// "<dir>/oth06.AppImage" or "<dir>/oth06.app/Contents/MacOS/oth06". The
// string comes from arena.
char *Install_GetGamePath(
    const INSTALL_PLATFORM *platform, const char *dir, const char *game,
    ARENA *arena);

// Installs game and othpatch into dir from package, a zip archive of size
// bytes. The game is "o<game><suffix>" in the package, a file or, for a
// bundle, every entry below the folder "o<game>.app/".
//
// *othpatch_version holds the version of the othpatch in dir. The
// othpatch of the package replaces it only when the version that it
// prints for --version is newer; *othpatch_version then holds that
// version, and *othpatch_replaced is true. When othpatch_version is
// nullptr, as for a development build, othpatch is never replaced.
//
// The programs get the executable bit; the files of a bundle keep the
// modes of the archive. Reports failure when the package lacks the game or
// othpatch, when a bundle entry has an unsafe path or is a symbolic link,
// or when a file cannot be written. A failure of othpatch leaves the new
// game in place.
RESULT Install_Package(
    const INSTALL_PLATFORM *platform, const char *dir, const char *game,
    const uint8_t *package, size_t size, RELEASE_VERSION *othpatch_version,
    ARENA *arena, bool *othpatch_replaced);

// Installs othpatch into dir from data, the size bytes of its executable.
// Reports failure when the file cannot be written.
RESULT Install_Othpatch(
    const INSTALL_PLATFORM *platform, const char *dir, const uint8_t *data,
    size_t size, ARENA *arena);

// Removes the "<name>.old" and "<name>.new" files and folders that
// Install_Package leaves in dir for othpatch and for each game of games.
// Failures are ignored; the next start tries again.
void Install_RemoveLeftovers(
    const INSTALL_PLATFORM *platform, const char *dir, const char *const *games,
    int32_t game_count, ARENA *arena);
