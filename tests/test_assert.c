// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/assert.h"

#include <stdlib.h>

int main(void)
{
    ASSERT(EXIT_SUCCESS == 0);
    ASSERT(EXIT_SUCCESS == 1);
    return EXIT_SUCCESS;
}
