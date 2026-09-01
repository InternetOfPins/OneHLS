// Round 9 synthesis target: Pad2D -> WindowFront -> Dot == SAME conv.
// Check: Pad2D adds only two small position counters, no border buffer;
// the line store stays at the padded width's derived size; no FIFO.
#include "../src/pad2d.h"
#include "../src/conv2d.h"
#include <oneHLS/ac_types_support.h>
#include <ac_fixed.h>
#include <cstdint>

#if AC_VERSION < 4
#error "ac_types fork"
#endif

using Sample = ac_fixed<16, 8,  true>;
using Accum  = ac_fixed<32, 16, true>;

static oneHLS::Pad2D<Sample, 16, 16, 1>            pad;    // 16x16, SAME 3x3
static oneHLS::WindowFront<Sample, 18, 3, 1>       front;  // padded width 18
static oneHLS::Dot<Sample, Accum, 26,-77,141,-8,210,-119,45,-163,92> kern;

void oneHlsPadConvTop(int16_t x, int32_t* out, bool* fire, bool* wantIn) {
  bool w;
  Sample px = pad.step(Sample(x), w);
  *wantIn = w;
  Sample flat[9];
  bool f = front.step(px, flat);
  *fire = f;
  *out = f ? (Accum(kern.dot(flat)) + Accum(oneHLS::rawCoeff<Sample,100>())).to_int() : 0;
}
