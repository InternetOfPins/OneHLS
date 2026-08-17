/**
 * @file cic_decimator_top.cpp
 * @brief Bambu synthesis target for the CIC decimator example. Same
 * device/clock convention as every other synthesis target in this
 * library: xc7a100t-1csg324-VVD, 10ns clock (100MHz).
 */
#include <oneHLS/ac_types_support.h>
#include "../src/cic_decimator.h"
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sum = ac_int<32,true>;

cic_example::CicDecimator<Sum, Sum, 2, 4> dec;

int32_t cicDecimatorTop(int32_t x, bool* valid) {
  bool v = false;
  Sum y = dec.step(Sum(x), v);
  *valid = v;
  return y.to_int();
}
