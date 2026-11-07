// Vendored from musl libc (MIT licence), originally FreeBSD /usr/src/lib/
// msun/src/s_tan.c (Sun Microsystems' permissive notice).
// Source: https://github.com/kraj/musl, path src/math/tan.c, master
// branch at commit 7813e9119eb5fe681b6750b8a033a0490bca0941. Renamed
// (otm_ prefix) to avoid colliding with a platform libm.

#include "libm_internal.h"

double otm_Tan(double x)
{
    double y[2];
    uint32_t ix;
    unsigned n;

    OTM_GET_HIGH_WORD(ix, x);
    ix &= 0x7fffffff;

    if (ix <= 0x3fe921fb) { // |x| ~< pi/4
        if (ix < 0x3e400000) { // |x| < 2**-27
            OTM_FORCE_EVAL(ix < 0x00100000 ? x / 0x1p120 : x + 0x1p120);
            return x;
        }
        return otm_TanKernel(x, 0.0, 0);
    }

    if (ix >= 0x7ff00000) { // tan(Inf or NaN) is NaN
        return x - x;
    }

    n = otm_RemPio2(x, y);
    return otm_TanKernel(y[0], y[1], n & 1);
}
