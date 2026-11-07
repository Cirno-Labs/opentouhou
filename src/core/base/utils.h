#pragma once

// Returns the number of elements of an array. The argument must be an array,
// not a pointer.
#define ARRAY_SIZE(array_) (sizeof(array_) / sizeof((array_)[0]))

// Return the smaller and the larger of two values. Each argument is evaluated
// twice.
#define MIN(a_, b_) ((a_) <= (b_) ? (a_) : (b_))
#define MAX(a_, b_) ((a_) >= (b_) ? (a_) : (b_))
