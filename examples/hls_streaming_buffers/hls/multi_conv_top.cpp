// Round 14 synthesis: multi-channel conv (Cin=4, Cout=4, K=3, W=16),
// weights loaded from a real ROM (not compile-time literals). This is the
// realistic case: a trained layer's weights are DATA -> Bambu can't
// strength-reduce them, so both toolchains use real multipliers.
#include "../src/multi_conv.h"
#include <ac_fixed.h>
#include <cstdint>
using S = ac_fixed<16, 8, true>;
using A = ac_fixed<32, 16, true>;

static const int16_t WROM[4 * 36] = {
  #define R9(o)  (o+3),(o-7),(o+11),(o-2),(o+21),(o-13),(o+4),(o-16),(o+9)
  R9(1),R9(2),R9(3),R9(4),   R9(-5),R9(6),R9(-7),R9(8),
  R9(9),R9(-10),R9(11),R9(-12),   R9(13),R9(14),R9(-15),R9(16),
};
static const int16_t BROM[4] = { 100, -40, 25, -60 };

static oneHLS::MultiConv<S, A, 16, 4, 4, 3, 1> conv;
static bool loaded = false;

void oneHlsMultiConvTop(int16_t x, int32_t out[4], bool* fire) {
  if (!loaded) {
    S wq[4 * 36]; A bq[4];
    for (int i = 0; i < 4 * 36; ++i) wq[i] = S(WROM[i]) / 256;
    for (int i = 0; i < 4; ++i)      bq[i] = A(BROM[i]) / 256;
    conv.load(wq, bq);
    loaded = true;
  }
  S in[4]; for (int i = 0; i < 4; ++i) in[i] = S(x + i);
  A o[4];
  bool f = conv.step(in, o);
  *fire = f;
  for (int i = 0; i < 4; ++i) out[i] = f ? o[i].to_int() : 0;
}
