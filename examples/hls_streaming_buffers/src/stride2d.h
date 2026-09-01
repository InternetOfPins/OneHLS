/**
 * @file stride2d.h
 * @brief Round 6 refinement of Round 5's Stride<S>.
 *
 * Stride<S> (stride.h) decimates a 1D stream: every S-th valid item in
 * raster order. Composing a real conv layer in Round 6 showed that is
 * NOT 2D conv stride -- a stride-2 conv keeps output pixels where BOTH
 * row and column are multiples of 2, not every 2nd pixel in raster
 * order. Stride2D<Wout,S> is the position-aware gate: it tracks the
 * output-grid position of each valid window and fires on (row%S==0 &&
 * col%S==0). Still Shape B -- a strobe, zero storage beyond two small
 * counters, never stalls.
 */
#pragma once
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  template<int32_t Wout, int32_t S>
  struct Stride2D {
    static_assert(Wout >= 1, "Wout >= 1");
    static_assert(S >= 1, "S >= 1");

    int32_t colPhase{0};   // 0..S-1 across the output row
    int32_t rowPhase{0};   // 0..S-1 down the output rows
    int32_t col{0};        // 0..Wout-1

    bool fire(bool upstreamValid) {
      if (!upstreamValid) return false;
      bool f = (colPhase == 0) && (rowPhase == 0);

      colPhase = (colPhase + 1 == S) ? 0 : colPhase + 1;
      if (++col == Wout) {
        col = 0;
        colPhase = 0;
        rowPhase = (rowPhase + 1 == S) ? 0 : rowPhase + 1;
      }
      return f;
    }

    void reset() { colPhase = rowPhase = col = 0; }
  };

}
