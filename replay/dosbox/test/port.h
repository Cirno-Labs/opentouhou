#pragma once

#include "replay/dosbox/dosbox.h"

// Describes the test game dostest, FRAMES.COM of frames.asm, to the
// harness and to the dosbox backend.

#define DOSTEST_PROGRAM "FRAMES.COM"
#define DOSTEST_PROGRAM_SHA1 "55537397c2be00a5289810a8b04018a2c44ac791"

extern const HARNESS_GAME g_DostestHarness;
extern const DOSBOX_PORT g_DostestPort;
