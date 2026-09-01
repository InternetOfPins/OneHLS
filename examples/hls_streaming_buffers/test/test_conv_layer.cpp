/**
 * @file native_test_r6.cpp
 * @brief Round 6 — a full streaming conv layer vs a nested-loop reference,
 * with validity-threading checked as its own pass/fail.
 *   g++ -std=c++17 -I. -I<oneHLS> -I<hapi> -I<oneData> native_test_r6.cpp -o /tmp/cv && /tmp/cv
 */
#include "../src/conv2d.h"
#include <cstdio>
#include <vector>

using oneHLS::Conv2D;
using oneHLS::WindowFront;
using oneHLS::Dot;

static int fails = 0;
static void check(const char* what, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

static constexpr int W = 7, H = 6, K = 3;
static constexpr int Wout = W - K + 1;   // 5
static constexpr int Hout = H - K + 1;   // 4

// realistic (non power-of-two) Q8.8 raw kernel + a second one for Cout=2
static const int KA[9] = { 26, -77, 141, -8, 210, -119, 45, -163, 92 };
static const int KB[9] = { -33, 64, -5, 128, -200, 17, -71, 9, 150 };
static const int BA = 100, BB = -40;

static int img(int r, int c) { return ((r * 13 + c * 7) % 29) - 14; }  // deterministic

static long ref_conv(const int kern[9], int bias, int orow, int ocol) {
  long acc = bias;
  for (int kr = 0; kr < K; ++kr)
    for (int kc = 0; kc < K; ++kc)
      acc += (long)kern[kr * K + kc] * img(orow + kr, ocol + kc);
  return acc;
}

// expected fired output-grid positions for a 2D-separable stride S
static std::vector<std::pair<int,int>> fired_positions(int S) {
  std::vector<std::pair<int,int>> v;
  for (int r = 0; r < Hout; r += S)
    for (int c = 0; c < Wout; c += S)
      v.push_back({r, c});
  return v;
}

template<int S>
static void run_layer(std::vector<long>& vals) {
  Conv2D<int, long, W, K, S, BA, 26,-77,141,-8,210,-119,45,-163,92> layer;
  for (int r = 0; r < H; ++r)
    for (int c = 0; c < W; ++c) {
      long o;
      if (layer.step(img(r, c), o)) vals.push_back(o);
    }
}

static void test_stride(int S, void(*runner)(std::vector<long>&)) {
  std::vector<long> got;
  runner(got);
  auto pos = fired_positions(S);

  char msg[80];
  std::snprintf(msg, sizeof msg, "S=%d: fire count == %zu output positions", S, pos.size());
  check(msg, got.size() == pos.size());

  bool allEq = got.size() == pos.size();
  for (size_t i = 0; i < got.size() && i < pos.size(); ++i) {
    long want = ref_conv(KA, BA, pos[i].first, pos[i].second);
    if (got[i] != want) {
      allEq = false;
      std::printf("     mismatch at fire %zu (out %d,%d): got %ld want %ld\n",
                  i, pos[i].first, pos[i].second, got[i], want);
    }
  }
  std::snprintf(msg, sizeof msg, "S=%d: every fired value == ref_conv at its grid position", S);
  check(msg, allEq);
}

static void run_s1(std::vector<long>& v) { run_layer<1>(v); }
static void run_s2(std::vector<long>& v) { run_layer<2>(v); }
static void run_s3(std::vector<long>& v) { run_layer<3>(v); }

// Cout=2: one WindowFront, two independent Dots
static void test_cout2() {
  std::printf("-- Cout=2 (shared WindowFront, 2 kernels), S=1 --\n");
  WindowFront<int, W, K, 1> front;
  Dot<int, long, 26,-77,141,-8,210,-119,45,-163,92> ka;
  Dot<int, long, -33,64,-5,128,-200,17,-71,9,150>   kb;

  std::vector<long> a, b;
  for (int r = 0; r < H; ++r)
    for (int c = 0; c < W; ++c) {
      int flat[9];
      if (front.step(img(r, c), flat)) {
        a.push_back((long)ka.dot(flat) + BA);
        b.push_back((long)kb.dot(flat) + BB);
      }
    }

  auto pos = fired_positions(1);
  bool okA = a.size() == pos.size(), okB = b.size() == pos.size();
  for (size_t i = 0; i < pos.size(); ++i) {
    if (i < a.size() && a[i] != ref_conv(KA, BA, pos[i].first, pos[i].second)) okA = false;
    if (i < b.size() && b[i] != ref_conv(KB, BB, pos[i].first, pos[i].second)) okB = false;
  }
  check("channel A matches its own reference", okA);
  check("channel B matches its own reference", okB);
  check("both channels fire on the same positions", a.size() == b.size());
}

int main() {
  std::printf("== Round 6: streaming conv layer ==\n");
  std::printf("image %dx%d, K=%d, Wout=%d Hout=%d\n", W, H, K, Wout, Hout);

  std::printf("-- numeric + validity threading, Conv2D (Cout=1) --\n");
  test_stride(1, run_s1);
  test_stride(2, run_s2);
  test_stride(3, run_s3);

  test_cout2();

  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
