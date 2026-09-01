// Round 10 synthesis: Delay vs Fifo cost, and a residual conv block whose
// skip path is a static Delay (no elastic buffer).
#include "../src/topology.h"
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

static oneHLS::Delay<Sample, 32> dly;
static oneHLS::Fifo<Sample, 32>  fifo;

// depth-32 shift register: one input, one output
int32_t oneHlsDelayTop(int16_t x) { return dly.step(Sample(x)).to_int(); }

// elastic FIFO: push on even calls, pop on odd, expose occupancy
int32_t oneHlsFifoTop(int16_t x, bool doPush, bool* ok, int32_t* occ) {
  Sample v = 0;
  if (doPush) *ok = fifo.push(Sample(x));
  else        *ok = fifo.pop(v);
  *occ = fifo.size();
  return v.to_int();
}

// residual conv block: SAME 3x3 conv branch + skip aligned by Delay.
static oneHLS::Pad2D<Sample, 16, 16, 1>       rpad;
static oneHLS::WindowFront<Sample, 18, 3, 1>  rfront;
static oneHLS::Dot<Sample, Accum, 26,-77,141,-8,210,-119,45,-163,92> rkern;
static oneHLS::Delay<Sample, 20>              rskip;   // ~ one padded row of latency

void oneHlsResidualTop(int16_t x, int32_t* out, bool* fire) {
  bool want;
  Sample px = rpad.step(Sample(x), want);
  Sample xd = rskip.step(want ? Sample(x) : Sample(0));   // skip branch, delayed
  Sample flat[9];
  bool f = rfront.step(px, flat);
  *fire = f;
  Accum c = Accum(rkern.dot(flat)) + Accum(oneHLS::rawCoeff<Sample,0>());
  *out = f ? (c + Accum(xd)).to_int() : 0;
}
