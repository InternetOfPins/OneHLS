// Bambu synthesis target: OneHLS's generic oneHLS::Biquad<> instantiated
// with ac_fixed, same coefficients/types as the predecessor this
// generalizes (OneData/.RnD/acTypesHLS/hls/biquad_top.cpp) -- confirms
// the direct-form-I IIR composition (feedforward Tap reused verbatim,
// feedback FBTap's two-phase fbSum()/fbPush() split) still synthesizes
// cleanly once genericized.
#include <oneHLS/oneHLS.h>
#include <oneHLS/ac_types_support.h>
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16, 8, true>;   // Q8.8
using Accum  = ac_fixed<32, 16, true>;  // Q16.16

// b1=0.5(128), b2=0.25(64); -a1=0.5(128), -a2=-0.25(-64).
oneHLS::Biquad<Sample, Accum, 128, 64, 128, -64> biquad;

int32_t oneHlsBiquadTop(int16_t xr) {
  return biquad.step(Sample(xr)).template slc<16>(0).to_int();
}
