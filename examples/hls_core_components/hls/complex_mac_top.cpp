// Bambu synthesis target: OneHLS's generic oneHLS::ComplexMac<> (built on
// OneHLS's own vendor-agnostic Complex<T>, NOT ac_complex<T>) instantiated
// with ac_fixed, same coefficients/types as the predecessor this
// generalizes (OneData/.RnD/acTypesHLS/hls/ac_complex_cmac_top.cpp) --
// confirms whether the known BRAM-binding anomaly (the one non-zero-cost
// component in this whole investigation, see README) reproduces with a
// user-defined struct-of-two-fields the same way it did with ac_complex<T>,
// rather than assuming it does.
#include <oneHLS/oneHLS.h>
#include <oneHLS/ac_types_support.h>
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using CFixed = ac_fixed<16, 16, true>;
using CAccum = ac_fixed<32, 32, true>;

// coefficient 2 - 1j
oneHLS::ComplexMac<CFixed, CAccum, 2, -1> cmac;

void oneHlsComplexMacTop(int16_t xr, int16_t xi, int32_t* out_re, int32_t* out_im) {
  oneHLS::Complex<CAccum> r = cmac.step(oneHLS::Complex<CFixed>{CFixed(xr), CFixed(xi)});
  *out_re = r.re.to_int();
  *out_im = r.im.to_int();
}
