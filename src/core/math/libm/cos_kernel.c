// Vendored from musl libc (MIT licence), originally FreeBSD /usr/src/lib/
// msun/src/k_cos.c (Sun Microsystems' permissive notice).
// Source: https://github.com/kraj/musl, path src/math/__cos.c, master
// branch at commit 7813e9119eb5fe681b6750b8a033a0490bca0941. Renamed
// (otm_ prefix) to avoid colliding with a platform libm.
//
// Kernel cosine on [-pi/4, pi/4]. x is the reduced argument, y is its tail
// from argument reduction.

#include "libm_internal.h"

static const double C1 = 4.16666666666666019037e-02;
static const double C2 = -1.38888888888741095749e-03;
static const double C3 = 2.48015872894767294178e-05;
static const double C4 = -2.75573143513906633035e-07;
static const double C5 = 2.08757232129817482790e-09;
static const double C6 = -1.13596475577881948265e-11;

double otm_CosKernel(const double x, const double y)
{
    const double z = x * x;
    const double w = z * z;
    const double r =
        z * (C1 + z * (C2 + z * C3)) + w * w * (C4 + z * (C5 + z * C6));
    const double hz = 0.5 * z;
    const double w2 = 1.0 - hz;
    return w2 + (((1.0 - w2) - hz) + (z * r - x * y));
}
