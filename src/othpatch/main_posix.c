#include "othpatch/main.h"

#include "core/io/path.h"

#include "version.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Returns how stdout takes progress: in place with ANSI escape sequences on
// a terminal, with \r alone on a terminal that $TERM does not name or names
// "dumb", and as plain lines elsewhere.
static MAIN_OUTPUT M_GetOutput(void)
{
    if (!isatty(STDOUT_FILENO)) {
        return MAIN_OUTPUT_FILE;
    }
    const char *const term = getenv("TERM");
    return term == nullptr || term[0] == '\0' || strcmp(term, "dumb") == 0
        ? MAIN_OUTPUT_TERMINAL
        : MAIN_OUTPUT_ANSI;
}

int main(const int argc, char **const argv)
{
    FILE *const in = isatty(STDIN_FILENO) ? stdin : nullptr;
    const MAIN_OUTPUT output = M_GetOutput();
    return Main_Run(
        argc, argv, OTHPATCH_VERSION, Path_GetExeDir(), in, stdout, output);
}
