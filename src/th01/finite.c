#include "th01/finite.h"

#include <string.h>

typedef struct {
    uint64_t hi, lo;
} WIDE;

static WIDE M_Add(const WIDE a, const WIDE b)
{
    const uint64_t lo = a.lo + b.lo;
    return (WIDE) { a.hi + b.hi + (lo < a.lo), lo };
}

static WIDE M_Subtract(const WIDE a, const WIDE b)
{
    return (WIDE) { a.hi - b.hi - (a.lo < b.lo), a.lo - b.lo };
}

static WIDE M_Right(const WIDE value, const int32_t shift)
{
    if (shift == 0) {
        return value;
    }
    if (shift >= 128) {
        return (WIDE) { 0, (value.hi | value.lo) != 0 };
    }
    if (shift >= 64) {
        const int32_t high_shift = shift - 64;
        const uint64_t lost =
            high_shift == 0 ? 0 : value.hi << (64 - high_shift);
        return (WIDE) { 0,
                        (value.hi >> high_shift) | ((lost | value.lo) != 0) };
    }
    return (WIDE) { value.hi >> shift,
                    (value.lo >> shift) | (value.hi << (64 - shift))
                        | ((value.lo << (64 - shift)) != 0) };
}

static FINITE M_Round(WIDE value, int32_t exponent, const bool negative)
{
    if ((value.hi | value.lo) == 0) {
        return (FINITE) { 0, 0, negative };
    }
    // Keep three low rounding bits below a normalized 64-bit significand.
    while (value.hi > 7) {
        value = M_Right(value, 1);
        exponent++;
    }
    while (value.hi < 4) {
        value.hi = (value.hi << 1) | (value.lo >> 63);
        value.lo <<= 1;
        exponent--;
    }
    uint64_t significand = (value.hi << 61) | (value.lo >> 3);
    const uint64_t remainder = value.lo & 7;
    if (remainder > 4 || (remainder == 4 && (significand & 1) != 0)) {
        significand++;
        if (significand == 0) {
            significand = UINT64_C(1) << 63;
            exponent++;
        }
    }
    return (FINITE) { significand, exponent + 3, negative };
}

FINITE Finite_FromDouble(const double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    const bool negative = (bits >> 63) != 0;
    const int32_t encoded_exponent = (int32_t)((bits >> 52) & 0x7ff);
    uint64_t significand = bits & UINT64_C(0xfffffffffffff);
    int32_t exponent = -1074;
    if (encoded_exponent != 0) {
        significand |= UINT64_C(1) << 52;
        exponent = encoded_exponent - 1075;
    }
    if (significand != 0) {
        while ((significand >> 63) == 0) {
            significand <<= 1;
            exponent--;
        }
    }
    return (FINITE) { significand, exponent, negative };
}

FINITE Finite_Add(FINITE a, FINITE b)
{
    if (a.significand == 0 && b.significand == 0) {
        return (FINITE) { 0, 0, a.negative && b.negative };
    }
    if (a.significand == 0) {
        return b;
    }
    if (b.significand == 0) {
        return a;
    }
    if (a.exponent < b.exponent
        || (a.exponent == b.exponent && a.significand < b.significand)) {
        const FINITE swap = a;
        a = b;
        b = swap;
    }
    const WIDE large = { a.significand >> 61, a.significand << 3 };
    const WIDE small = M_Right(
        (WIDE) { b.significand >> 61, b.significand << 3 },
        a.exponent - b.exponent);
    const WIDE result = a.negative == b.negative ? M_Add(large, small)
                                                 : M_Subtract(large, small);
    return M_Round(
        result, a.exponent - 3, (result.hi | result.lo) != 0 && a.negative);
}

FINITE Finite_Multiply(const FINITE a, const FINITE b)
{
    const uint64_t a0 = (uint32_t)a.significand;
    const uint64_t a1 = a.significand >> 32;
    const uint64_t b0 = (uint32_t)b.significand;
    const uint64_t b1 = b.significand >> 32;
    const uint64_t p0 = a0 * b0;
    const uint64_t p1 = a0 * b1;
    const uint64_t p2 = a1 * b0;
    WIDE product = { a1 * b1, p0 };
    product = M_Add(product, (WIDE) { p1 >> 32, p1 << 32 });
    product = M_Add(product, (WIDE) { p2 >> 32, p2 << 32 });
    return M_Round(
        M_Right(product, 61), a.exponent + b.exponent + 61,
        a.negative != b.negative);
}

FINITE Finite_Negate(FINITE value)
{
    value.negative = !value.negative;
    return value;
}

FINITE Finite_Half(FINITE value)
{
    value.exponent--;
    return value;
}

double Finite_ToDouble(const FINITE value)
{
    const int32_t shift = value.exponent < -1085 ? -1074 - value.exponent : 11;
    uint64_t significand = 0;
    if (shift < 65) {
        if (shift == 64) {
            significand = value.significand > (UINT64_C(1) << 63);
        } else {
            significand = value.significand >> shift;
            const uint64_t mask = (UINT64_C(1) << shift) - 1;
            const uint64_t remainder = value.significand & mask;
            const uint64_t half = UINT64_C(1) << (shift - 1);
            significand += remainder > half
                || (remainder == half && (significand & 1) != 0);
        }
    }
    int32_t exponent = value.exponent + shift + 1075;
    if (significand >= (UINT64_C(1) << 53)) {
        significand >>= 1;
        exponent++;
    }
    uint64_t bits = (uint64_t)value.negative << 63;
    if (significand != 0) {
        if (significand >= (UINT64_C(1) << 52)) {
            bits |= (uint64_t)exponent << 52;
            significand &= UINT64_C(0xfffffffffffff);
        }
        bits |= significand;
    }
    double result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

int16_t Finite_ToPixel(const FINITE value)
{
    uint64_t magnitude = 0;
    if (value.exponent > -64 && value.exponent < 0) {
        magnitude = value.significand >> -value.exponent;
    }
    const uint32_t low =
        (uint32_t)(value.negative ? -magnitude : magnitude) & 0xffff;
    return (int16_t)(low >= 0x8000 ? (int32_t)low - 0x10000 : (int32_t)low);
}
