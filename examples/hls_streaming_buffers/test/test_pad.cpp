/**
 * @file native_test_r9.cpp
 * @brief Round 9 — Pad2D + WindowFront + Dot == SAME 3x3 convolution.
 *   g++ -std=c++17 -I. -I<oneHLS> -I<hapi> -I<oneData> native_test_r9.cpp -o /tmp/r9 && /tmp/r9
 */
#include "../src/pad2d.h"
#include "../src/conv2d.h"     // WindowFront, Dot
#include <cstdio>
#include <vector>

using oneHLS::Pad2D;
using oneHLS::WindowFront;
using oneHLS::Dot;

static int fails = 0;
static void check(const char* what, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

static constexpr int W = 6, H = 5, P = 1;   // 3x3 SAME -> P=1
static const int KW[9] = { 26,-77,141,-8,210,-119,45,-163,92 };
static const int BIAS = 100;

static int img(int r, int c) { return ((r * 7 + c * 3) % 17) - 8; }

// SAME conv reference: output HxW, zero outside the image
static long ref_same(int orow, int ocol) {
  long acc = BIAS;
  for (int kr = 0; kr < 3; ++kr)
    for (int kc = 0; kc < 3; ++kc) {
      int ir = orow + kr - P, ic = ocol + kc - P;
      int v = (ir >= 0 && ir < H && ic >= 0 && ic < W) ? img(ir, ic) : 0;
      acc += (long)KW[kr*3+kc] * v;
    }
  return acc;
}

int main() {
  std::printf("== Round 9: Pad2D -> SAME conv ==\n");

  Pad2D<int, W, H, P> pad;
  WindowFront<int, W + 2*P, 3, 1> front;   // window front runs on the PADDED width
  Dot<int, long, 26,-77,141,-8,210,-119,45,-163,92> kern;

  std::printf("padded frame: %dx%d, output (SAME): %dx%d\n",
              pad.Wp, pad.Hp, W, H);

  // drive the padded raster; advance the input source only when wantInput
  std::vector<long> got;
  int ir = 0, ic = 0;            // input raster cursor
  for (int pr = 0; pr < pad.Hp; ++pr)
    for (int pc = 0; pc < pad.Wp; ++pc) {
      bool want;
      int nextIn = (ir < H) ? img(ir, ic) : 0;
      int px = pad.step(nextIn, want);
      if (want) { if (++ic == W) { ic = 0; ++ir; } }

      int flat[9];
      if (front.step(px, flat))
        got.push_back((long)kern.dot(flat) + BIAS);
    }

  std::vector<long> want;
  for (int r = 0; r < H; ++r) for (int c = 0; c < W; ++c) want.push_back(ref_same(r, c));

  check("output count == W*H (SAME preserves size)", got.size() == want.size());
  bool eq = got.size() == want.size();
  for (size_t i = 0; i < got.size() && i < want.size(); ++i)
    if (got[i] != want[i]) { eq = false;
      std::printf("   @%zu (%zu,%zu) got %ld want %ld\n", i, i/W, i%W, got[i], want[i]); }
  check("every output == SAME-conv reference (value + position)", eq);

  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
