/**
 * @file pool2d.h
 * @brief Round 7 — P x P pooling with stride P, composed from the same
 * primitives as a conv layer: WindowFront (line buffer + edge gate +
 * Stride2D, here K=S=P) feeding a ReduceTree<Op>. Op=op::Max -> max-pool,
 * Op=op::Add -> sum (divide downstream for average pool).
 *
 * Non-overlapping: stride == window size, so Stride2D fires once per
 * P x P tile. Output feature map is floor(Width/P) wide.
 */
#pragma once
#include "conv2d.h"      // WindowFront
#include "reduce_tree.h"

namespace oneHLS {

  template<typename Op, typename T, int32_t Width, int32_t P>
  struct Pool2D {
    static_assert(Width % P == 0, "Pool2D: P must divide Width");
    static constexpr int32_t Wout = Width / P;   // pooled feature-map width
    static constexpr int32_t LatencyBound =
        WindowFront<T, Width, P, P>::LatencyBound + ReduceTree<Op, T, P * P>::Depth;

    WindowFront<T, Width, P, P> front;
    ReduceTree<Op, T, P * P>    tree;

    bool step(T in, T& out) {
      T flat[P * P];
      if (!front.step(in, flat)) return false;
      out = tree.reduce(flat);
      return true;
    }
  };

}
