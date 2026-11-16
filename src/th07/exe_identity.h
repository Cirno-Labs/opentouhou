#pragma once

#include <stdint.h>

// Identify retail th07.exe 1.00b, so its saves and score file carry the
// size and checksum (unsorted433e90.cpp, ChecksumExecutable) ZUN's exe
// checks before accepting them. Never checked on read.
#define TH07_EXE_SIZE ((uint32_t)650752)
#define TH07_EXE_CHECKSUM ((uint32_t)0xaec5445c)
