// Round 15 synthesis: AXI4-Stream boundary adapter. The pipeline outputs
// 900 beats (30x30); the skid FIFO must be PIPELINE-LATENCY sized (~24),
// NOT output-sized -- that's the whole point of "thin adapter at the ends".
#include "../src/axis.h"
#include "../src/conv2d.h"
#include "../src/dot.h"
#include <ac_fixed.h>
#include <cstdint>
using S = ac_fixed<16, 8, true>;
using A = ac_fixed<32, 16, true>;

static constexpr int W = 32, K = 3;
static constexpr int NOUT = (W - K + 1) * (W - K + 1);   // 900

struct Pipe {
  oneHLS::WindowFront<S, W, K, 1> front;
  oneHLS::Dot<S, A, 26,-77,141,-8,210,-119,45,-163,92> kern;
  static constexpr int LatencyBound =
      decltype(front)::LatencyBound + decltype(kern)::LatencyBound + 1;
  bool step(S x, A& out) {
    S flat[9];
    if (!front.step(x, flat)) return false;
    out = kern.dot(flat);
    return true;
  }
};

static oneHLS::AxisWrap<S, A, Pipe, NOUT> axis;

// one AXIS-master beat per call: feed input if accepted, pull output if
// consumer ready. tready in; tvalid/tlast/tdata out.
void oneHlsAxisTop(int16_t x, bool tready,
                   int32_t* tdata, bool* tvalid, bool* tlast, bool* in_tready) {
  *in_tready = axis.feed(S(x));
  A td; bool tv, tl;
  axis.pull(tready, td, tv, tl);
  *tdata  = td.to_int();
  *tvalid = tv;
  *tlast  = tl;
}
