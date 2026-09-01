/**
 * @file native_test_r13.cpp
 * @brief Round 13 — Activation (ReLU family) + Saturate, vs references.
 *   g++ -std=c++17 -I. native_test_r13.cpp -o /tmp/r13 && /tmp/r13
 */
#include "../src/activation.h"
#include "../src/saturate.h"
#include <cstdio>

using namespace oneHLS;

static int fails = 0;
static void check(const char* what, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

int main() {
  std::printf("== Round 13: Activation + Saturate ==\n");

  // ReLU
  {
    Activation<act::Relu<int>> relu;
    bool ok = true;
    for (int x = -8; x <= 8; ++x) if (relu.apply(x) != (x < 0 ? 0 : x)) ok = false;
    check("Relu == max(0,x)", ok);
  }
  // clamped ReLU (cap 6)
  {
    Activation<act::Relu6<int, 6>> r6;
    bool ok = true;
    for (int x = -4; x <= 12; ++x) {
      int want = x < 0 ? 0 : (x > 6 ? 6 : x);
      if (r6.apply(x) != want) ok = false;
    }
    check("Relu6 == clamp(x, 0, 6)", ok);
  }
  // leaky ReLU, shift 2 (negative slope 1/4)
  {
    Activation<act::LeakyRelu<int, 2>> lr;
    check("LeakyRelu(8) == 8",  lr.apply(8) == 8);
    check("LeakyRelu(-8) == -2", lr.apply(-8) == (-8 >> 2));
    check("LeakyRelu(0) == 0",  lr.apply(0) == 0);
  }

  // SaturateClamp
  {
    SaturateClamp<int, -100, 100> s;
    check("clamp(150) == 100",  s.apply(150) == 100);
    check("clamp(-150) == -100", s.apply(-150) == -100);
    check("clamp(42) == 42",    s.apply(42) == 42);
  }
  // SaturateFlag: sticky overflow bit
  {
    SaturateFlag<int, 0, 255> s;
    check("no flag initially", !s.flagged());
    check("in-range passes, no flag", s.apply(128) == 128 && !s.flagged());
    check("over -> clamps + flags", s.apply(300) == 255 && s.flagged());
    check("flag sticky across in-range", (s.apply(10), s.flagged()));
    s.clear();
    check("cleared", !s.flagged());
    check("under -> clamps + flags", s.apply(-5) == 0 && s.flagged());
  }

  // composition: Requant-style narrow then Saturate then Activation
  {
    // (acc>>4) clamped to int8 range, then ReLU
    SaturateClamp<int, -128, 127> sat;
    Activation<act::Relu<int>>    relu;
    auto pipe = [&](long acc) { return relu.apply(sat.apply((int)(acc >> 4))); };
    check("pipe(4096) = relu(clamp(256)) = relu(127) = 127", pipe(4096) == 127);
    check("pipe(-4096) = relu(clamp(-256)) = relu(-128) = 0", pipe(-4096) == 0);
    check("pipe(160) = relu(clamp(10)) = 10", pipe(160) == 10);
  }

  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
