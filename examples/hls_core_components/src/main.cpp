/**
 * @file main.cpp
 * @brief Native-only demo of OneHLS's five core components -- no Bambu
 * required to run this file. See ../hls/*.cpp for the actual Bambu
 * synthesis targets (same instantiations, same coefficients).
 *
 * Every printed sequence below matches the main README's "Verified
 * results" table exactly and is also independently exercised (both
 * ac_fixed and ap_fixed) by ../../../test/test.cpp -- this demo exists
 * to show the components running, not to re-derive their correctness.
 */
#include <oneHLS/oneHLS.h>
#include <oneHLS/ac_types_support.h>
#include <cstdint>
#include <cstdio>

using Sample = ac_fixed<16, 16, true>;   // Fir: whole-number coefficients
using Accum  = ac_fixed<32, 32, true>;

using SampleQ = ac_fixed<16, 8, true>;   // Biquad/Pid: Q8.8
using AccumQ  = ac_fixed<32, 16, true>;  // Q16.16

using Sum = ac_int<8, true>;             // Accumulator: deliberately narrow

using CFixed = ac_fixed<16, 16, true>;
using CAccum = ac_fixed<32, 32, true>;

// File-scope, not function-local: ac_int/ac_fixed leave default
// construction's bits indeterminate on some vendor paths, and only
// static storage duration's language-mandated zero-init makes the first
// read well-defined -- same reasoning as every other native demo in this
// library's own test.cpp and examples.
oneHLS::Fir<Sample, Accum, 10, 118, 118, 10> fir;
oneHLS::Biquad<SampleQ, AccumQ, 128, 64, 128, -64> biquad;
oneHLS::Pid<SampleQ, AccumQ, 256, 64, 128> pidConst, pidImpulse;
oneHLS::Accumulator<Sum, Sum> acc;
oneHLS::ComplexMac<CFixed, CAccum, 2, -1> cmac;  // coefficient 2 - 1j

static int32_t pidStep(oneHLS::Pid<SampleQ, AccumQ, 256, 64, 128>& pid, int16_t er) {
  SampleQ u = pid.step(SampleQ(er));
  return ac_int<16, true>(u.template slc<16>(0)).to_int();
}

int main() {
  std::printf("OneHLS core components -- native demo (ac_fixed)\n\n");

  std::printf("Fir<> impulse response (expect 0 10 118 118 10 0 0 0):\n");
  int16_t firImpulse[] = {1, 0, 0, 0, 0, 0, 0, 0};
  for (int i = 0; i < 8; ++i)
    std::printf("%d ", fir.filter(Sample(firImpulse[i])).to_int());
  std::printf("\n\n");

  std::printf("Biquad<> impulse response (expect 0 128 128 32 -16 -16 -4 2):\n");
  int16_t biquadImpulse[] = {1, 0, 0, 0, 0, 0, 0, 0};
  for (int i = 0; i < 8; ++i)
    std::printf("%d ", biquad.step(SampleQ(biquadImpulse[i])).template slc<16>(0).to_int());
  std::printf("\n\n");

  std::printf("Pid<> constant error e=1 x5 (expect 448 384 448 512 576):\n");
  for (int i = 0; i < 5; ++i)
    std::printf("%d ", pidStep(pidConst, 1));
  std::printf("\n\n");

  std::printf("Pid<> impulse disturbance (expect 448 -64 64 64 64):\n");
  int16_t pidImpulseSeq[] = {1, 0, 0, 0, 0};
  for (int i = 0; i < 5; ++i)
    std::printf("%d ", pidStep(pidImpulse, pidImpulseSeq[i]));
  std::printf("\n\n");

  std::printf("Accumulator<ac_int<8,true>> +50 x3, wraps (expect 50 100 -106):\n");
  std::printf("%d ", acc.step(Sum(50)).to_int());
  std::printf("%d ", acc.step(Sum(50)).to_int());
  std::printf("%d ", acc.step(Sum(50)).to_int());
  std::printf("\n\n");

  std::printf("ComplexMac<> coeff 2-1j, input 3+4j x2 (expect 10+5j then 20+10j):\n");
  oneHLS::Complex<CFixed> x{CFixed(3), CFixed(4)};
  auto r1 = cmac.step(x);
  std::printf("%d+%dj ", r1.re.to_int(), r1.im.to_int());
  auto r2 = cmac.step(x);
  std::printf("%d+%dj\n", r2.re.to_int(), r2.im.to_int());

  return 0;
}
