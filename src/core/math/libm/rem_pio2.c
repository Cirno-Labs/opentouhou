// Vendored from musl libc (MIT licence), originally FreeBSD /usr/src/lib/
// msun/src/e_rem_pio2.c (Sun Microsystems' permissive notice), optimized
// by Bruce D. Evans.
// Source: https://github.com/kraj/musl, path src/math/__rem_pio2.c,
// master branch at commit 7813e9119eb5fe681b6750b8a033a0490bca0941.
// Renamed (otm_ prefix) to avoid colliding with a platform libm.
//
// Reduces x modulo pi/2 into y[0] + y[1] (y[1] is the tail) and returns
// the quotient mod 8 (the caller only reads it mod 4). Dispatches to
// otm_RemPio2Large for |x| beyond about 2^20*(pi/2).

#include "libm_internal.h"

static const double toint = 1.5 / DBL_EPSILON;
static const double pio4 = 0x1.921fb54442d18p-1;
static const double invpio2 = 6.36619772367581382433e-01;
static const double pio2_1 = 1.57079632673412561417e+00;
static const double pio2_1t = 6.07710050650619224932e-11;
static const double pio2_2 = 6.07710050630396597660e-11;
static const double pio2_2t = 2.02226624879595063154e-21;
static const double pio2_3 = 2.02226624871116645580e-21;
static const double pio2_3t = 8.47842766036889956997e-32;

// The caller must handle the case when no reduction is needed: |x| <~ pi/4.
int otm_RemPio2(double x, double *y)
{
    union {
        double f;
        uint64_t i;
    } u = { x };
    double_t z, w, t, r, fn;
    double tx[3], ty[2];
    uint32_t ix;
    int sign, n, ex, ey, i;

    sign = u.i >> 63;
    ix = u.i >> 32 & 0x7fffffff;
    if (ix <= 0x400f6a7a) { // |x| ~<= 5pi/4
        if ((ix & 0xfffff) == 0x921fb) { // |x| ~= pi/2 or 2pi/2
            goto medium; // cancellation -- use medium case
        }
        if (ix <= 0x4002d97c) { // |x| ~<= 3pi/4
            if (!sign) {
                z = x - pio2_1; // one round good to 85 bits
                y[0] = z - pio2_1t;
                y[1] = (z - y[0]) - pio2_1t;
                return 1;
            }
            z = x + pio2_1;
            y[0] = z + pio2_1t;
            y[1] = (z - y[0]) + pio2_1t;
            return -1;
        }
        if (!sign) {
            z = x - 2 * pio2_1;
            y[0] = z - 2 * pio2_1t;
            y[1] = (z - y[0]) - 2 * pio2_1t;
            return 2;
        }
        z = x + 2 * pio2_1;
        y[0] = z + 2 * pio2_1t;
        y[1] = (z - y[0]) + 2 * pio2_1t;
        return -2;
    }
    if (ix <= 0x401c463b) { // |x| ~<= 9pi/4
        if (ix <= 0x4015fdbc) { // |x| ~<= 7pi/4
            if (ix == 0x4012d97c) { // |x| ~= 3pi/2
                goto medium;
            }
            if (!sign) {
                z = x - 3 * pio2_1;
                y[0] = z - 3 * pio2_1t;
                y[1] = (z - y[0]) - 3 * pio2_1t;
                return 3;
            }
            z = x + 3 * pio2_1;
            y[0] = z + 3 * pio2_1t;
            y[1] = (z - y[0]) + 3 * pio2_1t;
            return -3;
        }
        if (ix == 0x401921fb) { // |x| ~= 4pi/2
            goto medium;
        }
        if (!sign) {
            z = x - 4 * pio2_1;
            y[0] = z - 4 * pio2_1t;
            y[1] = (z - y[0]) - 4 * pio2_1t;
            return 4;
        }
        z = x + 4 * pio2_1;
        y[0] = z + 4 * pio2_1t;
        y[1] = (z - y[0]) + 4 * pio2_1t;
        return -4;
    }
    if (ix < 0x413921fb) { // |x| ~< 2^20*(pi/2), medium size
    medium:
        // rint(x/(pi/2))
        fn = (double_t)x * invpio2 + toint - toint;
        n = (int32_t)fn;
        r = x - fn * pio2_1;
        w = fn * pio2_1t; // 1st round, good to 85 bits
        // Matters with directed rounding.
        if (otm_predict_false(r - w < -pio4)) {
            n--;
            fn--;
            r = x - fn * pio2_1;
            w = fn * pio2_1t;
        } else if (otm_predict_false(r - w > pio4)) {
            n++;
            fn++;
            r = x - fn * pio2_1;
            w = fn * pio2_1t;
        }
        y[0] = r - w;
        u.f = y[0];
        ey = u.i >> 52 & 0x7ff;
        ex = ix >> 20;
        if (ex - ey > 16) { // 2nd round, good to 118 bits
            t = r;
            w = fn * pio2_2;
            r = t - w;
            w = fn * pio2_2t - ((t - r) - w);
            y[0] = r - w;
            u.f = y[0];
            ey = u.i >> 52 & 0x7ff;
            if (ex - ey > 49) { // 3rd round, good to 151 bits, all cases
                t = r;
                w = fn * pio2_3;
                r = t - w;
                w = fn * pio2_3t - ((t - r) - w);
                y[0] = r - w;
            }
        }
        y[1] = (r - y[0]) - w;
        return n;
    }
    // All other (large) arguments.
    if (ix >= 0x7ff00000) { // x is inf or NaN
        y[0] = y[1] = x - x;
        return 0;
    }
    // Set z = scalbn(|x|,-ilogb(x)+23).
    u.f = x;
    u.i &= (uint64_t)-1 >> 12;
    u.i |= (uint64_t)(0x3ff + 23) << 52;
    z = u.f;
    for (i = 0; i < 2; i++) {
        tx[i] = (double)(int32_t)z;
        z = (z - tx[i]) * 0x1p24;
    }
    tx[i] = z;
    // Skip zero terms, the first term is non-zero.
    while (tx[i] == 0.0) {
        i--;
    }
    n = otm_RemPio2Large(tx, ty, (int)(ix >> 20) - (0x3ff + 23), i + 1, 1);
    if (sign) {
        y[0] = -ty[0];
        y[1] = -ty[1];
        return -n;
    }
    y[0] = ty[0];
    y[1] = ty[1];
    return n;
}
