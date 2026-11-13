// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// Marks a function that takes a printf-style format at parameter fmt_ and its
// arguments from parameter args_, so that the compiler checks them. MinGW's
// GCC reads plain printf as Microsoft's format syntax, which lacks %zu, so
// MinGW gets the C99 syntax that its own printf family implements.
#if defined(__MINGW32__)
    #define PRINTF_FORMAT(fmt_, args_) [[gnu::format(gnu_printf, fmt_, args_)]]
#else
    #define PRINTF_FORMAT(fmt_, args_) [[gnu::format(printf, fmt_, args_)]]
#endif

// Returns the number of elements of an array. The argument must be an array,
// not a pointer.
#define ARRAY_SIZE(array_) (sizeof(array_) / sizeof((array_)[0]))

// Return the smaller and the larger of two values. Each argument is evaluated
// twice.
#define MIN(a_, b_) ((a_) <= (b_) ? (a_) : (b_))
#define MAX(a_, b_) ((a_) >= (b_) ? (a_) : (b_))
