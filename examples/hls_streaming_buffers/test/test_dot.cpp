/**
 * @file native_test_r3.cpp
 * @brief Round 3 — Dot<> verified against oneHLS::Fir<> (the known-good
 * reference: a Dot over a shifting window is a FIR filter).
 *   g++ -std=c++17 -I. -I<oneHLS> -I<hapi> -I<onedata> native_test_r3.cpp -o /tmp/dot && /tmp/dot
 */
#include "../src/dot.h"
#include <cstdio>

using oneHLS::Dot;
using oneHLS::Fir;

static int fails = 0;
static void check(const char* what, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

int main() {
  std::printf("== Round 3: Dot vs Fir ==\n");

  using S = int;
  using A = long;
  constexpr int K = 4;

  Fir<S, A, 10, 118, 118, 10>  fir;
  Dot<S, A, 10, 118, 118, 10>  d;
  check("Dot::K == 4", (Dot<S,A,10,118,118,10>::K) == 4);

  // manual most-recent-first shift register feeding Dot the same window
  // Fir builds internally.
  S sr[K] = { 0, 0, 0, 0 };
  const S x[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };   // impulse
  const A expect[8] = { 0, 10, 118, 118, 10, 0, 0, 0 };  // README's Fir impulse response

  for (int n = 0; n < 8; ++n) {
    A yf = fir.filter(x[n]);

    A yd = d.dot(sr);                 // Dot sees the pre-update window
    // then shift x[n] in, most-recent first
    for (int i = K - 1; i > 0; --i) sr[i] = sr[i - 1];
    sr[0] = x[n];

    char msg[64];
    std::snprintf(msg, sizeof msg, "n=%d  Fir=%ld  Dot=%ld  expect=%ld", n, yf, yd, expect[n]);
    check(msg, yf == expect[n] && yd == expect[n]);
  }

  // second vector: step input, still must track Fir exactly
  std::printf("  -- step input --\n");
  Fir<S, A, 10, 118, 118, 10>  fir2;
  Dot<S, A, 10, 118, 118, 10>  d2;
  S sr2[K] = { 0, 0, 0, 0 };
  for (int n = 0; n < 8; ++n) {
    S in = 1;
    A yf = fir2.filter(in);
    A yd = d2.dot(sr2);
    for (int i = K - 1; i > 0; --i) sr2[i] = sr2[i - 1];
    sr2[0] = in;
    char msg[48];
    std::snprintf(msg, sizeof msg, "n=%d  Fir=%ld  Dot=%ld", n, yf, yd);
    check(msg, yf == yd);
  }

  std::printf(fails ? "\nFAILED (%d)\n" : "\nOK\n", fails);
  return fails ? 1 : 0;
}
