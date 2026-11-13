// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// Separates a fake program of fake_program.h from the exit status and the
// output that follow it: the marker, the status in decimal, a newline,
// then the output up to the end of the file.
#define FAKE_PROGRAM_MARKER "\n#fake-program-output\n"
