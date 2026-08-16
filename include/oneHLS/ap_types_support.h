/**
 * @file ap_types_support.h
 * @brief RawBitsCtor specialization for Xilinx/AMD ap_types
 *        (github.com/Xilinx/HLS_arbitrary_Precision_Types) -- raw bit
 *        pattern via .range(Hi,Lo)=, a DIFFERENT accessor than ac_fixed's
 *        set_slc (confirmed while porting the biquad -- see
 *        apTypesHLS/HANDOFF.md; ap_range_ref::to_int() also reads as
 *        unsigned, unlike ac_fixed's slc<>(), unrelated to construction
 *        but worth remembering if this type is read back the same way).
 *        Opt-in: include this alongside oneHLS.h only when using
 *        ap_int/ap_fixed as the Sample/Accum type. NOTE: real upstream
 *        ap_types has been observed to not synthesize under Bambu HLS in
 *        practical time (see apTypesHLS/HANDOFF.md) -- this
 *        specialization is verified at the native/composition level
 *        only, not synthesis-verified end to end.
 */
#pragma once
#include <oneHLS/oneHLS.h>
#include <ap_fixed.h>
#include <ap_int.h>

namespace oneHLS {
  template<int W, int I, ap_q_mode Q, ap_o_mode O, int N>
  struct RawBitsCtor<ap_fixed<W,I,Q,O,N>> {
    template<int32_t RawBits>
    static inline ap_fixed<W,I,Q,O,N> make() {
      ap_fixed<W,I,Q,O,N> c = 0;
      c.range(W-1, 0) = ap_int<W>(RawBits);
      return c;
    }
  };
}
