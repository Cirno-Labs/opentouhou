// Vendored from musl libc (MIT licence), originally FreeBSD /usr/src/lib/
// msun/src/k_tan.c (Sun Microsystems' permissive notice).
// Source: https://github.com/kraj/musl, path src/math/__tan.c, master
// branch at commit 7813e9119eb5fe681b6750b8a033a0490bca0941. Renamed
// (otm_ prefix) to avoid colliding with a platform libm.
//
// Kernel tangent on ~[-pi/4, pi/4] (except on -0). x is the reduced
// argument, y is its tail from argument reduction. Returns tan(x+y) when
// odd == 0, and -1/tan(x+y) when odd != 0.

#include "libm_internal.h"

static const double T[] = {
    3.33333333333334091986e-01, 1.33333333333201242699e-01,
    5.39682539762260521377e-02, 2.18694882948595424599e-02,
    8.86323982359930005737e-03, 3.59207910759131235356e-03,
    1.45620945432529025516e-03, 5.88041240820264096874e-04,
    2.46463134818469906812e-04, 7.81794442939557092300e-05,
    7.14072491382608190305e-05, -1.85586374855275456654e-05,
    2.59073051863633712884e-05,
};
static const double PIO4 = 7.85398163397448278999e-01;
static const double PIO4_LO = 3.06161699786838301793e-17;

double otm_TanKernel(double x, double y, const int odd)
{
    double_t z, r, v, w, s, a;
    double w0, a0;
    uint32_t hx;
    int big, sign;

    OTM_GET_HIGH_WORD(hx, x);
    big = (hx & 0x7fffffff) >= 0x3fe59428; // |x| >= 0.6744
    if (big) {
        sign = hx >> 31;
        if (sign) {
            x = -x;
            y = -y;
        }
        x = (PIO4 - x) + (PIO4_LO - y);
        y = 0.0;
    }
    z = x * x;
    w = z * z;
    // Break x^5*(T[1]+x^2*T[2]+...) into
    // x^5(T[1]+x^4*T[3]+...+x^20*T[11]) +
    // x^5(x^2*(T[2]+x^4*T[4]+...+x^22*[T12]))
    r = T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] + w * T[11]))));
    v = z
        * (T[2]
           + w * (T[4] + w * (T[6] + w * (T[8] + w * (T[10] + w * T[12])))));
    s = z * x;
    r = y + z * (s * (r + v) + y) + s * T[0];
    w = x + r;
    if (big) {
        s = 1 - 2 * odd;
        v = s - 2.0 * (x + (r - w * w / (w + s)));
        return sign ? -v : v;
    }
    if (!odd) {
        return w;
    }
    // -1.0/(x+r) has up to 2ulp error, so compute it accurately.
    w0 = w;
    OTM_SET_LOW_WORD(w0, 0);
    v = r - (w0 - x); // w0+v = r+x
    a0 = a = -1.0 / w;
    OTM_SET_LOW_WORD(a0, 0);
    return a0 + a * (1.0 + a0 * w0 + a0 * v);
}
