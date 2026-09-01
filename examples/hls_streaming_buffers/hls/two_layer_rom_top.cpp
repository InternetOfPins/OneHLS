// Round 14b — the R7/R8 two-layer net (conv3x3 -> maxpool2 -> conv3x3),
// same shape (Cin=Cout=1, K=3, W=12), but weights loaded from ROM instead
// of NTTP literals. Isolates the literal-vs-ROM confound in R7/R8's DSP
// numbers, which the promotion decision (8d) leaned on.
#include "../src/multi_conv.h"
#include "../src/pool2d.h"
#include <ac_fixed.h>
#include <cstdint>
using S = ac_fixed<16, 8, true>;
using A = ac_fixed<32, 16, true>;

static const int16_t W1ROM[9] = { 26,-77,141,-8,210,-119,45,-163,92 };
static const int16_t W2ROM[9] = { -33,64,-5,128,-200,17,-71,9,150 };
static const int16_t B1ROM[1] = { 100 }, B2ROM[1] = { -40 };

static oneHLS::MultiConv<S, A, 12, 1, 1, 3, 1> l1;
static oneHLS::Pool2D<oneHLS::op::Max, S, 10, 2> pool;
static oneHLS::MultiConv<S, A, 5, 1, 1, 3, 1>  l2;
static bool loaded = false;

void oneHlsTwoLayerRomTop(int16_t x, int32_t* out, bool* fire) {
  if (!loaded) {
    S w1[9], w2[9]; A b1[1], b2[1];
    for (int i = 0; i < 9; ++i) { w1[i] = S(W1ROM[i]) / 256; w2[i] = S(W2ROM[i]) / 256; }
    b1[0] = A(B1ROM[0]) / 256; b2[0] = A(B2ROM[0]) / 256;
    l1.load(w1, b1); l2.load(w2, b2);
    loaded = true;
  }
  S in[1] = { S(x) };
  A a[1];
  if (!l1.step(in, a)) { *fire = 0; *out = 0; return; }
  S b;
  if (!pool.step(S(a[0]), b)) { *fire = 0; *out = 0; return; }
  S bi[1] = { b };
  A o[1];
  bool f = l2.step(bi, o);
  *fire = f;
  *out = f ? o[0].to_int() : 0;
}
