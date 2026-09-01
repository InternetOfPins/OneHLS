// Round 12 synthesis: Upsample2D nearest (1 input row buffered) vs
// zero-stuff (no buffer). Check the buffer stays one row (W*C), not the
// frame -- i.e. it passes the "cheap" half of the static-depth claim,
// unlike Transpose2D.
#include "../src/upsample2d.h"
#include <ac_fixed.h>
#include <cstdint>
using S = ac_fixed<16, 8, true>;

// 16 wide, 8 channels, S=2  ->  nearest RowBuf = 128; zero RowBuf = 1
static oneHLS::Upsample2D<S, 16, 8, 2, oneHLS::UpMode::Nearest> upN;
static oneHLS::Upsample2D<S, 16, 8, 2, oneHLS::UpMode::Zero>    upZ;

void oneHlsUpNearestTop(int16_t x, int32_t out[8], bool* wantIn) {
  S in[8]; for (int i = 0; i < 8; ++i) in[i] = S(x);
  S o[8]; bool w;
  upN.step(in, o, w);
  *wantIn = w;
  for (int i = 0; i < 8; ++i) out[i] = o[i].to_int();
}

void oneHlsUpZeroTop(int16_t x, int32_t out[8], bool* wantIn) {
  S in[8]; for (int i = 0; i < 8; ++i) in[i] = S(x);
  S o[8]; bool w;
  upZ.step(in, o, w);
  *wantIn = w;
  for (int i = 0; i < 8; ++i) out[i] = o[i].to_int();
}
