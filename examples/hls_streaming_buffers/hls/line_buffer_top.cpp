// Round 1 Bambu/Vitis synthesis target: oneHLS::LineBuffer<> at a fixed,
// statically-known shape. The point of the round is the resource report --
// confirm storage instantiates exactly Depth = ((Taps-1)*Width+1)*Channels
// elements, with no conservative FIFO sizing.
#include "../src/line_buffer.h"
#include <ac_fixed.h>
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16, 16, true>;

// Width=8, Channels=1, Taps=3  ->  RingPixels=17, Depth=17 Sample elements.
static oneHLS::LineBuffer<Sample, 8, 1, 3> lb;

// col_out carries the 3-pixel vertical strip; valid_out is the warmup flag.
void oneHlsLineBufferTop(int16_t x, int32_t col_out[3], bool* valid_out) {
  Sample in[1]  = { Sample(x) };
  Sample col[3][1];
  bool v = lb.step(in, col);
  for (int i = 0; i < 3; ++i) col_out[i] = col[i][0].to_int();
  *valid_out = v;
}
