/**
 * @file dot.h
 * @brief Round 3 — Dot<Sample,Accum,Coeffs...>: a K-tap multiply-accumulate
 * over a window supplied externally (K = sizeof...(Coeffs)).
 *
 * Per the Round 3 gate (HANDOFF.md): this is Fir<> with the inter-tap
 * delay removed. Same Chain<Tap...> + Terminal shape, same rawCoeff /
 * Accum-widening arithmetic as oneHLS::Fir; the only new code is
 * DotTapLogic = TapLogic minus its Data<Sample> state and Base::set().
 * A Dot over a shifting window IS a FIR filter, and Fir is its reference.
 */
#pragma once
#include <oneHLS/oneHLS.h>   // rawCoeff, RawBitsCtor
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  /// terminal for the Dot fold: window pointer + position threaded through
  template<typename Sample, typename Accum>
  struct DotTerminal {
    static Accum mac(const Sample*, int32_t, Accum acc) { return acc; }
  };

  /// one tap: multiply w[i] by this tap's compile-time coefficient, add to
  /// the running sum, advance to the next window position. No state.
  template<typename Sample, typename Accum, int32_t RawBits>
  struct DotTapLogic {
    template<typename I>
    struct Part : I {
      using Base = I;
      using Base::Base;
      Accum mac(const Sample* w, int32_t i, Accum acc) {
        Accum sum = acc + Accum(rawCoeff<Sample,RawBits>()) * Accum(w[i]);
        return I::mac(w, i + 1, sum);
      }
    };
  };

  template<typename Sample, typename Accum, int32_t... Coeffs>
  struct Dot : APIOf<DotTerminal<Sample,Accum>,
                     Chain<DotTapLogic<Sample,Accum,Coeffs>...>> {
    using Base = APIOf<DotTerminal<Sample,Accum>,
                       Chain<DotTapLogic<Sample,Accum,Coeffs>...>>;
    using Base::Base;
    static constexpr int32_t K = sizeof...(Coeffs);
    /// structural upper bound on HW stages: a K-term MAC balances to a
    /// ceil(log2 K) adder tree; +2 for the multiplies and the load.
    static constexpr int32_t LatencyBound =
        (K <= 1 ? 1 : K <= 2 ? 3 : K <= 4 ? 4 : K <= 8 ? 5 : K <= 16 ? 6 :
         K <= 32 ? 7 : K <= 64 ? 8 : K <= 128 ? 9 : 10);

    /// w[0..K-1], w[0] weighted by the first Coeff. Returns sum coeff[i]*w[i].
    Accum dot(const Sample w[K]) { return Base::mac(w, 0, Accum(0)); }
  };

}
