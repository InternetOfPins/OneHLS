// Round 8 throughput probe: same 2-layer net, but the top is marked
// PIPELINE II=1 -- does the composition accept a one-pixel-per-cycle
// initiation interval (matching hls4ml's per-layer II=1), or does
// something in the primitives force II>1?
#include "../src/two_layer_net.h"
#include <oneHLS/ac_types_support.h>
#include <ac_fixed.h>
#include <cstdint>

#if AC_VERSION < 4
#error "ac_types fork"
#endif

using Sample = ac_fixed<16, 8,  true>;
using Accum  = ac_fixed<32, 16, true>;

static oneHLS::TwoLayerNetK3<Sample, Accum, 12,
    100, 26,-77,141,-8,210,-119,45,-163,92,
    -40, -33,64,-5,128,-200,17,-71,9,150> net;

void oneHlsTwoLayerPipeTop(int16_t x, int32_t* out, bool* fire) {
#pragma HLS PIPELINE II=1
  Accum o;
  bool f = net.step(Sample(x), o);
  *fire = f;
  *out  = f ? o.to_int() : 0;
}
