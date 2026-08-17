// HLS synthesis target: oneHLS::Fir<> instantiated with ac_std_float --
// real IEEE754 bit-accurate floating point, not fixed-point. Same
// composition, same coefficients as every ac_fixed/ap_fixed Fir<> in
// this library; only Sample/Accum change. See ../README.md before
// treating this as the normal way to use OneHLS -- it's a documented,
// working, but deliberately non-primary path (see main README.md's
// "Experimental: ac_std_float" section for the full resource story).
#include <oneHLS/oneHLS.h>
#include <ac_std_float.h>
#include <cstdint>

using Sample = ac_std_float<32, 8>;   // IEEE754 float32
using Accum  = ac_std_float<64, 11>;  // IEEE754 float64

// 4-tap Hamming-LPF coefficients, same as every other Fir<> target in
// this library -- plain integer values here (zero fraction bits needed:
// ac_std_float's explicit ac_std_float(int) ctor treats an integer
// coefficient as its literal value, so the default RawBitsCtor<T> just
// works, no ac_std_float-specific specialization required).
oneHLS::Fir<Sample, Accum, 10, 118, 118, 10> fir;

// Raw IEEE754 float64 bit pattern out, exactly what a real synthesized
// output port would give you -- decoding it to a human-readable value is
// the test harness's job, not this function's (see src/main.cpp).
int64_t oneHlsFloatFirTop(int16_t x) {
  Accum y = fir.filter(Sample(x));
  return y.data().to_int64();
}
