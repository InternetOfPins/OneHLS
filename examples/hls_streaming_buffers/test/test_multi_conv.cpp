/**
 * @file native_test_r14.cpp
 * @brief Round 14 — multi-channel Conv2D vs a full nested-loop reference,
 * plus an end-to-end storage estimate walked per-stage-per-resolution.
 *   g++ -std=c++17 -I. native_test_r14.cpp -o /tmp/r14 && /tmp/r14
 */
#include "../src/multi_conv.h"
#include "../src/line_buffer.h"
#include <cstdio>
#include <vector>

using oneHLS::MultiConv;
using oneHLS::LineBuffer;

static int fails = 0;
static void check(const char* w, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", w);
  if (!ok) ++fails;
}

static constexpr int W = 8, H = 7, Cin = 3, Cout = 4, K = 3, S = 1;
static constexpr int Wo = W - K + 1, Ho = H - K + 1;

static int img(int r, int c, int ci) { return ((r*7 + c*5 + ci*11) % 19) - 9; }
static int wt(int co, int t)         { return ((co*13 + t*3) % 17) - 8; }
static int bs(int co)                { return co * 10 - 15; }

static long ref(int orow, int ocol, int co) {
  long acc = bs(co);
  for (int kr = 0; kr < K; ++kr)
    for (int kc = 0; kc < K; ++kc)
      for (int ci = 0; ci < Cin; ++ci) {
        int t = kr*K*Cin + kc*Cin + ci;
        acc += (long)wt(co, t) * img(orow+kr, ocol+kc, ci);
      }
  return acc;
}

int main() {
  std::printf("== Round 14: multi-channel Conv2D ==\n");
  std::printf("in %dx%dx%d -> conv3x3 -> %dx%dx%d\n", W, H, Cin, Wo, Ho, Cout);

  MultiConv<int, long, W, Cin, Cout, K, S> conv;
  for (int co = 0; co < Cout; ++co) {
    conv.bias[co] = bs(co);
    for (int t = 0; t < conv.Taps; ++t) conv.w[co][t] = wt(co, t);
  }
  check("Taps == K*K*Cin == 27", conv.Taps == 27);

  std::vector<std::vector<long>> got;
  for (int r = 0; r < H; ++r)
    for (int c = 0; c < W; ++c) {
      int in[Cin]; for (int ci = 0; ci < Cin; ++ci) in[ci] = img(r, c, ci);
      long o[Cout];
      if (conv.step(in, o)) got.push_back(std::vector<long>(o, o + Cout));
    }

  check("fire count == Wo*Ho", (int)got.size() == Wo*Ho);
  bool ok = (int)got.size() == Wo*Ho;
  int idx = 0;
  for (int r = 0; r < Ho && ok; ++r)
    for (int c = 0; c < Wo; ++c, ++idx)
      for (int co = 0; co < Cout; ++co)
        if (got[idx][co] != ref(r, c, co)) { ok = false;
          std::printf("   (%d,%d) co=%d got %ld want %ld\n", r, c, co, got[idx][co], ref(r,c,co)); }
  check("every (pixel, out-channel) == nested-loop conv reference", ok);

  // --- end-to-end storage estimate, per stage at ITS OWN resolution ---
  // toy net: in 32x32x3 -> conv(3->16) -> pool2 -> conv(16->32)
  // (standing note: NOT one flat per-channel number * stages)
  std::printf("-- end-to-end line-buffer estimate (bytes, 2B/elt) --\n");
  auto ring = [](int w, int c, int k){ return ((k-1)*w + 1) * c; };
  int s1 = ring(32, 3, 3);       // conv1 line buffer: 32 wide, 3 ch, K=3
  int sp = ring(30, 16, 2);      // pool: conv1 out is 30 wide, 16 ch, P=2
  int s2 = ring(15, 16, 3);      // conv2: pooled 15 wide, 16 ch, K=3
  std::printf("   conv1(32w,3c)  ring=%d elts\n", s1);
  std::printf("   pool (30w,16c)  ring=%d elts\n", sp);
  std::printf("   conv2(15w,16c)  ring=%d elts\n", s2);
  std::printf("   total ~= %d elts (%d B)  -- vs a flat 3-stage*maxCH guess would mislead\n",
              s1+sp+sp*0 + s2, 2*(s1+sp+s2));
  check("each stage's ring uses ITS width & channel count, not a global",
        s1 == ((3-1)*32+1)*3 && s2 == ((3-1)*15+1)*16);

  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
