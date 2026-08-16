/**
 * @file ac_types_support.h
 * @brief RawBitsCtor specialization for HLSLibs ac_types
 *        (github.com/hlslibs/ac_types) -- raw bit pattern via set_slc,
 *        the construction verified NOT to synthesize a static-const
 *        lazy-init guard under Bambu HLS (see acTypesHLS/HANDOFF.md).
 *        Opt-in: include this alongside oneHLS.h only when using
 *        ac_int/ac_fixed as the Sample/Accum type.
 */
#pragma once
#include <oneHLS/oneHLS.h>
#include <ac_fixed.h>
#include <ac_int.h>

namespace oneHLS {
  template<int W, int I, bool S, ac_q_mode Q, ac_o_mode O>
  struct RawBitsCtor<ac_fixed<W,I,S,Q,O>> {
    template<int32_t RawBits>
    static inline ac_fixed<W,I,S,Q,O> make() {
      ac_fixed<W,I,S,Q,O> c;
      c.set_slc(0, ac_int<W,true>(RawBits));
      return c;
    }
  };
}
