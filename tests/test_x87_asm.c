// SPDX-License-Identifier: GPL-3.0-only

// Checks core/math/x87 against the real x87 instructions, run at 24-bit
// precision through inline asm, on x86 with a compiler that supports GCC's
// inline asm syntax. On every other target, this test file compiles to an
// empty program that passes trivially (see the #else branch at the end).
//
// By default this runs a few thousand points per function, which stays
// within a few seconds. Set OTM_X87_SWEEP_COUNT to a larger count (for
// example a few million) for a slow, thorough sweep across the whole
// range the games can produce; this is meant to be run by hand, not as
// part of `just test`.

#include "test.h"

#if (defined(__x86_64__) || defined(__i386__))                                 \
    && (defined(__GNUC__) || defined(__clang__))

    #include "core/math/x87.h"

    #include <math.h>
    #include <stdint.h>
    #include <stdlib.h>
    #include <string.h>

    // The x87 control word for 24-bit precision, round-to-nearest-even, with
    // every exception masked: Direct3D 8's mode, and the mode every function
    // in core/math/x87 is written against.
    #define PC24_CW 0x007f
    // PC24_CW with the rounding control switched to truncate (round toward
    // zero), which _ftol sets before its fistp and restores afterward.
    #define PC24_TRUNC_CW 0x0f7f

static uint16_t M_GetControlWord(void)
{
    uint16_t cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}

static void M_SetControlWord(const uint16_t cw)
{
    __asm__ volatile("fldcw %0" : : "m"(cw));
}

static float M_RealSin(const float x)
{
    const uint16_t saved = M_GetControlWord();
    const uint16_t cw = PC24_CW;
    M_SetControlWord(cw);
    float result;
    __asm__ volatile(
        "flds %1\n\t"
        "fsin\n\t"
        "fstps %0"
        : "=m"(result)
        : "m"(x));
    M_SetControlWord(saved);
    return result;
}

static float M_RealCos(const float x)
{
    const uint16_t saved = M_GetControlWord();
    const uint16_t cw = PC24_CW;
    M_SetControlWord(cw);
    float result;
    __asm__ volatile(
        "flds %1\n\t"
        "fcos\n\t"
        "fstps %0"
        : "=m"(result)
        : "m"(x));
    M_SetControlWord(saved);
    return result;
}

static float M_RealTan(const float x)
{
    const uint16_t saved = M_GetControlWord();
    const uint16_t cw = PC24_CW;
    M_SetControlWord(cw);
    float result;
    __asm__ volatile(
        "flds %1\n\t"
        "fptan\n\t"
        "fstp %%st(0)\n\t" // discards the 1.0 fptan pushes
        "fstps %0"
        : "=m"(result)
        : "m"(x));
    M_SetControlWord(saved);
    return result;
}

static float M_RealAtan2(const float y, const float x)
{
    const uint16_t saved = M_GetControlWord();
    const uint16_t cw = PC24_CW;
    M_SetControlWord(cw);
    float result;
    __asm__ volatile(
        "flds %1\n\t" // st(0) = y
        "flds %2\n\t" // st(0) = x, st(1) = y
        "fpatan\n\t" // st(0) = atan(st(1)/st(0)) = atan2(y, x)
        "fstps %0"
        : "=m"(result)
        : "m"(y), "m"(x));
    M_SetControlWord(saved);
    return result;
}

static void M_RealSinCos(const float angle, float *const sin, float *const cos)
{
    const uint16_t saved = M_GetControlWord();
    const uint16_t cw = PC24_CW;
    M_SetControlWord(cw);
    __asm__ volatile(
        "flds %2\n\t"
        "fsincos\n\t" // st(1) = sin(angle), st(0) = cos(angle)
        "fstps %0\n\t" // pop cos
        "fstps %1" // pop sin
        : "=m"(*cos), "=m"(*sin)
        : "m"(angle));
    M_SetControlWord(saved);
}

