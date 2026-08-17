/**
 * @file polyphase_fir_top.cpp
 * @brief Bambu synthesis target for the polyphase FIR decimator example.
 * Same device/clock convention as every other synthesis target in this
 * library: xc7a100t-1csg324-VVD, 10ns clock (100MHz).
 */
#include <oneHLS/ac_types_support.h>
#include "../src/polyphase_fir.h"
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16,16,true>;
using Accum  = ac_fixed<32,32,true>;

polyphase_example::PolyphaseFirDecim<Sample, Accum, 2, 10,118,118,10> dec;

int32_t polyphaseFirTop(int32_t x, bool* valid) {
  bool v = false;
  Accum y = dec.step(Sample(x), v);
  *valid = v;
  return y.to_int();
}
