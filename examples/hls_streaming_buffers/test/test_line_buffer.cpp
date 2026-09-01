/**
 * @file native_test.cpp
 * @brief Round 1 native correctness for LineBuffer<>. Plain int, deterministic.
 *   g++ -std=c++17 -I. native_test.cpp -o /tmp/lb && /tmp/lb
 */
#include "../src/line_buffer.h"
#include <cstdio>
#include <cassert>

using oneHLS::LineBuffer;

static int fails = 0;
static void check(const char* what, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

// 4-wide, 1-channel, 3-tall window.
//   row0: 1  2  3  4
//   row1: 5  6  7  8
//   row2: 9 10 11 12
static void test_w4_c1_t3() {
  std::printf("LineBuffer<int,4,1,3>: Depth=%d (expect 9)\n",
              LineBuffer<int,4,1,3>::Depth);
  check("Depth == ((3-1)*4+1)*1 == 9", LineBuffer<int,4,1,3>::Depth == 9);

  LineBuffer<int,4,1,3> lb;
  int col[3][1];
  bool valid = false;

  // stream positions 0..7 (rows 0 and 1, then row2 col0): not valid until pos 8
  for (int v = 1; v <= 8; ++v) {
    int in[1] = { v };
    valid = lb.step(in, col);
    check("not valid before (Taps-1) full rows", !valid);
  }

  // pos 8, value 9 (row2 col0) -> first valid strip = left edge column [1,5,9]
  { int in[1] = { 9 }; valid = lb.step(in, col); }
  check("valid at pos 8", valid);
  check("col == [1,5,9]", col[0][0]==1 && col[1][0]==5 && col[2][0]==9);

  { int in[1] = { 10 }; valid = lb.step(in, col); }
  check("col == [2,6,10]", col[0][0]==2 && col[1][0]==6 && col[2][0]==10);

  { int in[1] = { 11 }; valid = lb.step(in, col); }
  check("col == [3,7,11]", col[0][0]==3 && col[1][0]==7 && col[2][0]==11);

  { int in[1] = { 12 }; valid = lb.step(in, col); }
  check("col == [4,8,12]", col[0][0]==4 && col[1][0]==8 && col[2][0]==12);
}

// 3-wide, 2-channel, 2-tall: interleaved channels, Depth = ((2-1)*3+1)*2 = 8
static void test_w3_c2_t2() {
  std::printf("LineBuffer<int,3,2,2>: Depth=%d (expect 8)\n",
              LineBuffer<int,3,2,2>::Depth);
  check("Depth == 8", LineBuffer<int,3,2,2>::Depth == 8);

  LineBuffer<int,3,2,2> lb;
  int col[2][2];
  bool valid = false;

  // pixels p0.., channel values (10*p, 10*p+1). RingPixels = 1*3+1 = 4, so
  // filled hits 4 (valid) on the 4th pixel, p==3 (row1 col0).
  for (int p = 0; p < 3; ++p) {
    int in[2] = { 10*p, 10*p + 1 };
    valid = lb.step(in, col);
    check("not valid before 4th pixel", !valid);
  }
  { int in[2] = { 30, 31 }; valid = lb.step(in, col); }
  check("valid at 4th pixel (row1 col0)", valid);
  // current row pixel = p3 = (30,31); 1 row back at col0 = p0 = (0,1)
  check("col == [[0,1],[30,31]]",
        col[0][0]==0 && col[0][1]==1 && col[1][0]==30 && col[1][1]==31);
}

int main() {
  std::printf("== Round 1: LineBuffer native ==\n");
  test_w4_c1_t3();
  test_w3_c2_t2();
  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
