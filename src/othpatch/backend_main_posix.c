#include "othpatch/main.h"

#include "core/io/path.h"

#include <unistd.h>

int main(const int argc, char **const argv)
{
    FILE *const in = isatty(STDIN_FILENO) ? stdin : nullptr;
    return Main_Run(argc, argv, Path_GetExeDir(), in, stdout);
}
