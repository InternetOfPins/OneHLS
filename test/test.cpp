/**
 * @file test.cpp
 * @brief OneHLS native regression tests.
 *
 * Native (plain g++, no Bambu) correctness for every component, run
 * against TWO independent vendor fixed-point libraries with the exact
 * same template instantiations (only Sample/Accum change) -- proof the
 * library is genuinely type-agnostic, not just ac_types-shaped:
 *   Fir<>     -- 4-tap Hamming-LPF, hand-derived impulse response
 *   Biquad<>  -- direct-form-I 2nd-order IIR section, hand-derived
 *               impulse response (feedforward + feedback delay lines)
 *   Pid<>     -- constant-error and impulse-disturbance step responses
 *
 * Every expected sequence here reproduces this library's origin
 * investigation exactly (see OneData/.RnD/acTypesHLS/HANDOFF.md) and
 * every Bambu resource count recorded there for the hand-written,
 * non-generic predecessors of these components (FIR: FF=62/area=7679;
 * Biquad: FF=101/area=6855; PID: FF=32/area=3841; all 0 DSPs) is
 * reproduced bit-for-bit and gate-for-gate by the generic versions here
 * (see .RnD/hls/ -- fir_top.cpp, biquad_top.cpp, pid_top.cpp).
 *
 * Component instances are file-scope (static storage duration) rather
 * than locals inside each test function, unlike OneData's own test.cpp
 * convention: ac_fixed/ap_fixed deliberately leave a default-constructed
 * value's bits indeterminate (mirroring an unreset hardware register,
 * not a bug) -- only static storage duration's language-mandated
 * zero-init makes the first read well-defined. Real Bambu-synthesized
 * hardware gets its own reset network regardless, so this is a native-
 * test-only concern, not a synthesis concern.
 *
 * Requires on the include path (not vendored, see each _support.h):
 *   AC_TYPES_INCLUDE -- git clone --depth 1 https://github.com/hlslibs/ac_types
 *   AP_TYPES_INCLUDE -- git clone --depth 1 https://github.com/Xilinx/HLS_arbitrary_Precision_Types
 */

#include <cstdint>
#include <cassert>
#include <cstdio>
#include <oneHLS/oneHLS.h>
#include <oneHLS/ac_types_support.h>
#include <oneHLS/ap_types_support.h>

#ifdef ARDUINO
  #define TLOG(x) Serial.println(x)
#else
  #define TLOG(x) printf("%s\n", x)
#endif

// ---------------------------------------------------------------------
// ac_fixed instantiations (the library's origin vendor)
// ---------------------------------------------------------------------
namespace ac_round {
  using Sample = ac_fixed<16, 16, true>;  // FIR: no fractional bits needed,
  using Accum  = ac_fixed<32, 32, true>;  // coefficients are plain integers

  using SampleQ = ac_fixed<16, 8, true>;  // Biquad/PID: Q8.8
  using AccumQ  = ac_fixed<32, 16, true>; // Q16.16

  oneHLS::Fir<Sample, Accum, 10, 118, 118, 10> fir;
  // b1=0.5(128), b2=0.25(64); -a1=0.5(128), -a2=-0.25(-64)
  oneHLS::Biquad<SampleQ, AccumQ, 128, 64, 128, -64> biquad;
  // Kp=1.0(256), Ki=0.25(64), Kd=0.5(128)
  oneHLS::Pid<SampleQ, AccumQ, 256, 64, 128> pidConst, pidImpulse;
}

void test_ac_fixed_fir() {
  using namespace ac_round;
  int16_t impulse[] = {1, 0, 0, 0, 0, 0, 0, 0};
  int32_t expected[] = {0, 10, 118, 118, 10, 0, 0, 0};
  for (int i = 0; i < 8; ++i)
    assert(fir.filter(Sample(impulse[i])).to_int() == expected[i]);
  TLOG("ac_fixed Fir<>: ok (0 10 118 118 10 0 0 0)");
}

void test_ac_fixed_biquad() {
  using namespace ac_round;
  int16_t impulse[] = {1, 0, 0, 0, 0, 0, 0, 0};
  int32_t expected[] = {0, 128, 128, 32, -16, -16, -4, 2};
  for (int i = 0; i < 8; ++i)
    assert(biquad.step(SampleQ(impulse[i])).template slc<16>(0).to_int() == expected[i]);
  TLOG("ac_fixed Biquad<>: ok (0 128 128 32 -16 -16 -4 2)");
}

