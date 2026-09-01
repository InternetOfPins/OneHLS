/**
 * @file conv2d.h
 * @brief Round 6 — a single streaming 2D conv layer composed from the
 * Round 1-5 primitives. Cin = 1. K x K kernel, 'valid' convolution,
 * stride S (2D-separable, via Stride2D).
 *
 * WindowFront = the shared per-pixel machinery (line buffer + edge gate +
 * 2D stride + flatten). Conv2D adds one Dot kernel; a Cout>1 layer is
 * WindowFront + N independent Dots over the one flat window.
 *
 * Weights / bias are Q-format raw-bit NTTPs, same convention as
 * oneHLS::Fir / oneHLS::Dot.
 */
#pragma once
#include "window_extract.h"
#include "stride2d.h"
#include "dot.h"
#include <oneHLS/oneHLS.h>   // rawCoeff

namespace oneHLS {

  template<typename Sample, int32_t Width, int32_t K, int32_t S>
  struct WindowFront {
    static constexpr int32_t Wout = Width - K + 1;
    static constexpr int32_t LatencyBound =
        WindowExtract<Sample, Width, 1, K>::LatencyBound + 2;  // + Stride2D + flatten

    WindowExtract<Sample, Width, 1, K> we;
    Stride2D<Wout, S>                  st;

    /// Push one pixel. If this is a fired output-grid position, fill
    /// flat[K*K] (row-major) with the window and return true.
    bool step(Sample in, Sample flat[K * K]) {
      Sample px[1] = { in };
      Sample w[K][K][1];
      bool wv = we.step(px, w);
      if (!st.fire(wv)) return false;
      for (int32_t r = 0; r < K; ++r)
        for (int32_t c = 0; c < K; ++c)
          flat[r * K + c] = w[r][c][0];
      return true;
    }
  };

  template<typename Sample, typename Accum,
           int32_t Width, int32_t K, int32_t S, int32_t Bias, int32_t... W>
  struct Conv2D {
    static_assert(sizeof...(W) == K * K, "Conv2D: need exactly K*K weights");
    static constexpr int32_t LatencyBound =
        WindowFront<Sample, Width, K, S>::LatencyBound
      + Dot<Sample, Accum, W...>::LatencyBound + 1;   // + bias add

    WindowFront<Sample, Width, K, S> front;
    Dot<Sample, Accum, W...>         kern;

    bool step(Sample in, Accum& out) {
      Sample flat[K * K];
      if (!front.step(in, flat)) return false;
      out = Accum(kern.dot(flat)) + Accum(rawCoeff<Sample, Bias>());
      return true;
    }
  };

}
