/**
 * @file multi_conv.h
 * @brief Round 14 — multi-channel 2D conv (Cin>1, Cout>1).
 *
 * output[co] = bias[co] + Σ_ci Σ_kr Σ_kc  W[co][kr*K*Cin + kc*Cin + ci]
 *                                          * window[kr][kc][ci]
 *
 * Composition: WindowExtract<T,Width,Cin,K> (already channel-aware, Round
 * 2) + Stride2D + Cout independent MACs over the one K*K*Cin flat window.
 * The per-output-channel MAC is a Dot over K*K*Cin taps -- the channel
 * reduction is just more taps in the same sum, no separate ReduceTree.
 *
 * Weights/bias are plain member arrays (become ROM under HLS) rather than
 * NTTPs -- Cout*K*K*Cin template parameters is impractical past toy sizes,
 * and a trained layer's weights come as data, not literals.
 */
#pragma once
#include "window_extract.h"
#include "stride2d.h"
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  template<typename T, typename Accum,
           int32_t Width, int32_t Cin, int32_t Cout, int32_t K, int32_t S>
  struct MultiConv {
    static constexpr int32_t Taps = K * K * Cin;
    static constexpr int32_t Wout = Width - K + 1;
    static constexpr int32_t LatencyBound =
        WindowExtract<T, Width, Cin, K>::LatencyBound + 2   // + Stride2D + flatten
      + (Taps <= 8 ? 5 : Taps <= 16 ? 6 : Taps <= 32 ? 7 : Taps <= 64 ? 8
         : Taps <= 128 ? 9 : Taps <= 256 ? 10 : 11);        // MAC adder tree

    WindowExtract<T, Width, Cin, K> we;
    Stride2D<Wout, S>               st;
    T     w[Cout][Taps];      // [co][kr*K*Cin + kc*Cin + ci]
    Accum bias[Cout];

    /// fill weights/bias from flat data (a trained layer's weights are data).
    void load(const T* wflat, const Accum* b) {
      for (int32_t co = 0; co < Cout; ++co) {
        bias[co] = b[co];
        for (int32_t t = 0; t < Taps; ++t) w[co][t] = wflat[co * Taps + t];
      }
    }

    /// one input pixel (Cin values). On a fired output-grid position,
    /// fills out[Cout] and returns true.
    bool step(const T in[Cin], Accum out[Cout]) {
      T win[K][K][Cin];
      bool v = we.step(in, win);
      if (!st.fire(v)) return false;

      T flat[Taps];
      for (int32_t kr = 0; kr < K; ++kr)
        for (int32_t kc = 0; kc < K; ++kc)
          for (int32_t ci = 0; ci < Cin; ++ci)
            flat[kr * K * Cin + kc * Cin + ci] = win[kr][kc][ci];

      for (int32_t co = 0; co < Cout; ++co) {
        Accum acc = bias[co];
        for (int32_t t = 0; t < Taps; ++t)
          acc = Accum(acc + Accum(w[co][t]) * Accum(flat[t]));
        out[co] = acc;
      }
      return true;
    }
  };

}
