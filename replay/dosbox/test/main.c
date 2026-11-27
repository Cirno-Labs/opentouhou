#include "replay/dosbox/test/port.h"

#include "core/base/utils.h"

// Runs the test game dostest on the dosbox backend as dostestdosbox.

int main(int argc, char **argv)
{
    static const DOSBOX_FILE files[] = {
        { DOSTEST_PROGRAM, DOSTEST_PROGRAM_SHA1 },
    };
    static const char *const commands[] = { DOSTEST_PROGRAM };
    const DOSBOX_PROGRAM program = {
        .files = files,
        .file_count = ARRAY_SIZE(files),
        .commands = commands,
        .command_count = ARRAY_SIZE(commands),
        .library = "dostestdosbox.so",
    };
    return Dosbox_Launch(&program, argc, argv);
}
