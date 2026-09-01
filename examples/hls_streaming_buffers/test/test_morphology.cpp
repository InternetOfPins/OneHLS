/**
 * @file native_test_r16.cpp
 * @brief Round 16 — streaming erode/dilate (no new primitive) vs a
 * plain nested-loop morphology reference.
 *   g++ -std=c++17 -I. native_test_r16.cpp -o /tmp/r16 && /tmp/r16
 */
#include "../src/morph.h"
#include <cstdio>
#include <vector>

using oneHLS::Erode;
using oneHLS::Dilate;

static int fails = 0;
static void check(const char* w, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", w);
  if (!ok) ++fails;
}

static constexpr int W = 9, H = 7, K = 3;
static constexpr int Wo = W - K + 1, Ho = H - K + 1;

// a little binary-ish image with a hole and a speckle
static int img(int r, int c) {
  if (r == 3 && c == 4) return 0;         // hole in a bright region
  if (r == 1 && c == 7) return 255;       // isolated speckle
  return (r >= 2 && r <= 4 && c >= 2 && c <= 6) ? 200 : 30;
}

static int ref_min(int orow, int ocol) {
  int m = 255;
  for (int kr = 0; kr < K; ++kr) for (int kc = 0; kc < K; ++kc)
    if (img(orow + kr, ocol + kc) < m) m = img(orow + kr, ocol + kc);
  return m;
}
static int ref_max(int orow, int ocol) {
  int m = 0;
  for (int kr = 0; kr < K; ++kr) for (int kc = 0; kc < K; ++kc)
    if (img(orow + kr, ocol + kc) > m) m = img(orow + kr, ocol + kc);
  return m;
}

template<typename Filt>
static std::vector<int> run(Filt& f) {
  std::vector<int> out;
  for (int r = 0; r < H; ++r)
    for (int c = 0; c < W; ++c) {
      int o;
      if (f.step(img(r, c), o)) out.push_back(o);
    }
  return out;
}

int main() {
  std::printf("== Round 16: streaming morphology (erode / dilate) ==\n");
  std::printf("no new primitive: WindowExtract (R2) + ReduceTree<Min|Max> (R4)\n");
  std::printf("image %dx%d -> %dx%d\n", W, H, Wo, Ho);

  {
    Erode<int, W, K> e;
    auto got = run(e);
    bool ok = (int)got.size() == Wo * Ho;
    int i = 0;
    for (int r = 0; r < Ho && ok; ++r) for (int c = 0; c < Wo; ++c, ++i)
      if (got[i] != ref_min(r, c)) ok = false;
    check("erode == nested-loop min over 3x3", ok);
    // erosion removes the isolated speckle (min pulls it to background)
    check("erosion kills the isolated 255 speckle", got[0] == 30);
  }
  {
    Dilate<int, W, K> d;
    auto got = run(d);
    bool ok = (int)got.size() == Wo * Ho;
    int i = 0;
    for (int r = 0; r < Ho && ok; ++r) for (int c = 0; c < Wo; ++c, ++i)
      if (got[i] != ref_max(r, c)) ok = false;
    check("dilate == nested-loop max over 3x3", ok);
    // dilation fills the single-pixel hole (max pulls it back to 200)
    // hole at img(3,4): output pos (2,3) window covers rows 2-4 cols 3-5
    check("dilation fills the 1-px hole", got[2 * Wo + 3] == 200);
  }

  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
