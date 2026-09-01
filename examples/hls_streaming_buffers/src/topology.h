/**
 * @file topology.h
 * @brief Round 10 — graph-topology primitives for non-linear streaming
 * graphs (residual/skip, concat, branch).
 *
 *   Delay<T,N>     fixed N-cycle shift register. Aligns a bypass branch to
 *                  the compile-time pipeline latency of the block it skips.
 *                  Static depth by construction -- N IS that latency.
 *   Fifo<T,Depth>  elastic buffer with occupancy: push/pop/full/empty.
 *                  The honest Shape A. Depth is a justified NTTP, never a
 *                  default. For clock-domain crossing / real AXI
 *                  backpressure / data-dependent branch skew -- cases a
 *                  static feed-forward CNN does NOT have (see HANDOFF R10).
 *   Concat<T,N>    bundle N synchronous streams into one N-vector. No
 *                  storage when the branches are cycle-aligned; skew is the
 *                  caller's job (Delay or Fifo upstream).
 */
#pragma once
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  template<typename T, int32_t N>
  struct Delay {
    static_assert(N >= 1, "Delay<N>: N >= 1 (N==0 is a bare wire)");
    T       buf[N]{};
    int32_t wr{0};

    T step(T in) {
      T out = buf[wr];
      buf[wr] = in;
      wr = (wr + 1 == N) ? 0 : wr + 1;
      return out;
    }
    void reset() { wr = 0; for (int32_t i = 0; i < N; ++i) buf[i] = T{}; }
  };

  template<typename T, int32_t Depth>
  struct Fifo {
    static_assert(Depth >= 1, "Fifo<Depth>: Depth >= 1");
    T       buf[Depth]{};
    int32_t rd{0}, wr{0}, count{0};

    bool full()  const { return count == Depth; }
    bool empty() const { return count == 0; }
    int32_t size() const { return count; }

    bool push(T v) {
      if (full()) return false;
      buf[wr] = v; wr = (wr + 1 == Depth) ? 0 : wr + 1; ++count;
      return true;
    }
    bool pop(T& v) {
      if (empty()) return false;
      v = buf[rd]; rd = (rd + 1 == Depth) ? 0 : rd + 1; --count;
      return true;
    }
    void reset() { rd = wr = count = 0; }
  };

  template<typename T, int32_t N>
  struct Concat {
    static_assert(N >= 1, "Concat<N>: N >= 1");
    // pack N cycle-aligned inputs into out[N]; pure wiring, no state.
    void step(const T in[N], T out[N]) const {
      for (int32_t i = 0; i < N; ++i) out[i] = in[i];
    }
  };

}
