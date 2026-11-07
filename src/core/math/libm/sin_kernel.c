// Vendored from musl libc (MIT licence), originally FreeBSD /usr/src/lib/
// msun/src/k_sin.c (Sun Microsystems' permissive notice).
// Source: https://github.com/kraj/musl, path src/math/__sin.c, master
// branch at commit 7813e9119eb5fe681b6750b8a033a0490bca0941. Renamed
// (otm_ prefix) to avoid colliding with a platform libm.
//
// Kernel sine on ~[-pi/4, pi/4] (except on -0). x is the reduced argument,
// y is its tail from argument reduction (0 when iy == 0).

#include "libm_internal.h"

static const double S1 = -1.66666666666666324348e-01;
static const double S2 = 8.33333333332248946124e-03;
static const double S3 = -1.98412698298579493134e-04;
static const double S4 = 2.75573137070700676789e-06;
static const double S5 = -2.50507602534068634195e-08;
static const double S6 = 1.58969099521155010221e-10;

double otm_SinKernel(const double x, const double y, const int iy)
{
    const double z = x * x;
    const double w = z * z;
    const double r = S2 + z * (S3 + z * S4) + z * w * (S5 + z * S6);
    const double v = z * x;
    if (iy == 0) {
        return x + v * (S1 + z * r);
    }
    return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}
