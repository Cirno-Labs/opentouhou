#include "othpatch/main.h"

#include "core/io/path.h"

#include "version.h"

#include <unistd.h>

int main(const int argc, char **const argv)
{
    FILE *const in = isatty(STDIN_FILENO) ? stdin : nullptr;
    return Main_Run(argc, argv, OTHPATCH_VERSION, Path_GetExeDir(), in, stdout);
}
