// Bambu synthesis target: OneHLS's generic oneHLS::Pid<> instantiated
// with ac_fixed, same coefficients/types as the predecessor this
// generalizes (OneData/.RnD/acTypesHLS/hls/pid_top.cpp) -- confirms the
// integral (accumulator-shaped) / derivative (FIR-tap-delay-shaped) split
// still synthesizes cleanly once genericized.
#include <oneHLS/oneHLS.h>
#include <oneHLS/ac_types_support.h>
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16, 8, true>;   // Q8.8
using Accum  = ac_fixed<32, 16, true>;  // Q16.16

// Kp=1.0(256), Ki=0.25(64), Kd=0.5(128).
oneHLS::Pid<Sample, Accum, 256, 64, 128> pid;

int32_t oneHlsPidTop(int16_t er) {
  Sample u = pid.step(Sample(er));
  return ac_int<16, true>(u.template slc<16>(0)).to_int();
}
