#include "../src/morph.h"
#include <ac_fixed.h>
#include <cstdint>
using S = ac_fixed<16,8,true>;
static oneHLS::Erode<S,16,3>  ero;
static oneHLS::Dilate<S,16,3> dil;
int32_t oneHlsErodeTop(int16_t x, bool* f){ S o; bool ff=ero.step(S(x),o); *f=ff; return ff?o.to_int():0; }
int32_t oneHlsDilateTop(int16_t x, bool* f){ S o; bool ff=dil.step(S(x),o); *f=ff; return ff?o.to_int():0; }
