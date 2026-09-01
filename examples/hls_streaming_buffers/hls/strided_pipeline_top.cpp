// Round 5 synthesis target: the WindowExtract -> Stride<S> -> ReduceTree
// pipeline. Check: rate mismatch handled by a strobe, not a buffer --
// report should show WindowExtract's line store + one small stride
// counter + a combinational adder tree, NO FIFO, no stall/restart logic.
#include "../src/strided_window_reduce.h"
#include <ac_fixed.h>
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16, 16, true>;

// Width=8, K=3, Op=Add, Stride=2.
static oneHLS::StridedWindowReduce<Sample, 8, 3, oneHLS::op::Add, 2> pipe;

void oneHlsStridedPipelineTop(int16_t x, int32_t* out, bool* fire) {
  Sample o;
  bool f = pipe.step(Sample(x), o);
  *fire = f;
  *out  = f ? o.to_int() : 0;
}
