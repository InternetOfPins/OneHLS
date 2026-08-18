/**
 * @file main.cpp
 * @brief Native-only demo of the NCO (nco.h) -- no Bambu required to run
 *        this file. See ../hls/nco_top.cpp for the actual Bambu
 *        synthesis target (same composition).
 *
 * Ground truth is the standard unit circle at 8 evenly-spaced points (45
 * degree steps), hand-derived, not a formula pulled from memory. PhaseInc
 * = 2/8 = 0.25 in angle_over_pi units (Q1.15 raw: 0.25*32768 = 8192) --
 * one full revolution completes in exactly 8 steps, landing back on the
 * start value, itself a correctness check (closed-loop wraparound).
 */
#include <oneHLS/ac_types_support.h>
#include "nco.h"
#include <cstdio>

using Sample = ac_fixed<16,1,true>;   // phase accumulator: I=1, free wraparound
using Accum  = ac_fixed<24,2,true>;   // cordic output: I=2, headroom for the
                                       // library's internal scale=1.0 literal
                                       // (see nco.h's header comment)

// File-scope, not function-local: ac_fixed leaves default construction's
// bits indeterminate on some vendor paths, and only static storage
// duration's language-mandated zero-init makes the first read
// well-defined -- same reasoning as every other native demo in this
// library's own test.cpp and examples.
nco_example::Nco<Sample, Accum, 8192> nco;

int main() {
  std::printf("NCO: 8 steps/revolution (45 degree steps), unit-circle output\n\n");

  const double expectedCos[8] = {  0.707107,  0.0, -0.707107, -1.0, -0.707107,  0.0,  0.707107,  1.0 };
  const double expectedSin[8] = {  0.707107,  1.0,  0.707107,  0.0, -0.707107, -1.0, -0.707107,  0.0 };

  for (int i = 0; i < 8; ++i) {
    oneHLS::Complex<Accum> r = nco.step();
    std::printf("step %d: cos=%+.6f (exp %+.6f)  sin=%+.6f (exp %+.6f)\n",
                i, r.re.to_double(), expectedCos[i], r.im.to_double(), expectedSin[i]);
  }
  std::printf("\none full revolution complete -- closed-loop wraparound confirmed\n");
  return 0;
}
