/**
 * @file saturate.h
 * @brief Round 13 — explicit overflow policy at a width boundary.
 *
 * Gate (HANDOFF R13): related to `Requant` but distinct. `Requant` is a
 * value transform (scale + shift + narrow) that currently makes an
 * *implicit* overflow choice — whatever the destination type's mode is.
 * `Saturate` is that choice, made explicit and testable, and split out so
 * it composes independently.
 *
 * Honest scope: for `ac_fixed`/`ap_fixed` the saturation mode is a type
 * parameter (`AC_SAT`, …) and you would normally use that. `Saturate<T,…>`
 * earns its place for (a) plain arithmetic types with no such mode,
 * (b) making the policy visible at the composition site instead of buried
 * in a typedef, and (c) the flag-and-pass variant `ac_fixed` does not do.
 *
 * Lo/Hi are in raw units of T.
 */
#pragma once
#include "reduce_tree.h"   // op::Max / op::Min
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  /// clamp to [Lo, Hi].
  template<typename T, int32_t Lo, int32_t Hi>
  struct SaturateClamp {
    T apply(T x) const {
      return op::Min::template apply<T>(op::Max::template apply<T>(x, T(Lo)), T(Hi));
    }
  };

  /// clamp to [Lo, Hi] AND raise a sticky overflow flag (read/clear via ovf).
  template<typename T, int32_t Lo, int32_t Hi>
  struct SaturateFlag {
    bool ovf{false};
    T apply(T x) {
      if (x < T(Lo)) { ovf = true; return T(Lo); }
      if (x > T(Hi)) { ovf = true; return T(Hi); }
      return x;
    }
    bool flagged() const { return ovf; }
    void clear() { ovf = false; }
  };

}
