/**
 * @file transpose2d.h
 * @brief Round 11 — layout transpose for a streaming raster + channel concat.
 *
 * Transpose2D<T,H,W,C>: NHWC <-> NCHW for a streaming frame. Unlike every
 * buffered primitive so far, a genuine channel transpose of a *stream*
 * needs the WHOLE H*W*C tensor buffered: output position c*HW+p reads
 * input position p*C+c, and NCHW demands channel 0's LAST pixel before
 * channel 1's FIRST -- so nothing can be emitted early. Depth is still
 * statically derivable (H*W*C is a constant) but it is NOT the small
 * (K-1)*W*C line-buffer cost. First primitive where "static" != "small".
 *
 * ChannelConcat<T,C1,C2>: two same-spatial-shape streams, different
 * channel counts, bundled into one C1+C2 stream. Cycle-aligned wiring;
 * latency skew between the branches is a Delay upstream, never a Fifo.
 */
#pragma once
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  template<typename T, int32_t H, int32_t W, int32_t C>
  struct Transpose2D {
    static constexpr int32_t Frame = H * W * C;   // full-frame reorder buffer
    T       buf[Frame]{};
    int32_t wr{0};       // NHWC write cursor (raster: p*C + c)
    int32_t rd{0};       // NCHW read cursor  (c*HW + p), advanced after the frame

    /// Phase 1: call Frame times with the NHWC stream (feeds buf).
    void write(T v) { buf[wr] = v; wr = (wr + 1 == Frame) ? 0 : wr + 1; }

    /// Phase 2: call Frame times to read the NCHW stream out.
    T readNCHW() {
      // rd counts 0..Frame-1 in NCHW order; map to NHWC index.
      int32_t c = rd / (H * W);
      int32_t p = rd - c * (H * W);
      T v = buf[p * C + c];
      rd = (rd + 1 == Frame) ? 0 : rd + 1;
      return v;
    }
  };

  template<typename T, int32_t C1, int32_t C2>
  struct ChannelConcat {
    static constexpr int32_t Cout = C1 + C2;
    /// a[C1] and b[C2] for the SAME pixel, same cycle -> out[C1+C2]. Wiring.
    void step(const T a[C1], const T b[C2], T out[C1 + C2]) const {
      for (int32_t i = 0; i < C1; ++i) out[i]      = a[i];
      for (int32_t i = 0; i < C2; ++i) out[C1 + i] = b[i];
    }
  };

}
