/**
 * @file stride.h
 * @brief Round 5 — Stride<S>: a rate-control gate between streaming stages.
 *
 * Shape B (see HANDOFF.md Round 5): a valid/enable strobe, not a buffer.
 * Every stage still ticks every cycle; Stride<S> asserts fire() once per S
 * upstream-valid cycles, so a downstream consumer knows which cycles'
 * results to keep. Zero storage beyond one small counter. Never stalls or
 * restarts -- an un-fired cycle just doesn't assert.
 *
 * The wrapping counter is oneData::StaticRange<0,S-1,true>'s own
 * stepUp(_wraps=true), not a hand-rolled modulo.
 */
#pragma once
#include <oneData/oneData.h>
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  template<int32_t S>
  struct Stride {
    static_assert(S >= 1, "S >= 1");
    using Range = oneData::StaticRange<0, S - 1, true>;

    int32_t cnt{0};

    /// Tick once per upstream cycle, passing that cycle's upstream valid.
    /// Returns true on the cycles a strided downstream stage should fire
    /// (the 1st of every S upstream-valid cycles). S==1 -> always fire.
    bool fire(bool upstreamValid) {
      if (!upstreamValid) return false;
      bool f = (cnt == 0);
      cnt = Range::template stepUp<int32_t>(cnt, 1);
      return f;
    }

    void reset() { cnt = 0; }
  };

}
