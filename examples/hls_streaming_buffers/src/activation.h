/**
 * @file activation.h
 * @brief Round 13 — activation slot.
 *
 * Gate (HANDOFF R13): the ReLU family is NOT new machinery — it is
 * oneHLS::op::Max / op::Min (Round 4) applied with a constant operand.
 * `Activation<Fn>` is a named composition slot after a layer's output,
 * ~0 new logic. sigmoid/tanh (LUT / ac_math CORDIC, cf. Nco) is the part
 * that would be real work — deferred, not built here.
 *
 * Fn is a stateless functor: `T operator()(T) const`.
 */
#pragma once
#include "reduce_tree.h"   // op::Max / op::Min
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  namespace act {
    template<typename T>
    struct Relu {
      T operator()(T x) const { return op::Max::template apply<T>(T(0), x); }
    };
    /// clamped ReLU: min(max(0,x), Cap) -- Cap in raw units of T
    template<typename T, int32_t Cap>
    struct Relu6 {
      T operator()(T x) const {
        return op::Min::template apply<T>(op::Max::template apply<T>(T(0), x), T(Cap));
      }
    };
    /// leaky: x>=0 ? x : x>>Shift  (Shift-bit negative-slope, multiplier-free)
    template<typename T, int32_t Shift>
    struct LeakyRelu {
      T operator()(T x) const { return (x < T(0)) ? T(x >> Shift) : x; }
    };
    template<typename T>
    struct Identity { T operator()(T x) const { return x; } };
  }

  template<typename Fn>
  struct Activation {
    Fn fn{};
    template<typename T>
    T apply(T x) const { return fn(x); }
  };

}