static void M_RealSinCosScale(
    const float angle, const float radius, float *const x, float *const y)
{
    const uint16_t saved = M_GetControlWord();
    const uint16_t cw = PC24_CW;
    M_SetControlWord(cw);
    // fsincos leaves sin and cos at extended precision; each is multiplied
    // by radius and rounded to float only once, by its own fstps, which is
    // what X87_SinCosScale's double-rounding caveat describes.
    __asm__ volatile(
        "flds %2\n\t" // st(0) = angle
        "fsincos\n\t" // st(1) = sin, st(0) = cos
        "fld %%st(0)\n\t" // st(0) = cos (copy), st(1) = cos, st(2) = sin
        "fmuls %3\n\t" // st(0) = cos * radius
        "fstps %0\n\t" // pop into *x; st(0) = cos, st(1) = sin
        "fstp %%st(0)\n\t" // discard the original cos
        "fmuls %3\n\t" // st(0) = sin * radius
        "fstps %1"
        : "=m"(*x), "=m"(*y)
        : "m"(angle), "m"(radius));
    M_SetControlWord(saved);
}

static float M_RealRint(const float x)
{
    const uint16_t saved = M_GetControlWord();
    const uint16_t cw = PC24_CW;
    M_SetControlWord(cw);
    float result;
    __asm__ volatile(
        "flds %1\n\t"
        "frndint\n\t"
        "fstps %0"
        : "=m"(result)
        : "m"(x));
    M_SetControlWord(saved);
    return result;
}

static int32_t M_RealFtol(const float x)
{
    const uint16_t saved = M_GetControlWord();
    const uint16_t cw = PC24_TRUNC_CW;
    M_SetControlWord(cw);
    int32_t result;
    __asm__ volatile(
        "flds %1\n\t"
        "fistpl %0"
        : "=m"(result)
        : "m"(x));
    M_SetControlWord(saved);
    return result;
}

static float M_RealSqrt(const float x)
{
    const uint16_t saved = M_GetControlWord();
    const uint16_t cw = PC24_CW;
    M_SetControlWord(cw);
    float result;
    __asm__ volatile(
        "flds %1\n\t"
        "fsqrt\n\t"
        "fstps %0"
        : "=m"(result)
        : "m"(x));
    M_SetControlWord(saved);
    return result;
}

// Bits-equal comparison, since NaN != NaN with == and we want the exact
// result, not an approximation.
static bool M_SameBits(const float a, const float b)
{
    uint32_t ua, ub;
    memcpy(&ua, &a, sizeof(ua));
    memcpy(&ub, &b, sizeof(ub));
    return ua == ub;
}

typedef struct {
    const char *name;
    int64_t mismatches;
    int64_t total;
} SWEEP_RESULT;

static void M_Report(const SWEEP_RESULT *const r)
{
    printf(
        "    %s: %lld/%lld mismatches\n", r->name, (long long)r->mismatches,
        (long long)r->total);
}

static int64_t M_SweepCount(void)
{
    const char *const env = getenv("OTM_X87_SWEEP_COUNT");
    if (env == nullptr) {
        return 4000;
    }
    return strtoll(env, nullptr, 10);
}

