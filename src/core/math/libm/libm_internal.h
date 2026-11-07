#pragma once

// Vendored from musl libc (MIT licence), itself from FreeBSD's msun (Sun
// Microsystems' permissive notice): src/internal/libm.h, trimmed to what
// otm_sin, otm_cos, otm_tan and otm_atan2 need.
// Source: https://github.com/kraj/musl, path src/internal/libm.h, master
// branch at commit 7813e9119eb5fe681b6750b8a033a0490bca0941.
//
// Renamed (otm_ prefix) so these symbols never collide with a platform
// libm. The project always builds x86 with SSE2 (never x87), so
// FLT_EVAL_METHOD is 0 and double_t is plain double; this file assumes
// that instead of detecting it.

#include <float.h>
#include <math.h>
#include <stdint.h>

typedef double double_t;

#ifdef __GNUC__
    #define otm_predict_false(x_) __builtin_expect(x_, 0)
#else
    #define otm_predict_false(x_) (x_)
#endif

static inline void otm_ForceEval(double x)
{
    volatile double y = x;
    (void)y;
}
#define OTM_FORCE_EVAL(x_) otm_ForceEval(x_)

#define OTM_ASUINT64(f_)                                                       \
    ((union {                                                                  \
        double f_f;                                                            \
        uint64_t f_i;                                                          \
    }) { f_ })                                                                 \
        .f_i
#define OTM_ASDOUBLE(i_)                                                       \
    ((union {                                                                  \
        uint64_t f_i;                                                          \
        double f_f;                                                            \
    }) { i_ })                                                                 \
        .f_f

#define OTM_EXTRACT_WORDS(hi_, lo_, d_)                                        \
    do {                                                                       \
        uint64_t u_ = OTM_ASUINT64(d_);                                        \
        (hi_) = u_ >> 32;                                                      \
        (lo_) = (uint32_t)u_;                                                  \
    } while (0)

#define OTM_GET_HIGH_WORD(hi_, d_)                                             \
    do {                                                                       \
        (hi_) = OTM_ASUINT64(d_) >> 32;                                        \
    } while (0)

#define OTM_INSERT_WORDS(d_, hi_, lo_)                                         \
    do {                                                                       \
        (d_) = OTM_ASDOUBLE(((uint64_t)(hi_) << 32) | (uint32_t)(lo_));        \
    } while (0)

#define OTM_SET_LOW_WORD(d_, lo_)                                              \
    OTM_INSERT_WORDS(d_, OTM_ASUINT64(d_) >> 32, lo_)

// otm_sin, otm_cos and otm_tan take the reduced argument x + y (y is the
// tail from argument reduction) and, for otm_tan, whether to return tan
// (odd == 0) or -1/tan (odd != 0).
double otm_SinKernel(double x, double y, int iy);
double otm_CosKernel(double x, double y);
double otm_TanKernel(double x, double y, int odd);

// Reduces x modulo pi/2 into y[0] + y[1] (y[1] is the tail) and returns the
// quadrant index mod 4 in the low two bits. Callers only use this outside
// |x| <~ pi/4.
int otm_RemPio2(double x, double *y);
int otm_RemPio2Large(double *x, double *y, int e0, int nx, int prec);
