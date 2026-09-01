/**
 * @file reduce_tree.h
 * @brief Round 4 — ReduceTree<Op, T, N>: balanced binary-tree reduction of
 * N values of T, combined pairwise by Op. One primitive, two use sites:
 * Op=op::Add (conv accumulate / avg-pool sum), Op=op::Max (max-pool).
 *
 * Stateless (like Dot). The tree is structural: ReduceTree<Op,T,N> splits
 * N into N/2 + (N-N/2) and recurses, so combine depth is ceil(log2 N),
 * visible in the type -- not a linear N-deep Chain fold.
 */
#pragma once
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  namespace op {
    struct Add { template<typename T> static T apply(T a, T b) { return T(a + b); } };
    struct Max { template<typename T> static T apply(T a, T b) { return a < b ? b : a; } };
    struct Min { template<typename T> static T apply(T a, T b) { return b < a ? b : a; } };
  }

  template<typename Op, typename T, int32_t N>
  struct ReduceTree {
    static_assert(N >= 1, "N >= 1");
    static constexpr int32_t Half = N / 2;
    static constexpr int32_t Depth =
        1 + (ReduceTree<Op,T,Half>::Depth > ReduceTree<Op,T,N-Half>::Depth
             ? ReduceTree<Op,T,Half>::Depth : ReduceTree<Op,T,N-Half>::Depth);
    static constexpr int32_t LatencyBound = Depth;   // one HW stage per combine level

    T reduce(const T in[N]) const {
      ReduceTree<Op,T,Half>     lo;
      ReduceTree<Op,T,N - Half> hi;
      return Op::template apply<T>(lo.reduce(in), hi.reduce(in + Half));
    }
  };

  template<typename Op, typename T>
  struct ReduceTree<Op, T, 1> {
    static constexpr int32_t Half = 0;
    static constexpr int32_t Depth = 0;
    static constexpr int32_t LatencyBound = 0;
    T reduce(const T in[1]) const { return in[0]; }
  };

}
