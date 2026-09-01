// Round 6 synthesis target: a full streaming conv layer from the Round 1-5
// primitives. Realistic (non power-of-two) Q8.8 kernel, so the Bambu-vs-
// Vitis DSP-vs-strength-reduction result is representative.
//   oneHlsConvLayerTop    -- Cout=1, stride 1
//   oneHlsConvLayerS2Top  -- Cout=1, stride 2 (2D-separable via Stride2D)
//   oneHlsConvLayer2Top   -- Cout=2, stride 1, one shared WindowFront
#include "../src/conv2d.h"
#include <oneHLS/ac_types_support.h>
#include <ac_fixed.h>
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16, 8,  true>;   // Q8.8
using Accum  = ac_fixed<32, 16, true>;

// realistic kernel: {26,-77,141,-8,210,-119,45,-163,92}, bias 100 (Q8.8 raw)
#define KA 100, 26,-77,141,-8,210,-119,45,-163,92
#define KB -40, -33,64,-5,128,-200,17,-71,9,150

static oneHLS::Conv2D<Sample, Accum, 8, 3, 1, KA> convS1;
static oneHLS::Conv2D<Sample, Accum, 8, 3, 2, KA> convS2;

static oneHLS::WindowFront<Sample, 8, 3, 1>                          front2;
static oneHLS::Dot<Sample, Accum, 26,-77,141,-8,210,-119,45,-163,92> kA2;
static oneHLS::Dot<Sample, Accum, -33,64,-5,128,-200,17,-71,9,150>   kB2;

void oneHlsConvLayerTop(int16_t x, int32_t* out, bool* fire) {
  Accum o; bool f = convS1.step(Sample(x), o);
  *fire = f; *out = f ? o.to_int() : 0;
}

void oneHlsConvLayerS2Top(int16_t x, int32_t* out, bool* fire) {
  Accum o; bool f = convS2.step(Sample(x), o);
  *fire = f; *out = f ? o.to_int() : 0;
}

void oneHlsConvLayer2Top(int16_t x, int32_t* outA, int32_t* outB, bool* fire) {
  Sample flat[9];
  bool f = front2.step(Sample(x), flat);
  *fire = f;
  *outA = f ? (Accum(kA2.dot(flat)) + Accum(oneHLS::rawCoeff<Sample, 100>())).to_int() : 0;
  *outB = f ? (Accum(kB2.dot(flat)) + Accum(oneHLS::rawCoeff<Sample, -40>())).to_int() : 0;
}
