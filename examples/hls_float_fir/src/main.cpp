/**
 * @file main.cpp
 * @brief OneHLS's Fir<> over real IEEE754 floating point (ac_std_float),
 *        not fixed-point -- native-only demo, no Bambu required to run
 *        this file. See ../hls/fir_std_float_top.cpp for the actual
 *        Bambu synthesis target (same composition, raw-bit I/O instead
 *        of the human-readable doubles this demo prints).
 *
 * This is NOT the primary/recommended way to use OneHLS -- ac_fixed/
 * ap_fixed are the well-supported, zero-cost backends (see the main
 * README.md). This demonstrates a real, working, but deliberately
 * secondary capability: the exact same Fir<> template, unmodified,
 * also runs over genuine floating point.
 */
#include <oneHLS/oneHLS.h>
#include <ac_std_float.h>
#include <cstdint>
#include <cstdio>

using Sample = ac_std_float<32, 8>;   // IEEE754 float32
using Accum  = ac_std_float<64, 11>;  // IEEE754 float64

// Same 4-tap Hamming-LPF coefficients as every other Fir<> example in
// this library. Coefficients sum to 256 (10+118+118+10), so a step of
// height V settles at a steady-state output of V*256 once all 4 taps
// hold V -- this demo's own scale factor, not a OneHLS convention.
//
// Both instances are file-scope (static storage duration), not locals
// inside main(): ac_std_float deliberately leaves a default-constructed
// value's bits indeterminate (mirroring an unreset hardware register,
// not a bug) -- only static storage duration's language-mandated
// zero-init makes the first read well-defined. A stack-local instance
// here would read garbage on its first few calls; this isn't
// hypothetical, it's what happened when this file was first written.
oneHLS::Fir<Sample, Accum, 10, 118, 118, 10> fir;
oneHLS::Fir<Sample, Accum, 10, 118, 118, 10> stepFir;

int main() {
  std::printf("Impulse response (expect 0 10 118 118 10 0 0 0):\n");
  int16_t impulse[] = {1, 0, 0, 0, 0, 0, 0, 0};
  for (int16_t x : impulse) {
    Accum y = fir.filter(Sample(x));
    std::printf("%g ", y.to_double());
  }
  std::printf("\n\n");

  std::printf("Step response, step from 0 to 1 at sample 2 (expect 0 0 0 10 128 246 256 256):\n");
  for (int i = 0; i < 8; ++i) {
    int16_t x = (i >= 2) ? 1 : 0;
    Accum y = stepFir.filter(Sample(x));
    std::printf("%g ", y.to_double());
  }
  std::printf("\n");
  return 0;
}
