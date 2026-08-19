// Bambu synthesis target: OneHLS's generic oneHLS::Accumulator<> instantiated
// with ac_int<8,true> for both Sample and Accum, matching the predecessor
// this generalizes (OneData/.RnD/acTypesHLS/hls/ac_accumulator_top.cpp)
// exactly, including its deliberate narrow-width 2's-complement wraparound.
#include <oneHLS/oneHLS.h>
#include <oneHLS/ac_types_support.h>
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sum = ac_int<8, true>;

oneHLS::Accumulator<Sum, Sum> acc;

int32_t oneHlsAccumulatorTop(int8_t x) {
  return acc.step(Sum(x)).to_int();
}
