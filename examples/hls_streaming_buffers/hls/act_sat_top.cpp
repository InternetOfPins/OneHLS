// Round 13 synthesis: Activation + Saturate are combinational (SaturateFlag
// = 1 bit). A conv layer with requant -> saturate -> ReLU tail.
#include "../src/conv2d.h"
#include "../src/requant.h"
#include "../src/saturate.h"
#include "../src/activation.h"
#include <oneHLS/ac_types_support.h>
#include <ac_fixed.h>
#include <cstdint>
#if AC_VERSION < 4
#error "ac_types fork"
#endif
using S = ac_fixed<16, 8, true>;
using A = ac_fixed<32, 16, true>;

static oneHLS::WindowFront<S, 18, 3, 1> front;
static oneHLS::Dot<S, A, 26,-77,141,-8,210,-119,45,-163,92> kern;
static oneHLS::Requant<S, A, 3, 9>          rq;
static oneHLS::SaturateClamp<S, -128, 127>  sat;
static oneHLS::Activation<oneHLS::act::Relu<S>> relu;

int32_t oneHlsConvActTop(int16_t x, bool* fire) {
  S flat[9];
  bool f = front.step(S(x), flat);
  *fire = f;
  if (!f) return 0;
  A acc = A(kern.dot(flat)) + A(oneHLS::rawCoeff<S, 100>());
  S q = rq.apply(acc);
  return relu.apply(sat.apply(q)).to_int();
}
