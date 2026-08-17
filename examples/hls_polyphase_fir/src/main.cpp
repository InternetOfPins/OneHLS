/**
 * @file main.cpp
 * @brief Native-only demo of the polyphase FIR decimator
 * (polyphase_fir.h) -- no Bambu required to run this file. See
 * ../hls/polyphase_fir_top.cpp for the actual Bambu synthesis target
 * (same composition).
 *
 * Ground truth: the real, Tap-delayed oneHLS::Fir<> impulse/DC response
 * for these coefficients (10,118,118,10 -- README's own Fir<> example),
 * downsampled -- see ../../../docs/PHASE4_POLYPHASE_EXPERIMENT.md §2 for
 * the derivation (and a real first-pass mistake it corrects: an
 * idealized, non-pipelined reference model gives the WRONG answer here,
 * because Fir<>'s real Tap has one cycle of pipeline delay).
 */
#include <oneHLS/ac_types_support.h>
#include "polyphase_fir.h"
#include <cstdint>
#include <cstdio>

using Sample = ac_fixed<16,16,true>;
using Accum  = ac_fixed<32,32,true>;

// File-scope, not function-local: ac_fixed deliberately leaves default
// construction's bits indeterminate, and only static storage duration's
// language-mandated zero-init makes the first read well-defined -- this
// exact pitfall was hit (and documented) three times earlier in this
// research; naming it again here rather than silently avoiding it.
polyphase_example::PolyphaseFirDecim<Sample, Accum, 2, 10,118,118,10> decImpulse;
polyphase_example::PolyphaseFirDecim<Sample, Accum, 2, 10,118,118,10> decDc;

int main() {
  std::printf("Polyphase FIR decimator, M=2 branches, oneHLS::Fir<> coefficients (10,118,118,10)\n");
  std::printf("Branches held in oneHLS::StaticList<>, dispatched by a runtime commutator via visit()\n\n");

  std::printf("Impulse response, decimated output (expect 0 118 10 0 0 0 0 0):\n");
  for (int32_t n = 0; n < 20; ++n) {
    Sample x = Sample(n == 0 ? 1 : 0);
    bool valid = false;
    Accum y = decImpulse.step(x, valid);
    if (valid) std::printf("%d ", y.to_int());
  }
  std::printf("\n\n");

  std::printf("DC step response, decimated output (expect 0 128 256 256 256 256):\n");
  for (int32_t n = 0; n < 20; ++n) {
    Sample x = Sample(1);
    bool valid = false;
    Accum y = decDc.step(x, valid);
    if (valid) std::printf("%d ", y.to_int());
  }
  std::printf("\n");
  return 0;
}
