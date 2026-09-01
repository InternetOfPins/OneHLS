/**
 * @file window_extract.h
 * @brief Round 2 — KxK sliding window over a streaming image, composed on
 * top of LineBuffer (Taps=K), not reimplemented.
 *
 * LineBuffer supplies the vertical K-pixel strip at the current column
 * every cycle; WindowExtract adds a K-wide horizontal shift register of
 * those strips, so it holds the full KxK window. Storage it adds on top
 * of LineBuffer is exactly K*K*Channels elements of T -- the window
 * itself, nothing more; there is no second line store.
 */
#pragma once
#include "line_buffer.h"
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  template<typename T, int32_t Width, int32_t Channels, int32_t K>
  struct WindowExtract {
    static_assert(K >= 1, "K >= 1");

    LineBuffer<T, Width, Channels, K> lb;   // vertical strip source (Taps = K)
    T       win[K][K][Channels]{};          // [row][col], col K-1 = newest
    int32_t xcol{0};                        // column of the pixel being pushed, 0..Width-1

    /// CONSERVATIVE structural upper bound on added HW pipeline stages
    /// (strip read + K·K window shift + gate). Deliberately generous — the
    /// summed bound over a composition over-estimates the synthesised
    /// pipeline `Depth` (verified: 2-layer net bound 34 vs. real Depth 18,
    /// HANDOFF R15). Used to size AXIS skid FIFOs so the depth is derived,
    /// not hand-picked.
    static constexpr int32_t LatencyBound = 6;

    /// window footprint added on top of LineBuffer, elements of T
    static constexpr int32_t WinDepth = K * K * Channels;

    /// Push one pixel. Fill out with the current KxK window ([row][col][ch],
    /// col K-1 newest). Returns true only when the window is a real KxK
    /// image region: LineBuffer warmed up AND the K columns held don't
    /// straddle a row wrap (current column >= K-1).
    bool step(const T in[Channels], T out[K][K][Channels]) {
      T    strip[K][Channels];
      bool stripValid = lb.step(in, strip);

      for (int32_t r = 0; r < K; ++r)
        for (int32_t c = 0; c < K - 1; ++c)
          for (int32_t ch = 0; ch < Channels; ++ch)
            win[r][c][ch] = win[r][c + 1][ch];

      for (int32_t r = 0; r < K; ++r)
        for (int32_t ch = 0; ch < Channels; ++ch)
          win[r][K - 1][ch] = strip[r][ch];

      for (int32_t r = 0; r < K; ++r)
        for (int32_t c = 0; c < K; ++c)
          for (int32_t ch = 0; ch < Channels; ++ch)
            out[r][c][ch] = win[r][c][ch];

      bool xok = xcol >= K - 1;
      xcol = (xcol + 1 == Width) ? 0 : xcol + 1;
      return stripValid && xok;
    }
  };

}
