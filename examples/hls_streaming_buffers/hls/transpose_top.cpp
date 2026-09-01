// Round 11 synthesis: Transpose2D full-frame reorder buffer + ChannelConcat.
// Check: Transpose2D binds exactly H*W*C storage (static, but full frame --
// the first primitive where "statically derivable" != "small"); Concat is
// pure wiring, no state.
#include "../src/transpose2d.h"
#include <ac_fixed.h>
#include <cstdint>
using S = ac_fixed<16, 8, true>;

// 16x16x8 -> Frame = 2048 elements
static oneHLS::Transpose2D<S, 16, 16, 8> tr;
static oneHLS::ChannelConcat<S, 3, 5>    cc;

int32_t oneHlsTransposeWrTop(int16_t v) { tr.write(S(v)); return 0; }
int32_t oneHlsTransposeRdTop()          { return tr.readNCHW().to_int(); }

void oneHlsChannelConcatTop(int16_t a0,int16_t a1,int16_t a2,
                            int16_t b0,int16_t b1,int16_t b2,int16_t b3,int16_t b4,
                            int32_t out[8]) {
  S a[3] = { S(a0), S(a1), S(a2) };
  S b[5] = { S(b0), S(b1), S(b2), S(b3), S(b4) };
  S o[8];
  cc.step(a, b, o);
  for (int i = 0; i < 8; ++i) out[i] = o[i].to_int();
}
