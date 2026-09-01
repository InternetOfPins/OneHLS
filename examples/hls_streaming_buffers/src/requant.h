/**
 * @file requant.h
 * @brief Round 8 sketch — per-layer requantisation between composed layers.
 *
 * A trained quantized net rescales each layer's accumulator back to the
 * activation type before the next layer: out = (acc * M) >> Shift, with a
 * per-layer integer multiplier M and shift (the standard multiplier-based
 * requant; M==1 gives the shift-only case).
 *
 * This composes as a layer-output operation, like bias -- not a new
 * meta-component. Conv2D could take M/Shift as NTTPs directly; kept
 * standalone here so the seam is explicit for the Round 8 write-up.
 *
 * NOT yet validated against a real quantized reference (Brevitas/QONNX) --
 * that is the flagged next gap, see HANDOFF.md Round 8.
 */
#pragma once
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  template<typename Sample, typename Accum, int32_t M, int32_t Shift>
  struct Requant {
    static_assert(Shift >= 0, "Shift >= 0");
    Sample apply(Accum a) const {
      Accum s = Accum(a * Accum(M));
      return Sample(s >> Shift);           // arithmetic; Sample's own quant mode on narrow
    }
  };

}
