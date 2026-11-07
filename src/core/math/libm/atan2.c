// Vendored from musl libc (MIT licence), originally FreeBSD /usr/src/lib/
// msun/src/e_atan2.c (Sun Microsystems' permissive notice).
// Source: https://github.com/kraj/musl, path src/math/atan2.c, master
// branch at commit 7813e9119eb5fe681b6750b8a033a0490bca0941. Renamed
// (otm_ prefix) to avoid colliding with a platform libm.

#include "libm_internal.h"

double otm_Atan(double x);

static const double pi = 3.1415926535897931160E+00;
static const double pi_lo = 1.2246467991473531772E-16;

double otm_Atan2(double y, double x)
{
    double z;
    uint32_t m, lx, ly, ix, iy;

    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    OTM_EXTRACT_WORDS(ix, lx, x);
    OTM_EXTRACT_WORDS(iy, ly, y);
    if (((ix - 0x3ff00000) | lx) == 0) { // x = 1.0
        return otm_Atan(y);
    }
    m = ((iy >> 31) & 1) | ((ix >> 30) & 2); // 2*sign(x)+sign(y)
    ix = ix & 0x7fffffff;
    iy = iy & 0x7fffffff;

    if ((iy | ly) == 0) { // y = 0
        switch (m) {
        case 0:
        case 1:
            return y; // atan(+-0,+anything)=+-0
        case 2:
            return pi; // atan(+0,-anything) = pi
        default:
            return -pi; // atan(-0,-anything) =-pi
        }
    }
    if ((ix | lx) == 0) { // x = 0
        return m & 1 ? -pi / 2 : pi / 2;
    }
    if (ix == 0x7ff00000) { // x is INF
        if (iy == 0x7ff00000) {
            switch (m) {
            case 0:
                return pi / 4; // atan(+INF,+INF)
            case 1:
                return -pi / 4; // atan(-INF,+INF)
            case 2:
                return 3 * pi / 4; // atan(+INF,-INF)
            default:
                return -3 * pi / 4; // atan(-INF,-INF)
            }
        } else {
            switch (m) {
            case 0:
                return 0.0; // atan(+...,+INF)
            case 1:
                return -0.0; // atan(-...,+INF)
            case 2:
                return pi; // atan(+...,-INF)
            default:
                return -pi; // atan(-...,-INF)
            }
        }
    }
    if (ix + (64 << 20) < iy || iy == 0x7ff00000) { // |y/x| > 0x1p64
        return m & 1 ? -pi / 2 : pi / 2;
    }

    // z = atan(|y/x|) without spurious underflow.
    if ((m & 2) && iy + (64 << 20) < ix) { // |y/x| < 0x1p-64, x<0
        z = 0;
    } else {
        z = otm_Atan(fabs(y / x));
    }
    switch (m) {
    case 0:
        return z; // atan(+,+)
    case 1:
        return -z; // atan(-,+)
    case 2:
        return pi - (z - pi_lo); // atan(+,-)
    default:
        return (z - pi_lo) - pi; // atan(-,-)
    }
}
