/**
 * @file native_test_r2.cpp
 * @brief Round 2 native correctness for WindowExtract<> composed on LineBuffer<>.
 *   g++ -std=c++17 -I. native_test_r2.cpp -o /tmp/we && /tmp/we
 */
#include "../src/window_extract.h"
#include <cstdio>

using oneHLS::WindowExtract;

static int fails = 0;
static void check(const char* what, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

// 4-wide, 1-channel, 3x3 window.
//   r0: 1  2  3  4
//   r1: 5  6  7  8
//   r2: 9 10 11 12
static void test_w4_c1_k3() {
  std::printf("WindowExtract<int,4,1,3>: WinDepth=%d (expect 9)\n",
              WindowExtract<int,4,1,3>::WinDepth);
  check("WinDepth == 3*3*1", WindowExtract<int,4,1,3>::WinDepth == 9);

  WindowExtract<int,4,1,3> we;
  int w[3][3][1];
  bool valid = false;

  // valid needs LineBuffer warmup (pos >= 8) AND column >= K-1 (pos%4 in {2,3}).
  // first valid pos = 10.
  for (int v = 1; v <= 10; ++v) {          // positions 0..9
    int in[1] = { v };
    valid = we.step(in, w);
    check("not valid before pos 10", !valid);
  }

  { int in[1] = { 11 }; valid = we.step(in, w); }   // pos 10, r2 c2
  check("valid at pos 10", valid);
  bool topleft =
      w[0][0][0]==1 && w[0][1][0]==2  && w[0][2][0]==3  &&
      w[1][0][0]==5 && w[1][1][0]==6  && w[1][2][0]==7  &&
      w[2][0][0]==9 && w[2][1][0]==10 && w[2][2][0]==11;
  check("window == top-left 3x3 {1,2,3 / 5,6,7 / 9,10,11}", topleft);

  { int in[1] = { 12 }; valid = we.step(in, w); }   // pos 11, r2 c3
  bool slid =
      w[0][0][0]==2  && w[0][1][0]==3  && w[0][2][0]==4  &&
      w[1][0][0]==6  && w[1][1][0]==7  && w[1][2][0]==8  &&
      w[2][0][0]==10 && w[2][1][0]==11 && w[2][2][0]==12;
  check("window slid == {2,3,4 / 6,7,8 / 10,11,12}", slid);
  check("still valid at pos 11", valid);
}

// Channels=2, 3-wide, 2x2 window. Image r0 = p0,p1,p2 ; r1 = p3,p4,p5.
// pixel p carries (10p, 10p+1). First valid at p4 (LineBuffer warm at p>=3,
// column >= 1 at p%3 in {1,2}).
static void test_w3_c2_k2() {
  WindowExtract<int,3,2,2> we;
  int w[2][2][2];
  bool valid = false;
  int seq[5][2] = {{0,1},{10,11},{20,21},{30,31},{40,41}};
  for (int p = 0; p < 4; ++p) {
    valid = we.step(seq[p], w);
    check("c2: not valid before p4", !valid);
  }
  valid = we.step(seq[4], w);               // p4 = r1 c1
  check("c2: valid at p4", valid);
  bool region =
      w[0][0][0]==0  && w[0][0][1]==1  && w[0][1][0]==10 && w[0][1][1]==11 &&
      w[1][0][0]==30 && w[1][0][1]==31 && w[1][1][0]==40 && w[1][1][1]==41;
  check("c2 window == image rows0-1 cols0-1 {p0,p1 / p3,p4}", region);
}

int main() {
  std::printf("== Round 2: WindowExtract on LineBuffer ==\n");
  test_w4_c1_k3();
  test_w3_c2_k2();
  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