static int32_t acPidStep(oneHLS::Pid<ac_round::SampleQ, ac_round::AccumQ, 256, 64, 128>& pid, int16_t er) {
  ac_round::SampleQ u = pid.step(ac_round::SampleQ(er));
  return ac_int<16, true>(u.template slc<16>(0)).to_int();
}

void test_ac_fixed_pid() {
  using namespace ac_round;
  int32_t expectedConst[] = {448, 384, 448, 512, 576};
  for (int i = 0; i < 5; ++i) assert(acPidStep(pidConst, 1) == expectedConst[i]);
  TLOG("ac_fixed Pid<>: ok, constant error (448 384 448 512 576)");

  int16_t impulse[] = {1, 0, 0, 0, 0};
  int32_t expectedImpulse[] = {448, -64, 64, 64, 64};
  for (int i = 0; i < 5; ++i) assert(acPidStep(pidImpulse, impulse[i]) == expectedImpulse[i]);
  TLOG("ac_fixed Pid<>: ok, impulse disturbance (448 -64 64 64 64)");
}

// ---------------------------------------------------------------------
// ap_fixed instantiations -- SAME oneHLS:: templates, SAME coefficients,
// zero modification: only Sample/Accum and the _support.h header differ.
// ---------------------------------------------------------------------
namespace ap_round {
  using Sample = ap_fixed<16, 8>;
  using Accum  = ap_fixed<32, 16>;

  oneHLS::Fir<Sample, Accum, 10, 118, 118, 10> fir;
  oneHLS::Biquad<Sample, Accum, 128, 64, 128, -64> biquad;
  oneHLS::Pid<Sample, Accum, 256, 64, 128> pidConst, pidImpulse;

  static int32_t rd(Sample s) { return ap_int<16>(s.range(15, 0)).to_int(); }
}

void test_ap_fixed_fir() {
  using namespace ap_round;
  // Sample is Q8.8 here (unlike the ac_fixed round's Q16.0 whole-number
  // coefficients): a Q8.8 coefficient multiplied by a Q8.8 unity input
  // (x=1.0) and re-expressed back in Q8.8 preserves the raw bit pattern
  // exactly, so reading raw bits back out (rd()) reproduces the same
  // 10/118/118/10 sequence, bit-for-bit, as the ac_fixed round.
  int16_t impulse[] = {1, 0, 0, 0, 0, 0, 0, 0};
  int32_t expected[] = {0, 10, 118, 118, 10, 0, 0, 0};
  for (int i = 0; i < 8; ++i)
    assert(rd(fir.filter(Sample(impulse[i]))) == expected[i]);
  TLOG("ap_fixed Fir<>: ok (0 10 118 118 10 0 0 0) -- bit-identical to ac_fixed");
}

void test_ap_fixed_biquad() {
  using namespace ap_round;
  int16_t impulse[] = {1, 0, 0, 0, 0, 0, 0, 0};
  int32_t expected[] = {0, 128, 128, 32, -16, -16, -4, 2};
  for (int i = 0; i < 8; ++i)
    assert(rd(biquad.step(Sample(impulse[i]))) == expected[i]);
  TLOG("ap_fixed Biquad<>: ok (0 128 128 32 -16 -16 -4 2) -- bit-identical to ac_fixed");
}

void test_ap_fixed_pid() {
  using namespace ap_round;
  int32_t expectedConst[] = {448, 384, 448, 512, 576};
  for (int i = 0; i < 5; ++i) assert(rd(pidConst.step(Sample(1))) == expectedConst[i]);

  int16_t impulse[] = {1, 0, 0, 0, 0};
  int32_t expectedImpulse[] = {448, -64, 64, 64, 64};
  for (int i = 0; i < 5; ++i) assert(rd(pidImpulse.step(Sample(impulse[i]))) == expectedImpulse[i]);
  TLOG("ap_fixed Pid<>: ok, both sequences bit-identical to ac_fixed");
}

void doTests() {
  test_ac_fixed_fir();
  test_ac_fixed_biquad();
  test_ac_fixed_pid();
  test_ap_fixed_fir();
  test_ap_fixed_biquad();
  test_ap_fixed_pid();
  TLOG("all OneHLS tests passed");
}

#ifdef ARDUINO
  void setup() { Serial.begin(115200); while(!Serial); doTests(); }
  void loop() {}
#else
  int main() { doTests(); return 0; }
#endif
