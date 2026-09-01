// Round 4 synthesis target: oneHLS::ReduceTree<> at N=9 (a 3x3 window),
// both Op=op::Add (conv accumulate / avg-pool) and Op=op::Max (max-pool)
// from one primitive. Check: both synthesize clean, stateless (0
// registers), ceil(log2 9)=4 combine levels.
#include "../src/reduce_tree.h"
#include <ac_fixed.h>
#include <cstdint>

#if AC_VERSION < 4
#error "resolved to bambu's bundled ac_types fork, not real upstream github.com/hlslibs/ac_types"
#endif

using Sample = ac_fixed<16, 8, true>;    // Q8.8
using Accum  = ac_fixed<20, 12, true>;   // headroom for 9-way add

static oneHLS::ReduceTree<oneHLS::op::Add, Accum,  9> radd;
static oneHLS::ReduceTree<oneHLS::op::Max, Sample, 9> rmax;

int32_t oneHlsReduceAddTop(int16_t a0,int16_t a1,int16_t a2,int16_t a3,int16_t a4,
                           int16_t a5,int16_t a6,int16_t a7,int16_t a8) {
  Accum w[9] = { Accum(a0),Accum(a1),Accum(a2),Accum(a3),Accum(a4),
                 Accum(a5),Accum(a6),Accum(a7),Accum(a8) };
  return radd.reduce(w).to_int();
}

int32_t oneHlsReduceMaxTop(int16_t a0,int16_t a1,int16_t a2,int16_t a3,int16_t a4,
                           int16_t a5,int16_t a6,int16_t a7,int16_t a8) {
  Sample w[9] = { Sample(a0),Sample(a1),Sample(a2),Sample(a3),Sample(a4),
                  Sample(a5),Sample(a6),Sample(a7),Sample(a8) };
  return rmax.reduce(w).to_int();
}
