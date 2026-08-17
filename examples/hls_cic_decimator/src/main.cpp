/**
 * @file main.cpp
 * @brief Native-only demo of the CIC decimator (cic_decimator.h) -- no
 * Bambu required to run this file. See ../hls/cic_decimator_top.cpp for
 * the actual Bambu synthesis target (same composition).
 *
 * Ground truth below was independently derived by direct simulation of
 * the architecture itself (not a textbook formula), cross-checked with a
 * separate DC-gain identity ((R*M)^N) -- see
 * ../../../docs/PHASE4_CIC_EXPERIMENT.md for the derivation.
 */
#include <oneHLS/ac_types_support.h>
#include "cic_decimator.h"
#include <cstdint>
#include <cstdio>

using Sum = ac_int<32,true>;

// File-scope, not function-local: ac_int/ac_fixed leave default
// construction's bits indeterminate on some vendor paths, and only
// static storage duration's language-mandated zero-init makes the first
// read well-defined -- same reasoning as every other native demo in this
// library's own test.cpp and examples.
cic_example::CicDecimator<Sum, Sum, 2, 4> decImpulse;
cic_example::CicDecimator<Sum, Sum, 2, 4> decDc;

int main() {
  std::printf("CIC decimator, N=2 stages, R=4 decimation (real Hogenauer architecture)\n\n");

  std::printf("Impulse response, decimated output (expect 4 0 0 0):\n");
  for (int32_t n = 0; n < 16; ++n) {
    Sum x = Sum(n == 0 ? 1 : 0);
    bool valid = false;
    Sum y = decImpulse.step(x, valid);
    if (valid) std::printf("%d ", y.to_int());
  }
  std::printf("\n\n");

  std::printf("DC step response, decimated output (expect 10 16 16 16 16, settles to (R*M)^N=16):\n");
  for (int32_t n = 0; n < 20; ++n) {
    Sum x = Sum(1);
    bool valid = false;
    Sum y = decDc.step(x, valid);
    if (valid) std::printf("%d ", y.to_int());
  }
  std::printf("\n");
  return 0;
}
