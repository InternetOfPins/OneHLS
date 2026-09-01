/**
 * @file line_buffer.h
 * @brief Round 1 — streaming 2D line buffer. Holds the last Taps-1 rows of
 * a Width x Channels image plus the current pixel, and emits the vertical
 * Taps-pixel strip at the current column every cycle.
 *
 * Plain struct + fixed C-array storage + compile-time-bounded loops, the
 * same shape CicDecimator / PolyphaseFirDecim / Nco use for above-the-atom
 * composites (see ../../examples/hls_cic_decimator/src/cic_decimator.h and
 * HANDOFF.md Round 0) -- not a Width*Channels-long Tap chain.
 *
 * Storage is a compile-time constant: Depth = ((Taps-1)*Width + 1) *
 * Channels elements of T. Round 1's whole point is confirming synthesis
 * instantiates exactly that, with no conservative sizing.
 */
#pragma once
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  template<typename T, int32_t Width, int32_t Channels, int32_t Taps>
  struct LineBuffer {
    static_assert(Width   >= 1, "Width >= 1");
    static_assert(Channels >= 1, "Channels >= 1");
    static_assert(Taps    >= 1, "Taps >= 1");

    /// pixel slots in the ring: Taps-1 whole rows + the current pixel
    static constexpr int32_t RingPixels = (Taps - 1) * Width + 1;
    /// storage footprint, in elements of T -- statically derivable, no runtime sizing
    static constexpr int32_t Depth = RingPixels * Channels;

    T       ring[RingPixels][Channels]{};
    int32_t wr{0};       // current write pixel slot
    int32_t filled{0};   // pixels streamed, saturating at RingPixels

    /// Push one pixel (Channels samples). Fill col with the vertical strip
    /// of Taps pixels at the current write column: col[Taps-1] is the pixel
    /// just written, col[0] the one Taps-1 rows above it. Returns true once
    /// that strip is fully populated (>= (Taps-1)*Width + 1 pixels streamed).
    bool step(const T in[Channels], T col[Taps][Channels]) {
      for (int32_t c = 0; c < Channels; ++c) ring[wr][c] = in[c];

      for (int32_t k = 0; k < Taps; ++k) {
        int32_t slot = wr - k * Width;
        if (slot < 0) slot += RingPixels;      // single wrap: k*Width < RingPixels
        for (int32_t c = 0; c < Channels; ++c) col[Taps - 1 - k][c] = ring[slot][c];
      }

      if (filled < RingPixels) ++filled;
      wr = (wr + 1 == RingPixels) ? 0 : wr + 1;
      return filled >= RingPixels;
    }
  };

}
