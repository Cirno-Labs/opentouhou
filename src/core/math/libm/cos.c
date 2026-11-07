// Vendored from musl libc (MIT licence), originally FreeBSD /usr/src/lib/
// msun/src/s_cos.c (Sun Microsystems' permissive notice).
// Source: https://github.com/kraj/musl, path src/math/cos.c, master
// branch at commit 7813e9119eb5fe681b6750b8a033a0490bca0941. Renamed
// (otm_ prefix) to avoid colliding with a platform libm.

#include "libm_internal.h"

double otm_Cos(double x)
{
    double y[2];
    uint32_t ix;
    unsigned n;

    OTM_GET_HIGH_WORD(ix, x);
    ix &= 0x7fffffff;

    if (ix <= 0x3fe921fb) { // |x| ~< pi/4
        if (ix < 0x3e46a09e) { // |x| < 2**-27 * sqrt(2)
            OTM_FORCE_EVAL(x + 0x1p120); // raises inexact if x != 0
            return 1.0;
        }
        return otm_CosKernel(x, 0);
    }

    if (ix >= 0x7ff00000) { // cos(Inf or NaN) is NaN
        return x - x;
    }

    n = otm_RemPio2(x, y);
    switch (n & 3) {
    case 0:
        return otm_CosKernel(y[0], y[1]);
    case 1:
        return -otm_SinKernel(y[0], y[1], 1);
    case 2:
        return -otm_CosKernel(y[0], y[1]);
    default:
        return otm_SinKernel(y[0], y[1], 1);
    }
}
