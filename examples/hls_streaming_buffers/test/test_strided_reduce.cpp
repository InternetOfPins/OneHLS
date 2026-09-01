/**
 * @file native_test_r5.cpp
 * @brief Round 5 — Stride<S> gate + the WindowExtract->Stride->ReduceTree pipeline.
 *   g++ -std=c++17 -I. -I<oneData> -I<hapi> native_test_r5.cpp -o /tmp/st && /tmp/st
 */
#include "../src/stride.h"
#include "../src/strided_window_reduce.h"
#include <cstdio>
#include <vector>

using oneHLS::Stride;
using oneHLS::StridedWindowReduce;
namespace op = oneHLS::op;

static int fails = 0;
static void check(const char* what, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

static void test_gate() {
  std::printf("-- Stride<S>::fire --\n");
  { Stride<1> s; int n = 0; for (int i = 0; i < 6; ++i) n += s.fire(true); check("S=1 fires every cycle (6/6)", n == 6); }
  { Stride<2> s; std::vector<int> f; for (int i = 0; i < 6; ++i) f.push_back(s.fire(true));
    check("S=2 -> 1,0,1,0,1,0", f[0]&&!f[1]&&f[2]&&!f[3]&&f[4]&&!f[5]); }
  { Stride<3> s; std::vector<int> f; for (int i = 0; i < 7; ++i) f.push_back(s.fire(true));
    check("S=3 -> 1,0,0,1,0,0,1", f[0]&&!f[1]&&!f[2]&&f[3]&&!f[4]&&!f[5]&&f[6]); }
  { // invalid upstream cycles must not advance the phase
    Stride<2> s;
    check("S=2 first valid fires",        s.fire(true)  == true);
    check("S=2 invalid cycle: no fire",   s.fire(false) == false);
    check("S=2 invalid didn't advance",   s.fire(true)  == false);  // 2nd *valid* -> skip
    check("S=2 3rd valid fires",          s.fire(true)  == true);
  }
}

// 4-wide, 5-tall image, values 1..20. Valid 3x3 windows at stream positions
// 10,11,14,15,18,19 with sums 54,63,90,99,126,135.
static void test_pipeline() {
  std::printf("-- WindowExtract -> Stride -> ReduceTree(Add) --\n");
  const int allSums[6] = { 54, 63, 90, 99, 126, 135 };

  auto run = [](int S, std::vector<int>& got) {
    // dispatch S at compile time for the few we test
    if (S == 1) { StridedWindowReduce<int,4,3,op::Add,1> p; for (int v=1; v<=20; ++v){ int o; if (p.step(v,o)) got.push_back(o); } }
    if (S == 2) { StridedWindowReduce<int,4,3,op::Add,2> p; for (int v=1; v<=20; ++v){ int o; if (p.step(v,o)) got.push_back(o); } }
    if (S == 3) { StridedWindowReduce<int,4,3,op::Add,3> p; for (int v=1; v<=20; ++v){ int o; if (p.step(v,o)) got.push_back(o); } }
  };

  { std::vector<int> g; run(1, g);
    check("S=1: all 6 windows {54,63,90,99,126,135}",
          g.size()==6 && g[0]==54 && g[1]==63 && g[2]==90 && g[3]==99 && g[4]==126 && g[5]==135); }
  { std::vector<int> g; run(2, g);
    check("S=2: every other {54,90,126}", g.size()==3 && g[0]==54 && g[1]==90 && g[2]==126); }
  { std::vector<int> g; run(3, g);
    check("S=3: every third {54,99}", g.size()==2 && g[0]==54 && g[1]==99); }
  (void)allSums;
}

static void test_pipeline_max() {
  std::printf("-- same pipeline, Op=Max (max-pool) --\n");
  // S=3, K=3 over the 4x5 image: fired windows at pos 10 and 15 -> max 11, max 16
  StridedWindowReduce<int,4,3,op::Max,3> p;
  std::vector<int> g;
  for (int v = 1; v <= 20; ++v) { int o; if (p.step(v, o)) g.push_back(o); }
  check("Max pool S=3 -> {11,16}", g.size()==2 && g[0]==11 && g[1]==16);
}

int main() {
  std::printf("== Round 5: Stride + strided pipeline ==\n");
  test_gate();
  test_pipeline();
  test_pipeline_max();
  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
