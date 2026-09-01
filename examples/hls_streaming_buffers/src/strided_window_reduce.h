/**
 * @file strided_window_reduce.h
 * @brief Round 5 two-stage pipeline: WindowExtract -> Stride<S> -> ReduceTree.
 * Demonstrates the rate mismatch (a window every valid cycle in, one
 * reduced output every S windows out) handled by a gate, no buffer.
 * Channels fixed at 1 for the demo (flatten is row-major).
 */
#pragma once
#include "window_extract.h"
#include "stride.h"
#include "reduce_tree.h"

namespace oneHLS {

  template<typename T, int32_t Width, int32_t K, typename Op, int32_t S>
  struct StridedWindowReduce {
    WindowExtract<T, Width, 1, K> we;
    Stride<S>                     st;
    ReduceTree<Op, T, K * K>      rt;

    /// Push one pixel. On a fired cycle, out = reduce(current KxK window)
    /// and the return is true; otherwise out is untouched and the return
    /// is false.
    bool step(T in, T& out) {
      T    w[K][K][1];
      T    px[1] = { in };
      bool wv = we.step(px, w);

      if (!st.fire(wv)) return false;

      T flat[K * K];
      for (int32_t r = 0; r < K; ++r)
        for (int32_t c = 0; c < K; ++c)
          flat[r * K + c] = w[r][c][0];
      out = rt.reduce(flat);
      return true;
    }
  };

}
