/**
 * @file main.cpp
 * @brief Native-only demo: build a streaming conv -> 2x2 max-pool -> conv
 * pipeline from the primitives in this directory and run it against a
 * plain nested-loop reference. No Bambu needed to run this file.
 *
 * The point being demonstrated is a BUFFERING one (see ../README.md):
 * this whole pipeline holds one line buffer per stage, sized from that
 * stage's own W and C, and NOTHING between the stages -- no inter-layer
 * FIFO. The `hls/` tops synthesise the same compositions; the per-round
 * verification lives in `../test/`.
 */
#include "two_layer_net.h"
#include "line_buffer.h"
#include <cstdio>
#include <vector>

using oneHLS::TwoLayerNetK3;
using oneHLS::LineBuffer;

static constexpr int W = 12, H = 12;
static const int L1[9] = { 26,-77,141,-8,210,-119,45,-163,92 };  static const int B1 = 100;
static const int L2[9] = { -33,64,-5,128,-200,17,-71,9,150 };    static const int B2 = -40;
static int img(int r, int c) { return ((r * 5 + c * 3) % 23) - 11; }

static void ref_forward(std::vector<long>& out) {
  const int W1 = W - 2, H1 = H - 2;
  std::vector<std::vector<long>> a(H1, std::vector<long>(W1));
  for (int r = 0; r < H1; ++r) for (int c = 0; c < W1; ++c) {
    long s = B1;
    for (int kr = 0; kr < 3; ++kr) for (int kc = 0; kc < 3; ++kc)
      s += (long)L1[kr*3+kc] * img(r+kr, c+kc);
    a[r][c] = s;
  }
  const int Hp = H1/2, Wp = W1/2;
  std::vector<std::vector<long>> p(Hp, std::vector<long>(Wp));
  for (int r = 0; r < Hp; ++r) for (int c = 0; c < Wp; ++c) {
    long m = a[2*r][2*c];
    for (int dr = 0; dr < 2; ++dr) for (int dc = 0; dc < 2; ++dc)
      if (a[2*r+dr][2*c+dc] > m) m = a[2*r+dr][2*c+dc];
    p[r][c] = m;
  }
  for (int r = 0; r < Hp-2; ++r) for (int c = 0; c < Wp-2; ++c) {
    long s = B2;
    for (int kr = 0; kr < 3; ++kr) for (int kc = 0; kc < 3; ++kc)
      s += (long)L2[kr*3+kc] * p[r+kr][c+kc];
    out.push_back(s);
  }
}

int main() {
  using Net = TwoLayerNetK3<int, long, W,
      B1, 26,-77,141,-8,210,-119,45,-163,92,
      B2, -33,64,-5,128,-200,17,-71,9,150>;

  std::printf("conv3x3 -> maxpool2 -> conv3x3   (input %dx%d)\n", W, H);

  // line-buffer footprint, per stage, at that stage's own width & channels
  const int l1  = LineBuffer<int, W,        1, 3>::RingPixels;   // 2*12+1
  const int pl  = LineBuffer<int, Net::W1,  1, 2>::RingPixels;   // 1*10+1
  const int l2  = LineBuffer<int, Net::Wp,  1, 3>::RingPixels;   // 2*5+1
  std::printf("line buffers: L1=%d  pool=%d  L2=%d elements  (total %d)\n",
              l1, pl, l2, l1 + pl + l2);
  std::printf("inter-stage FIFO storage: 0\n\n");

  std::vector<long> got, ref;
  ref_forward(ref);
  Net net;
  for (int r = 0; r < H; ++r) for (int c = 0; c < W; ++c) {
    long o;
    if (net.step(img(r, c), o)) got.push_back(o);
  }

  bool ok = got == ref;
  std::printf("output beats: %zu (expected %zu)\n", got.size(), ref.size());
  std::printf("match nested-loop reference: %s\n", ok ? "YES" : "NO");
  return ok ? 0 : 1;
}
