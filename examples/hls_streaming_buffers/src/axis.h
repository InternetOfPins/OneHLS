/**
 * @file axis.h
 * @brief Round 15 — AXI4-Stream boundary adapter (tvalid/tready/tlast).
 *
 * Gate (HANDOFF R15): every primitive through R14 assumes a CLOSED
 * pipeline — nothing ever stalls. AXI4-Stream at the PS↔PL edge adds
 * `tready`: the consumer can refuse a cycle, and the producer must then
 * hold. This is the first first-class "downstream not ready" signal.
 *
 * Design B (thin adapter at the ends, not tready threaded through every
 * stage): the internal fixed-latency pipeline stays backpressure-free.
 * At the output, a skid FIFO absorbs the in-flight results that keep
 * arriving for ~PipeDepth cycles after `tready` drops; the input is gated
 * so the pipeline is only fed when the skid FIFO has room. `tlast` comes
 * from the output-pixel position counter.
 *
 * NOT free: the skid FIFO is a real elastic buffer (Round 10's `Fifo`).
 * But its depth is the PIPELINE LATENCY (+ a couple handshake cycles) —
 * ~20 elements, static, one-per-stage — not the frame or a feature map.
 * First place in this whole plan an elastic buffer genuinely earns its
 * place, and it stays at the boundary.
 *
 * Skid depth is DERIVED, not hand-picked: `Pipe` must expose a
 * `static constexpr int32_t LatencyBound` (a structural upper bound on
 * its added HW pipeline stages — every composable primitive here has
 * one, summed through the composition). `AxisWrap` defaults `SkidDepth`
 * to `LatencyBound + HandshakeSlack` and refuses (static_assert) any
 * explicit `SkidDepth` below that. Discipline: synth once per pipeline
 * and confirm the reported pipeline `Depth <= Pipe::LatencyBound`
 * (it has held for every composition built — see HANDOFF R15/R16).
 */
#pragma once
#include "topology.h"     // Fifo
#ifdef __AVR__
  #include <stdint.h>
  #include <hapi/platform/avr/avr_std.h>
#else
  #include <cstdint>
  #include <type_traits>
#endif

namespace oneHLS {

  template<typename P, typename = void>
  struct HasLatencyBound : std::false_type {};
  template<typename P>
  struct HasLatencyBound<P, decltype(void(P::LatencyBound))> : std::true_type {};

  /// Wraps a fixed-latency pipeline object `P` with an out-stream signature
  ///   bool P::step(In in, Out& out)   // true when `out` is a real result
  /// Presents an AXI4-Stream master out. NOut = total output beats
  /// (compile-time: the pipeline's output-pixel count). SkidDepth defaults
  /// to the pipeline's own derived LatencyBound + slack; an explicit value
  /// below that is a compile error.
  template<typename In, typename Out, typename P, int32_t NOut,
           int32_t HandshakeSlack = 4,
           int32_t SkidDepth = P::LatencyBound + HandshakeSlack>
  struct AxisWrap {
    static_assert(HasLatencyBound<P>::value,
      "AxisWrap: Pipe must expose `static constexpr int32_t LatencyBound` "
      "(sum of its primitives' LatencyBound through the composition).");
    static_assert(SkidDepth >= P::LatencyBound + 2,
      "AxisWrap: SkidDepth below the pipeline's LatencyBound + handshake "
      "slack -> beats dropped under sustained backpressure.");

    P                       pipe;
    Fifo<Out, SkidDepth>    skid;
    int32_t                 emitted{0};   // output beats pushed to skid

    /// Feed one input beat if the pipeline can currently accept it.
    /// Returns true if `in` was consumed (AXIS input tready to upstream).
    bool feed(In in) {
      // leave headroom for in-flight results (pipeline latency worst case).
      if (skid.size() >= SkidDepth - 1) return false;
      Out o;
      if (pipe.step(in, o)) { skid.push(o); ++emitted; }
      return true;
    }

    /// AXIS master out handshake. On a cycle the consumer asserts tready:
    /// pop one beat if available, report tvalid/tlast.
    bool pull(bool tready, Out& tdata, bool& tvalid, bool& tlast) {
      tvalid = !skid.empty();
      tlast  = false;
      if (tready && tvalid) {
        skid.pop(tdata);
        tlast = (skidPopped + 1 == NOut);
        ++skidPopped;
        return true;
      }
      return false;
    }

    int32_t skidPopped{0};
  };

}
