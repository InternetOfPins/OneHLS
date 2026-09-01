// Round 7 synthesis target: conv -> 2x2 maxpool -> conv as ONE synchronous
// step() chain. The check: does the multi-layer composition synthesize
// with NO inter-stage FIFO on either toolchain, and does each layer's
// line store stay at its statically-derived size?
#include "../src/two_layer_net.h"
#include <oneHLS/ac_types_support.h>
#include <ac_fixed.h>
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16, 8,  true>;
using Accum  = ac_fixed<32, 16, true>;

// input 12x12 -> L1 10x10 -> pool 5x5 -> L2 3x3
static oneHLS::TwoLayerNetK3<Sample, Accum, 12,
    100, 26,-77,141,-8,210,-119,45,-163,92,
    -40, -33,64,-5,128,-200,17,-71,9,150> net;

void oneHlsTwoLayerTop(int16_t x, int32_t* out, bool* fire) {
  Accum o;
  bool f = net.step(Sample(x), o);
  *fire = f;
  *out  = f ? o.to_int() : 0;
}
