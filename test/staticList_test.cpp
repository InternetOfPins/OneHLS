/**
 * @file staticList_test.cpp
 * @brief Native regression test for oneHLS::StaticList<>/staticList()
 * (include/oneHLS/staticList.h). Not yet wired into test.cpp's doTests()
 * umbrella or README's Features table -- newly promoted from
 * .RnD/polyphase_experiment/, verified here and under Bambu (see
 * docs/PHASE9_GENERIC_POLYPHASE.md), but the "ship it as a documented
 * Feature" decision is separate and not made yet.
 *
 * Three things checked: (1) plain POD-type correctness (visit/getAt,
 * single- and multi-element), (2) the intended real use case -- composing
 * actual OneHLS components (distinct Fir<> instantiations) via the
 * factory function, matching how the polyphase decimator experiment used
 * this shape, but now via the public, promoted header instead of a local
 * per-experiment copy, (3) staticList()'s decay_t fix: calling it with a
 * local (non-static) lvalue must still produce a StaticList holding an
 * OWNED COPY, not a reference to that lvalue -- verified by mutating the
 * original local after the list is built and confirming the list's own
 * copy is unaffected, not just by "it compiles."
 */
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <oneHLS/oneHLS.h>
#include <oneHLS/staticList.h>
#include <oneHLS/ac_types_support.h>
#include <ac_fixed.h>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

struct A { int v; };
struct B { int v; };
struct C { int v; };

static void test_pod_multi() {
  auto l = oneHLS::staticList(A{1}, B{2}, C{3});
  int got0 = -1, got1 = -1, got2 = -1;
  l.visit(0, [&](auto& x){ got0 = x.v; });
  l.visit(1, [&](auto& x){ got1 = x.v; });
  l.visit(2, [&](auto& x){ got2 = x.v; });
  assert(got0 == 1 && got1 == 2 && got2 == 3);
  assert(l.getAt<0>().v == 1);
  assert(l.getAt<1>().v == 2);
  assert(l.getAt<2>().v == 3);
  printf("StaticList POD multi-element: ok (visit + getAt agree)\n");
}

static void test_pod_single() {
  auto l = oneHLS::staticList(A{42});
  int got = -1;
  l.visit(0, [&](auto& x){ got = x.v; });
  assert(got == 42);
  assert(l.getAt<0>().v == 42);
  printf("StaticList POD single-element (non-empty base case): ok\n");
}

// std::decay_t regression: staticList() called with a LOCAL (non-static)
// lvalue must own an independent copy, not silently deduce a reference
// type via forwarding-reference rules -- mutating the original local
// after the list is built and confirming the list's own value is
// unaffected is the real check, not just "this compiles."
static void test_lvalue_decay() {
  A localA{100};
  auto l = oneHLS::staticList(localA);
  localA.v = 999;  // mutate the ORIGINAL local after the list was built
  assert(l.getAt<0>().v == 100);  // list's own copy must be unaffected
  printf("StaticList lvalue decay_t: ok (owns a copy, not a reference to a local)\n");
}

// Real use case: 2 distinct Fir<> instantiations (different coefficients
// -> different C++ types, same reason the polyphase decimator needed
// this shape rather than a plain array), composed via the factory.
// File-scope, not function-local: ac_fixed leaves default construction's
// bits indeterminate, and only static storage duration's language-
// mandated zero-init makes the first read well-defined -- this exact
// pitfall was already documented (and hit) twice earlier in this study
// (PHASE4_POLYPHASE_EXPERIMENT.md §2) and is worth naming again here
// rather than silently avoiding it, since it's the kind of mistake that
// recurs specifically when a test is written quickly.
namespace real_components_state {
  using Sample = ac_fixed<16,16,true>;
  using Accum  = ac_fixed<32,32,true>;
  oneHLS::Fir<Sample,Accum,10,118> branch0;
  oneHLS::Fir<Sample,Accum,118,10> branch1;
}

static void test_real_components() {
  using namespace real_components_state;
  auto branches = oneHLS::staticList(branch0, branch1);

  // Drive an impulse through branch 0 via visit(), branch 1 via getAt<1>() --
  // both access paths exist on the same list, exercised together here, and
  // both checked against real oneHLS::Fir<> semantics (the actual, verified
  // one-cycle-delayed Tap behavior -- see PHASE4_POLYPHASE_EXPERIMENT.md §2
  // for why an idealized/combinatorial expectation would be wrong here).
  // branch0 = Fir<10,118>: impulse response 0,10,118,0
  // branch1 = Fir<118,10>: impulse response 0,118,10,0
  int32_t impulse[] = {1,0,0,0};
  int32_t expect0[] = {0,10,118,0};
  int32_t expect1[] = {0,118,10,0};
  for (int32_t n = 0; n < 4; ++n) {
    Sample x = Sample(impulse[n]);
    Accum y0v{};
    branches.visit(0, [&](auto& br){ y0v = br.filter(x); });
    Accum y1g = branches.getAt<1>().filter(x);
    assert(y0v.to_int() == expect0[n]);
    assert(y1g.to_int() == expect1[n]);
  }
  printf("StaticList of real oneHLS::Fir<> branches: ok (visit()+getAt<> both match Fir<> semantics)\n");
}

int main() {
  test_pod_multi();
  test_pod_single();
  test_lvalue_decay();
  test_real_components();
  printf("All StaticList tests passed.\n");
  return 0;
}
