/**
 * @file native_test_r4.cpp
 * @brief Round 4 — ReduceTree<op::Add|op::Max, T, N> vs plain-loop references.
 *   g++ -std=c++17 -I. native_test_r4.cpp -o /tmp/rt && /tmp/rt
 */
#include "../src/reduce_tree.h"
#include <cstdio>

using oneHLS::ReduceTree;
namespace op = oneHLS::op;

static int fails = 0;
static void check(const char* what, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

template<typename T, int N>
static T ref_sum(const T* a) { T s = a[0]; for (int i = 1; i < N; ++i) s = T(s + a[i]); return s; }
template<typename T, int N>
static T ref_max(const T* a) { T m = a[0]; for (int i = 1; i < N; ++i) if (m < a[i]) m = a[i]; return m; }

int main() {
  std::printf("== Round 4: ReduceTree ==\n");

  // N=9 (a 3x3 conv/pool window), non-power-of-two -> exercises the
  // odd-split path (5 + 4).
  {
    const int in[9] = { 3, 1, 4, 1, 5, 9, 2, 6, 5 };
    ReduceTree<op::Add, int, 9> radd;
    ReduceTree<op::Max, int, 9> rmax;
    std::printf("N=9  Add.Depth=%d  Max.Depth=%d (expect 4)\n",
                (ReduceTree<op::Add,int,9>::Depth), (ReduceTree<op::Max,int,9>::Depth));
    check("Depth == 4", (ReduceTree<op::Add,int,9>::Depth) == 4);
    check("Add == 36 (== ref_sum)",  radd.reduce(in) == 36 && radd.reduce(in) == (ref_sum<int,9>(in)));
    check("Max == 9  (== ref_max)",  rmax.reduce(in) == 9  && rmax.reduce(in) == (ref_max<int,9>(in)));
  }

  // N=8, power-of-two -> perfectly balanced tree, Depth 3
  {
    const int in[8] = { -2, 7, 7, 0, 100, -1, 3, 8 };
    ReduceTree<op::Add, int, 8> radd;
    ReduceTree<op::Max, int, 8> rmax;
    ReduceTree<op::Min, int, 8> rmin;
    check("N=8 Depth == 3", (ReduceTree<op::Add,int,8>::Depth) == 3);
    check("Add == ref_sum", radd.reduce(in) == (ref_sum<int,8>(in)));
    check("Max == 100",     rmax.reduce(in) == 100);
    check("Min == -2",      rmin.reduce(in) == -2);
  }

  // N=1 degenerate
  {
    const int in[1] = { 42 };
    ReduceTree<op::Add, int, 1> r;
    check("N=1 Depth == 0", (ReduceTree<op::Add,int,1>::Depth) == 0);
    check("N=1 passes value through", r.reduce(in) == 42);
  }

  // tree vs linear must agree for exact integer add (associative)
  {
    int in[16];
    for (int i = 0; i < 16; ++i) in[i] = (i * 37 % 91) - 45;
    ReduceTree<op::Add, int, 16> r;
    check("N=16 tree == linear sum", r.reduce(in) == (ref_sum<int,16>(in)));
  }

  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
