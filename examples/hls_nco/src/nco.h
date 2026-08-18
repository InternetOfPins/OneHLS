/**
 * @file nco.h
 * @brief Nco<Sample,Accum,PhaseIncRawBits>: a numerically controlled
 * oscillator that composes an EXISTING external HLS primitive
 * (ac_math::ac_sin_cordic/ac_cos_cordic) directly, rather than
 * reimplementing it -- the one exception found (source-read, not
 * assumed) to this library's general finding that ac_math/ac_dsp's
 * Catapult/SystemC-shaped code doesn't fit HAPI's Chain<> composition
 * (see ../../ECOSYSTEM.md). ac_sincos_cordic is pure and stateless (no
 * internal state, no ac_channel<>, no mc_scverify.h), so it's a real
 * direct-composition candidate.
 *
 * The phase accumulator is real HAPI/OneData state+composition
 * (oneHLS::Accumulator<>, reused completely unmodified); the CORDIC call
 * is an ordinary external function call bolted on afterward, outside the
 * Chain<> -- the literal "compose an existing HLS primitive without
 * owning or wrapping its implementation" claim being demonstrated here.
 *
 * NOT vendor-generic like Fir/Biquad/Pid/Accumulator/ComplexMac --
 * ac_sincos_cordic is ac_math-specific (no ap_fixed equivalent), so Nco
 * is necessarily tied to ac_fixed. That's the honest cost of this
 * design, not hidden.
 *
 * Two independent type requirements, both real and non-obvious --
 * found verifying this, not documented anywhere upstream:
 *
 * 1. Sample (the phase accumulator's own type) must be
 *    ac_fixed<W,1,true,...> -- exactly 1 integer bit -- so the phase
 *    accumulator's 2's-complement wraparound on overflow lands EXACTLY
 *    on ac_sincos_cordic's own "angle scaled by pi, range [-1,1)"
 *    convention for free: one full 2*pi revolution is angle_over_pi
 *    advancing by 2.0, and wrapping mod 2 is precisely what an AI=1
 *    signed accumulator already does on overflow (same mechanism
 *    already verified by Accumulator<ac_int<8,true>>'s own "50 100
 *    -106" wraparound test in the main README). No ad hoc range
 *    reduction needed.
 *
 * 2. Accum (the sin/cos OUTPUT type) independently needs AI>=2.
 *    ac_sin_cordic/ac_cos_cordic each build an internal
 *    `ac_fixed<OW,OI,...> scale = 1.0;` constant TYPED FROM THE OUTPUT
 *    REFERENCE ARGUMENT. At OI=1 the representable range is [-1,1) and
 *    1.0 itself is out of range, so it silently WRAPS to -1.0 under the
 *    type's default AC_WRAP, negating (180-degree-rotating) every
 *    result. Isolated directly: the angle argument can stay AI=1
 *    unmodified -- only the output type's AI matters. This maps exactly
 *    onto OneHLS's pre-existing Sample/Accum headroom convention (Accum
 *    always wider than Sample elsewhere in this library too, e.g.
 *    Fir/Pid's Q16.16 Accum over Q8.8 Sample) -- no new shape needed,
 *    just the existing one applied correctly.
 */
#pragma once
#include <oneHLS/oneHLS.h>
#include <ac_math/ac_sincos_cordic.h>
#include <cstdint>

namespace nco_example {
  using namespace oneHLS;

  template<typename Sample, typename Accum, int32_t PhaseIncRawBits>
  struct Nco {
    Accumulator<Sample, Sample> phaseAcc;
    Complex<Accum> step() {
      Sample phase = phaseAcc.step(rawCoeff<Sample, PhaseIncRawBits>());
      Accum s, c;
      ac_math::ac_sin_cordic(phase, s);
      ac_math::ac_cos_cordic(phase, c);
      return Complex<Accum>{c, s};
    }
  };
}
