/**
 * @file native_test_r15.cpp
 * @brief Round 15 — AXI4-Stream boundary adapter: no data loss / order kept /
 * tlast correct under a consumer that randomly deasserts tready.
 *   g++ -std=c++17 -I. native_test_r15.cpp -o /tmp/r15 && /tmp/r15
 */
#include "../src/axis.h"
#include "../src/conv2d.h"      // WindowFront
#include "../src/dot.h"
#include <cstdio>
#include <vector>

using namespace oneHLS;

static int fails = 0;
static void check(const char* w, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", w);
  if (!ok) ++fails;
}

// pipeline under test: SAME-less 3x3 conv over a 6x6 image -> 4x4 = 16 beats
static constexpr int W = 6, H = 6, K = 3;
static constexpr int NOUT = (W - K + 1) * (H - K + 1);   // 16
static int img(int r, int c) { return ((r * 7 + c * 5) % 13) - 6; }

struct Pipe {
  WindowFront<int, W, K, 1> front;
  Dot<int, long, 1,2,3, 4,5,6, 7,8,9> kern;
  static constexpr int LatencyBound =
      decltype(front)::LatencyBound + decltype(kern)::LatencyBound + 1;
  bool step(int x, long& out) {
    int flat[9];
    if (!front.step(x, flat)) return false;
    out = kern.dot(flat);
    return true;
  }
};

static long ref(int orow, int ocol) {
  long a = 0;
  for (int kr = 0; kr < K; ++kr)
    for (int kc = 0; kc < K; ++kc)
      a += (kr*K + kc + 1) * img(orow + kr, ocol + kc);
  return a;
}

int main() {
  std::printf("== Round 15: AXI4-Stream adapter ==\n");
  std::printf("pipeline out beats NOUT = %d\n", NOUT);

  AxisWrap<int, long, Pipe, NOUT> axis;

  std::vector<long> golden;
  for (int r = 0; r < H - K + 1; ++r)
    for (int c = 0; c < W - K + 1; ++c) golden.push_back(ref(r, c));

  // Drive: input raster, consumer deasserts tready pseudo-randomly.
  std::vector<long> received;
  int lastCount = 0;
  int seed = 999, ir = 0, ic = 0, guard = 0;
  bool sawTlast = false;
  while ((int)received.size() < NOUT && guard++ < 100000) {
    // input side
    if (ir < H) {
      if (axis.feed(img(ir, ic))) { if (++ic == W) { ic = 0; ++ir; } }
    } else {
      axis.feed(0);   // keep clocking the pipeline to drain in-flight
    }
    // output side: tready random ~60%
    seed = seed * 1103515245 + 12345;
    bool tready = ((seed >> 16) & 7) < 5;
    long td; bool tv, tl;
    if (axis.pull(tready, td, tv, tl)) {
      received.push_back(td);
      if (tl) { sawTlast = true; ++lastCount; }
    }
  }

  check("received exactly NOUT beats", (int)received.size() == NOUT);
  check("no beat lost / order preserved (== golden)", received == golden);
  check("tlast asserted exactly once, on the last beat", sawTlast && lastCount == 1);

  // sanity: with tready always high, same result
  {
    AxisWrap<int, long, Pipe, NOUT> a2;
    std::vector<long> rx;
    int r2 = 0, c2 = 0, g = 0;
    while ((int)rx.size() < NOUT && g++ < 100000) {
      if (r2 < H) { if (a2.feed(img(r2, c2))) { if (++c2 == W) { c2 = 0; ++r2; } } }
      else a2.feed(0);
      long td; bool tv, tl;
      if (a2.pull(true, td, tv, tl)) rx.push_back(td);
    }
    check("tready-always-high path == golden", rx == golden);
  }

  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
