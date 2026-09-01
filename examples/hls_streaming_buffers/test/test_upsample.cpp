/**
 * @file native_test_r12.cpp
 * @brief Round 12 — Upsample2D nearest + zero-stuff vs references.
 *   g++ -std=c++17 -I. native_test_r12.cpp -o /tmp/r12 && /tmp/r12
 */
#include "../src/upsample2d.h"
#include <cstdio>
#include <vector>

using oneHLS::Upsample2D;
using oneHLS::UpMode;

static int fails = 0;
static void check(const char* what, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

static constexpr int W = 4, H = 3, C = 1, S = 2;
static int img(int r, int c) { return r * 10 + c + 1; }   // 1..12

template<UpMode M>
static std::vector<int> run() {
  Upsample2D<int, W, C, S, M> up;
  std::vector<int> out;
  int ir = 0, ic = 0;
  for (int orow = 0; orow < H * S; ++orow)
    for (int ocol = 0; ocol < W * S; ++ocol) {
      int in[C] = { img(ir, ic) }, o[C]; bool want;
      up.step(in, o, want);
      out.push_back(o[0]);
      if (want) { if (++ic == W) { ic = 0; ++ir; } }
    }
  return out;
}

int main() {
  std::printf("== Round 12: Upsample2D ==\n");
  std::printf("in %dx%d, S=%d -> out %dx%d\n", W, H, S, W*S, H*S);

  // --- Nearest: out(or,oc) = img(or/S, oc/S) ---
  {
    std::printf("-- Nearest (block replicate) --\n");
    check("RowBuf == W*C",
          (Upsample2D<int,W,C,S,UpMode::Nearest>::RowBuf) == W * C);
    auto got = run<UpMode::Nearest>();
    bool ok = (int)got.size() == W*S*H*S;
    for (int orow = 0; orow < H*S && ok; ++orow)
      for (int ocol = 0; ocol < W*S; ++ocol)
        if (got[orow*W*S + ocol] != img(orow/S, ocol/S)) ok = false;
    check("out(or,oc) == img(or/S, oc/S)  [needs 1-row replay buffer]", ok);
  }

  // --- Zero-stuff: out = img at S-multiples, 0 elsewhere ---
  {
    std::printf("-- Zero-stuff (transposed-conv prep) --\n");
    check("RowBuf == 1 (no buffer)",
          (Upsample2D<int,W,C,S,UpMode::Zero>::RowBuf) == 1);
    auto got = run<UpMode::Zero>();
    bool ok = (int)got.size() == W*S*H*S;
    for (int orow = 0; orow < H*S && ok; ++orow)
      for (int ocol = 0; ocol < W*S; ++ocol) {
        int want = (orow % S == 0 && ocol % S == 0) ? img(orow/S, ocol/S) : 0;
        if (got[orow*W*S + ocol] != want) ok = false;
      }
    check("out == img at (S|or, S|oc), 0 elsewhere  [no buffer]", ok);
  }

  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
