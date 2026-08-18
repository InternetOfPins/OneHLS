/**
 * @file nco_top.cpp
 * @brief Bambu synthesis target for the NCO example. Same device/clock
 * convention as every other synthesis target in this library:
 * xc7a100t-1csg324-VVD, 10ns clock (100MHz). No external input -- Nco is
 * a free-running oscillator, driven only by its own phase-accumulator
 * state across calls, same "state persists via a file/global-scope
 * instance across top-function calls" convention as every other target
 * here (Fir/Biquad/Pid/Accumulator/ComplexMac all use it too).
 */
#include <oneHLS/ac_types_support.h>
#include "../src/nco.h"
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16,1,true>;
using Accum  = ac_fixed<24,2,true>;

// PhaseInc = 0.25 (Q1.15 raw 8192) -- 8 steps per revolution, same as
// src/main.cpp's verified sequence.
nco_example::Nco<Sample, Accum, 8192> nco;

void oneHlsNcoTop(int32_t* out_re, int32_t* out_im) {
  oneHLS::Complex<Accum> r = nco.step();
  *out_re = r.re.to_int();
  *out_im = r.im.to_int();
}
