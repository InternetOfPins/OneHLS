/**
 * @file native_test_r7.cpp
 * @brief Round 7 — conv -> 2x2 maxpool -> conv, vs a nested-loop reference.
 * Numeric + validity-threading, and a line-buffer-depth audit.
 *   g++ -std=c++17 -I. -I<oneHLS> -I<hapi> -I<oneData> native_test_r7.cpp -o /tmp/r7 && /tmp/r7
 */
#include "../src/two_layer_net.h"
#include <cstdio>
#include <vector>

using oneHLS::TwoLayerNetK3;

static int fails = 0;
static void check(const char* what, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

static constexpr int W = 12, H = 12;
static const int L1[9] = { 26,-77,141,-8,210,-119,45,-163,92 };  static const int B1 = 100;
static const int L2[9] = { -33,64,-5,128,-200,17,-71,9,150 };    static const int B2 = -40;

static int img(int r, int c) { return ((r * 5 + c * 3) % 23) - 11; }

// reference forward: valid conv3 -> 2x2 maxpool stride2 -> valid conv3
static void ref_forward(std::vector<long>& out, int& w2, int& h2) {
  const int W1 = W - 2, H1 = H - 2;
  std::vector<std::vector<long>> a(H1, std::vector<long>(W1));
  for (int r = 0; r < H1; ++r)
    for (int c = 0; c < W1; ++c) {
      long s = B1;
      for (int kr = 0; kr < 3; ++kr)
        for (int kc = 0; kc < 3; ++kc)
          s += (long)L1[kr*3+kc] * img(r+kr, c+kc);
      a[r][c] = s;
    }
  const int Hp = H1/2, Wp = W1/2;
  std::vector<std::vector<long>> p(Hp, std::vector<long>(Wp));
  for (int r = 0; r < Hp; ++r)
    for (int c = 0; c < Wp; ++c) {
      long m = a[2*r][2*c];
      for (int dr = 0; dr < 2; ++dr) for (int dc = 0; dc < 2; ++dc)
        if (a[2*r+dr][2*c+dc] > m) m = a[2*r+dr][2*c+dc];
      p[r][c] = m;
    }
  h2 = Hp - 2; w2 = Wp - 2;
  for (int r = 0; r < h2; ++r)
    for (int c = 0; c < w2; ++c) {
      long s = B2;
      for (int kr = 0; kr < 3; ++kr)
        for (int kc = 0; kc < 3; ++kc)
          s += (long)L2[kr*3+kc] * p[r+kr][c+kc];
      out.push_back(s);
    }
}

int main() {
  std::printf("== Round 7: conv -> maxpool -> conv ==\n");
  using Net = TwoLayerNetK3<int, long, W,
      B1, 26,-77,141,-8,210,-119,45,-163,92,
      B2, -33,64,-5,128,-200,17,-71,9,150>;

  std::printf("shapes: input %dx%d -> L1 %dx%d -> pool %dx%d -> L2 %dx%d\n",
              W, H, Net::W1, Net::W1, Net::Wp, Net::Wp, Net::W2, Net::W2);

  // --- line-buffer depth audit: each is a static function of its own
  //     input feature-map width, no measurement ---
  constexpr int l1Ring   = oneHLS::LineBuffer<int, W,        1, 3>::RingPixels;
  constexpr int poolRing  = oneHLS::LineBuffer<int, Net::W1, 1, 2>::RingPixels;
  constexpr int l2Ring   = oneHLS::LineBuffer<int, Net::Wp, 1, 3>::RingPixels;
  std::printf("L1 line ring   = %d  (K=3 over width %d -> 2*%d+1)\n", l1Ring,  W,        W);
  std::printf("pool line ring = %d  (P=2 over width %d -> 1*%d+1)\n", poolRing, Net::W1, Net::W1);
  std::printf("L2 line ring   = %d  (K=3 over width %d -> 2*%d+1)\n", l2Ring,  Net::Wp, Net::Wp);
  check("L1 ring == 2*W+1",     l1Ring   == 2*W + 1);
  check("pool ring == 1*W1+1",  poolRing == Net::W1 + 1);
  check("L2 ring == 2*Wp+1",    l2Ring   == 2*Net::Wp + 1);

  // --- numeric + validity threading ---
  std::vector<long> ref; int w2, h2;
  ref_forward(ref, w2, h2);

  std::vector<long> got;
  Net net;
  for (int r = 0; r < H; ++r)
    for (int c = 0; c < W; ++c) {
      long o;
      if (net.step(img(r, c), o)) got.push_back(o);
    }

  std::printf("ref outputs: %zu (%dx%d)   got: %zu\n", ref.size(), w2, h2, got.size());
  check("fire count == reference output count", got.size() == ref.size());
  bool eq = got.size() == ref.size();
  for (size_t i = 0; i < got.size() && i < ref.size(); ++i)
    if (got[i] != ref[i]) { eq = false;
      std::printf("   mismatch @%zu: got %ld want %ld\n", i, got[i], ref[i]); }
  check("every fired value == reference (numeric + position)", eq);

  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
