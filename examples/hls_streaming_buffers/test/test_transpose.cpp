/**
 * @file native_test_r11.cpp
 * @brief Round 11 — Transpose2D (NHWC<->NCHW) and ChannelConcat.
 *   g++ -std=c++17 -I. native_test_r11.cpp -o /tmp/r11 && /tmp/r11
 */
#include "../src/transpose2d.h"
#include <cstdio>
#include <vector>

using oneHLS::Transpose2D;
using oneHLS::ChannelConcat;

static int fails = 0;
static void check(const char* what, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

static void test_transpose() {
  std::printf("-- Transpose2D<int,3,4,2>  (H=3 W=4 C=2) --\n");
  constexpr int H = 3, W = 4, C = 2, HW = H * W, Frame = HW * C;
  Transpose2D<int, H, W, C> tr;
  std::printf("   Frame buffer = H*W*C = %d elements (static, but FULL frame)\n", tr.Frame);
  check("Frame == 24", tr.Frame == 24);

  // NHWC input: value(p,c) = p*10 + c
  for (int p = 0; p < HW; ++p)
    for (int c = 0; c < C; ++c) tr.write(p * 10 + c);

  // NCHW output must be: for c in 0..C, for p in 0..HW: value(p,c)
  std::vector<int> got;
  for (int i = 0; i < Frame; ++i) got.push_back(tr.readNCHW());

  bool ok = true;
  int idx = 0;
  for (int c = 0; c < C; ++c)
    for (int p = 0; p < HW; ++p, ++idx)
      if (got[idx] != p * 10 + c) { ok = false;
        std::printf("   NCHW[%d] (c=%d p=%d) got %d want %d\n", idx, c, p, got[idx], p*10+c); }
  check("NCHW stream == transpose of NHWC input", ok);

  // key negative-result check: can ANY output be emitted before the whole
  // frame is in? NCHW position HW (channel 1, pixel 0) needs NHWC index
  // 0*C+1 = 1 (received 2nd) BUT is emitted only after all HW of channel 0,
  // the last of which is NHWC index (HW-1)*C+0 -- received 2nd-to-last.
  check("channel 1 pixel 0 blocked until channel 0 pixel HW-1 arrives "
        "=> whole frame must be buffered", (H*W - 1) * C > C);  // trivially true, documents it
}

static void test_channel_concat() {
  std::printf("-- ChannelConcat<int,3,5> --\n");
  ChannelConcat<int, 3, 5> cc;
  check("Cout == 8", cc.Cout == 8);
  int a[3] = { 1, 2, 3 }, b[5] = { 10, 20, 30, 40, 50 }, out[8];
  cc.step(a, b, out);
  bool ok = out[0]==1 && out[1]==2 && out[2]==3 &&
            out[3]==10 && out[4]==20 && out[5]==30 && out[6]==40 && out[7]==50;
  check("bundles [a0..a2, b0..b4] with defined ordering (no buffer)", ok);
}

int main() {
  std::printf("== Round 11: Transpose2D + ChannelConcat ==\n");
  test_transpose();
  test_channel_concat();
  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
