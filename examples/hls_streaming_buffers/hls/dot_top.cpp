// Round 3 synthesis target: oneHLS::Dot<> -- Fir<>'s MAC fold with the
// delay line removed. Same coeffs/types as the shipped Fir top
// (examples/hls_core_components/hls/fir_top.cpp); the check is whether
// removing the Data<Sample> state also removes all storage (Fir keeps 4
// delay registers; Dot should keep none) at no arithmetic cost.
#include "../src/dot.h"
#include <oneHLS/ac_types_support.h>
#include <ac_fixed.h>
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16, 16, true>;
using Accum  = ac_fixed<32, 32, true>;

static oneHLS::Dot<Sample, Accum, 10, 118, 118, 10> d;

int32_t oneHlsDotTop(int16_t w0, int16_t w1, int16_t w2, int16_t w3) {
  Sample w[4] = { Sample(w0), Sample(w1), Sample(w2), Sample(w3) };
  return d.dot(w).to_int();
}
