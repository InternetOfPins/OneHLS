// Round 2 synthesis target: oneHLS::WindowExtract<> composed on LineBuffer<>.
// The check is the seam -- confirm the report shows ONE line store (~17
// words, from the nested LineBuffer) + the KxK window as registers, and NO
// extra FIFO / channel buffer inserted between the two primitives.
#include "../src/window_extract.h"
#include <ac_fixed.h>
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16, 16, true>;

// Width=8, Channels=1, K=3  ->  LineBuffer RingPixels=17, window 3x3=9.
static oneHLS::WindowExtract<Sample, 8, 1, 3> we;

void oneHlsWindowExtractTop(int16_t x, int32_t win_out[9], bool* valid_out) {
  Sample in[1] = { Sample(x) };
  Sample w[3][3][1];
  bool v = we.step(in, w);
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      win_out[r * 3 + c] = w[r][c][0].to_int();
  *valid_out = v;
}
