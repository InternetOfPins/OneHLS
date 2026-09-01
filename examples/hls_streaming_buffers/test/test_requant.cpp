/**
 * @file native_test_r8.cpp
 * @brief Round 8 — Requant<> composes as a layer-output op; a conv layer
 * with integer requant matches an integer-quantized reference.
 *   g++ -std=c++17 -I. -I<oneHLS> -I<hapi> -I<oneData> native_test_r8.cpp -o /tmp/r8 && /tmp/r8
 */
#include "../src/conv2d.h"
#include "../src/requant.h"
#include <cstdio>
#include <vector>

using oneHLS::WindowFront;
using oneHLS::Dot;
using oneHLS::Requant;

static int fails = 0;
static void check(const char* what, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

static constexpr int W = 8, H = 8;
static const int KW[9] = { 26,-77,141,-8,210,-119,45,-163,92 };
static const int BIAS = 100, M = 3, SH = 9;   // requant: (acc*3) >> 9

static int img(int r, int c) { return ((r * 7 + c * 5) % 19) - 9; }

// integer-quantized reference: conv -> (acc*M)>>SH
static long ref(int orow, int ocol) {
  long acc = BIAS;
  for (int kr = 0; kr < 3; ++kr)
    for (int kc = 0; kc < 3; ++kc)
      acc += (long)KW[kr*3+kc] * img(orow+kr, ocol+kc);
  return (acc * M) >> SH;
}

int main() {
  std::printf("== Round 8: Requant composes as a layer-output op ==\n");

  WindowFront<int, W, 3, 1> front;
  Dot<int, long, 26,-77,141,-8,210,-119,45,-163,92> kern;
  Requant<int, long, M, SH> rq;

  std::vector<long> got;
  for (int r = 0; r < H; ++r)
    for (int c = 0; c < W; ++c) {
      int flat[9];
      if (front.step(img(r, c), flat)) {
        long acc = (long)kern.dot(flat) + BIAS;
        got.push_back(rq.apply(acc));            // <-- the requant seam
      }
    }

  const int Wout = W - 2, Hout = H - 2;
  std::vector<long> want;
  for (int r = 0; r < Hout; ++r)
    for (int c = 0; c < Wout; ++c) want.push_back(ref(r, c));

  check("output count == Wout*Hout", got.size() == want.size());
  bool eq = got.size() == want.size();
  for (size_t i = 0; i < got.size() && i < want.size(); ++i)
    if (got[i] != want[i]) { eq = false;
      std::printf("   @%zu got %ld want %ld\n", i, got[i], want[i]); }
  check("requantised conv == integer-quantized reference", eq);

  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
