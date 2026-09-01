/**
 * @file morph.h
 * @brief Round 16 — streaming grayscale morphology (erode / dilate).
 *
 * The point of this file is what it *doesn't* contain: no new primitive.
 * A streaming KxK erosion is exactly
 *   WindowExtract<T,Width,1,K>  +  ReduceTree<op::Min, T, K*K>
 * and dilation swaps in op::Max. Both are Rounds 2 and 4, unchanged, used
 * for a problem with no convolution, no pooling, no neural network
 * anywhere in it. That second, unrelated consumer is what earns the core
 * primitives the word "general" (same discipline as Round 3's gate).
 *
 * Square structuring element, stride 1, 'valid' region (output is
 * (W-K+1) x (H-K+1)).
 */
#pragma once
#include "window_extract.h"
#include "reduce_tree.h"
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  template<typename Op, typename T, int32_t Width, int32_t K>
  struct Morph {
    WindowExtract<T, Width, 1, K> we;
    ReduceTree<Op, T, K * K>      tree;

    /// one pixel in; on a valid interior position fills `out` and returns true.
    bool step(T x, T& out) {
      T i[1] = { x };
      T w[K][K][1];
      if (!we.step(i, w)) return false;
      T flat[K * K];
      for (int32_t r = 0; r < K; ++r)
        for (int32_t c = 0; c < K; ++c)
          flat[r * K + c] = w[r][c][0];
      out = tree.reduce(flat);
      return true;
    }
  };

  template<typename T, int32_t Width, int32_t K>
  struct Erode  : Morph<op::Min, T, Width, K> {};

  template<typename T, int32_t Width, int32_t K>
  struct Dilate : Morph<op::Max, T, Width, K> {};

}
