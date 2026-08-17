/**
 * @file cic_decimator.h
 * @brief A Hogenauer CIC decimator (N stages, decimation factor R), built
 * from oneHLS::Accumulator<> (reused unmodified) plus one small new Comb
 * atom, over HAPI composition -- shared by src/main.cpp (native demo) and
 * hls/cic_decimator_top.cpp (Bambu synthesis target) so both compile the
 * identical definition. Not yet promoted to include/oneHLS/oneHLS.h --
 * see ../../../docs/PHASE4_CIC_EXPERIMENT.md for the full derivation,
 * ground-truth verification, and the composed-vs-hand-written-monolithic
 * Bambu comparison (byte-identical resource counts) this example's own
 * README numbers are drawn from.
 */
#pragma once
#include <oneHLS/oneHLS.h>
#include <cstdint>

namespace cic_example {
  using namespace oneHLS;

  // Comb (M=1): y[n] = x[n] - x[n-1]. Same delay-register shape as
  // Tap's mac() -- one Data<Accum> register, read-old/store-new/forward
  // -- subtracting instead of multiply-accumulating.
  template<typename Accum>
  struct CombLogic {
    template<typename I>
    struct Part : I {
      using Base = I;
      using Base::Base;
      Accum comb(Accum x) {
        Accum prev = Base::get();
        Base::set(x);
        return I::comb(Accum(x - prev));
      }
    };
  };
  template<typename Accum>
  using CombChain = Chain<CombLogic<Accum>, Data<Accum>>;

  template<typename Accum>
  struct CombTerminal { static Accum comb(Accum x) { return x; } };

  template<typename Accum>
  struct Comb : APIOf<CombTerminal<Accum>, CombChain<Accum>> {
    using Base = APIOf<CombTerminal<Accum>, CombChain<Accum>>;
    using Base::Base;
    Accum step(Accum x) { return Base::comb(x); }
  };

  // N integrator stages (high rate) -> decimate by R -> N comb stages
  // (low rate). Genuinely breaks every other OneHLS component's 1-in/
  // 1-out .step() contract -- bool& valid: call every HIGH-rate cycle,
  // valid==true exactly 1-in-R calls, and only then does the returned
  // Accum carry a new, meaningful output.
  template<typename Sample, typename Accum, int32_t N, int32_t R>
  struct CicDecimator {
    Accumulator<Accum,Accum> integrator[N];
    Comb<Accum> comb[N];
    int32_t counter{0};
    Accum lastValid{};

    Accum step(Sample x, bool& valid) {
      Accum v = Accum(x);
      for (int32_t i = 0; i < N; ++i) v = integrator[i].step(v);
      valid = false;
      if (++counter == R) {
        counter = 0;
        Accum d = v;
        for (int32_t i = 0; i < N; ++i) d = comb[i].step(d);
        lastValid = d;
        valid = true;
      }
      return lastValid;
    }
  };
}
