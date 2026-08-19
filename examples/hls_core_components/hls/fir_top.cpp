// Bambu synthesis target: OneHLS's generic oneHLS::Fir<> instantiated with
// ac_fixed, same coefficients/types as the predecessor this generalizes
// (OneData/.RnD/acTypesHLS/hls/fir_lpf4_actypes_reg_top.cpp) -- confirms
// the library's type-erasure-free genericity costs nothing extra under
// synthesis vs. the hand-written, non-generic version it replaces.
#include <oneHLS/oneHLS.h>
#include <oneHLS/ac_types_support.h>
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16, 16, true>;
using Accum  = ac_fixed<32, 32, true>;

oneHLS::Fir<Sample, Accum, 10, 118, 118, 10> fir;

int32_t oneHlsFirTop(int16_t x) {
  return fir.filter(Sample(x)).to_int();
}
