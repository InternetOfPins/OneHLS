/**
 * @file two_layer_net.h
 * @brief Round 7 — conv -> 2x2 max-pool -> conv, as one synchronous step()
 * chain. The point: rate changes between stages (layer 1 emits a pixel per
 * valid window; pool emits 1 per 4; layer 2 consumes those) are absorbed
 * by the fire strobe threaded through, NOT by an inter-stage FIFO.
 *
 * Each layer's own line buffer is sized from ITS input feature-map width,
 * which is a static function of the preceding shapes -- no measurement.
 * Cin = Cout = 1 throughout (the composition question, not channel count).
 *
 * Requantisation between layers is a real concern in a trained net; here
 * Sample is used end to end (wide enough) and the cast is explicit so the
 * seam is visible.
 */
#pragma once
#include "conv2d.h"
#include "pool2d.h"

namespace oneHLS {

  // W1 = Width-2 (layer-1 valid conv), Wp = W1/2 (pool), layer 2 runs on Wp.
  template<typename Sample, typename Accum, int32_t Width,
           int32_t B1, int32_t A0,int32_t A1,int32_t A2,int32_t A3,int32_t A4,int32_t A5,int32_t A6,int32_t A7,int32_t A8,
           int32_t B2, int32_t C0,int32_t C1,int32_t C2,int32_t C3,int32_t C4,int32_t C5,int32_t C6,int32_t C7,int32_t C8>
  struct TwoLayerNetK3 {
    static constexpr int32_t W1 = Width - 2;      // layer-1 output width
    static constexpr int32_t Wp = W1 / 2;         // pooled width
    static constexpr int32_t W2 = Wp - 2;         // layer-2 output width
    static_assert(W1 % 2 == 0, "Width-2 must be even for 2x2 pool");

    Conv2D<Sample, Accum, Width, 3, 1, B1, A0,A1,A2,A3,A4,A5,A6,A7,A8> l1;
    Pool2D<op::Max, Sample, W1, 2>                                     pool;
    Conv2D<Sample, Accum, Wp, 3, 1, B2, C0,C1,C2,C3,C4,C5,C6,C7,C8>    l2;

    /// one input pixel in; on a cycle where layer 2 emits, out is set and
    /// the return is true.
    bool step(Sample in, Accum& out) {
      Accum a;
      if (!l1.step(in, a)) return false;
      Sample b;
      if (!pool.step(Sample(a), b)) return false;   // requant seam (explicit)
      return l2.step(b, out);
    }
  };

}
