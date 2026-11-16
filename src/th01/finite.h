#pragma once

#include "core/base/types.h"

// Represents a finite signed value as significand * 2^exponent. Nonzero
// significands have bit 63 set. Arithmetic rounds to 64 significant bits,
// nearest with ties to even, before a separate binary64 store. This models
// TH01 scalar arithmetic without depending on the host's long double.
// Supports finite binary64 operands and results within the binary64 range;
// excludes NaNs, infinities, exceptions and floating-point environment state.
typedef struct {
    uint64_t significand;
    int32_t exponent;
    bool negative;
} FINITE;

FINITE Finite_FromDouble(double value);
FINITE Finite_Add(FINITE a, FINITE b);
FINITE Finite_Multiply(FINITE a, FINITE b);
FINITE Finite_Negate(FINITE value);

// Divides by two exactly, without a binary64 store.
FINITE Finite_Half(FINITE value);
double Finite_ToDouble(FINITE value);

// Truncates toward zero, then wraps to signed 16 bits. Requires a magnitude
// below 2^63, the original integer conversion helper's finite domain.
int16_t Finite_ToPixel(FINITE value);
