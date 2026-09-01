/**
 * @file pad2d.h
 * @brief Round 9 — SAME-style zero padding for a streaming raster.
 *
 * Distinct primitive, not a LineBuffer parameter: LineBuffer/WindowExtract
 * gate LATE (emit only once a window is fully real). Pad2D emits EARLY --
 * it drives the padded (W+2P) x (H+2P) output raster and synthesises a
 * zero at every border position, pulling a real input pixel only on the
 * interior cycles. Shape B in the other direction: a gated read, no
 * storage.
 *
 * step() is called once per OUTPUT (padded-frame) cycle. On interior
 * cycles it returns the supplied `in` and sets wantInput=true (the caller
 * must have advanced its input source for this call); on border cycles it
 * returns T(0), wantInput=false, and `in` is ignored.
 */
#pragma once
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  template<typename T, int32_t Width, int32_t Height, int32_t P>
  struct Pad2D {
    static_assert(P >= 0, "P >= 0");
    static constexpr int32_t Wp = Width  + 2 * P;
    static constexpr int32_t Hp = Height + 2 * P;

    int32_t col{0};
    int32_t row{0};

    T step(T in, bool& wantInput) {
      bool inside = (row >= P) & (row < P + Height) & (col >= P) & (col < P + Width);
      wantInput = inside;
      T out = inside ? in : T(0);
      if (++col == Wp) { col = 0; ++row; }
      return out;
    }

    void reset() { col = row = 0; }
  };

}