TEST(trig_functions_match_the_real_x87_over_game_ranges)
{
    const int64_t count = M_SweepCount();
    const double range = 8.0 * 3.14159265358979323846;
    SWEEP_RESULT sin_r = { "Sin", 0, 0 };
    SWEEP_RESULT cos_r = { "Cos", 0, 0 };
    SWEEP_RESULT tan_r = { "Tan", 0, 0 };
    SWEEP_RESULT atan2_r = { "Atan2", 0, 0 };
    SWEEP_RESULT sincos_r = { "SinCos", 0, 0 };
    SWEEP_RESULT scale_r = { "SinCosScale", 0, 0 };

    for (int64_t i = 0; i < count; i++) {
        const double t = -range + 2.0 * range * (double)i / (double)(count - 1);
        const float x = (float)t;

        sin_r.total++;
        if (!M_SameBits(X87_Sin(x), M_RealSin(x))) {
            sin_r.mismatches++;
        }
        cos_r.total++;
        if (!M_SameBits(X87_Cos(x), M_RealCos(x))) {
            cos_r.mismatches++;
        }
        tan_r.total++;
        if (!M_SameBits(X87_Tan(x), M_RealTan(x))) {
            tan_r.mismatches++;
        }

        const float y = (float)(-range
                                + 2.0 * range * (double)(count - 1 - i)
                                    / (double)(count - 1));
        atan2_r.total++;
        if (!M_SameBits(X87_Atan2(y, x), M_RealAtan2(y, x))) {
            atan2_r.mismatches++;
        }

        float es, ec, as, ac;
        X87_SinCos(x, &es, &ec);
        M_RealSinCos(x, &as, &ac);
        sincos_r.total++;
        if (!M_SameBits(es, as) || !M_SameBits(ec, ac)) {
            sincos_r.mismatches++;
        }

        float ex, ey, ax, ay;
        X87_SinCosScale(x, y, &ex, &ey);
        M_RealSinCosScale(x, y, &ax, &ay);
        scale_r.total++;
        if (!M_SameBits(ex, ax) || !M_SameBits(ey, ay)) {
            scale_r.mismatches++;
        }
    }

    M_Report(&sin_r);
    M_Report(&cos_r);
    M_Report(&tan_r);
    M_Report(&atan2_r);
    M_Report(&sincos_r);
    M_Report(&scale_r);

    // The vendored libm and the x87's own argument reduction can land on
    // different sides of a half-ulp boundary; this is near 0, not exactly
    // 0. Anything beyond a handful out of thousands would mean a real bug.
    CHECK(sin_r.mismatches <= sin_r.total / 100);
    CHECK(cos_r.mismatches <= cos_r.total / 100);
    CHECK(tan_r.mismatches <= tan_r.total / 100);
    CHECK(atan2_r.mismatches <= atan2_r.total / 100);
    CHECK(sincos_r.mismatches <= sincos_r.total / 100);
    CHECK(scale_r.mismatches <= scale_r.total / 100);
}

TEST(rint_ftol_and_sqrt_match_the_real_x87_exactly)
{
    const float edge_cases[] = {
        0.0f,  -0.0f,  0.5f,       1.5f,        2.5f,          -0.5f,
        -1.5f, -2.5f,  8388608.0f, -8388608.0f, 2147483648.0f, -2147483649.0f,
        1e30f, -1e30f, INFINITY,   -INFINITY,   nanf(""),
    };
    int64_t rint_mismatches = 0, ftol_mismatches = 0, sqrt_mismatches = 0;
    const int64_t edge_count =
        (int64_t)(sizeof(edge_cases) / sizeof(edge_cases[0]));

    for (int64_t i = 0; i < edge_count; i++) {
        const float x = edge_cases[i];
        if (!M_SameBits(X87_Rint(x), M_RealRint(x))) {
            rint_mismatches++;
        }
        if (X87_Ftol(x) != M_RealFtol(x)) {
            ftol_mismatches++;
        }
        if (x >= 0.0f && !M_SameBits(X87_Sqrt(x), M_RealSqrt(x))) {
            sqrt_mismatches++;
        }
    }

    const int64_t count = M_SweepCount();
    for (int64_t i = 0; i < count; i++) {
        const float x =
            (float)(-1.0e6 + 2.0e6 * (double)i / (double)(count - 1));
        if (!M_SameBits(X87_Rint(x), M_RealRint(x))) {
            rint_mismatches++;
        }
        if (X87_Ftol(x) != M_RealFtol(x)) {
            ftol_mismatches++;
        }
        if (x >= 0.0f && !M_SameBits(X87_Sqrt(x), M_RealSqrt(x))) {
            sqrt_mismatches++;
        }
    }

    printf("    Rint: %lld mismatches\n", (long long)rint_mismatches);
    printf("    Ftol: %lld mismatches\n", (long long)ftol_mismatches);
    printf("    Sqrt: %lld mismatches\n", (long long)sqrt_mismatches);
    CHECK_EQ_INT(rint_mismatches, 0);
    CHECK_EQ_INT(ftol_mismatches, 0);
    CHECK_EQ_INT(sqrt_mismatches, 0);
}

#else

TEST(skipped_outside_x86_with_gcc_or_clang_inline_asm)
{
}

#endif
