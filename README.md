# OneHLS

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Type-agnostic, HLS-synthesizable DSP and control components for **HAPI** + **OneData**.

`Fir<>`, `Biquad<>`, and `Pid<>` are written once against a caller-chosen `Sample`/`Accum` type — the composition never names a vendor fixed-point type directly. The same unmodified templates have been verified against two independent bit-accurate fixed-point libraries (Siemens HLSLibs `ac_types` and Xilinx/AMD `ap_types`), both natively and synthesized to real RTL via [Bambu HLS](https://release.bambuhls.eu/), with bit-identical numeric results and gate-identical resource counts across both vendors.

---

## Features

| Component | Description | State |
|---|---|---|
| `Fir<Sample,Accum,Coeffs...>` | N-tap FIR filter, direct form | one `oneData::Data<Sample>` delay per tap |
| `Biquad<Sample,Accum,B1,B2,FBA1,FBA2>` | 2nd-order IIR section, direct form I | 2 feedforward + 2 feedback delays |
| `Pid<Sample,Accum,Kp,Ki,Kd>` | PID controller (`u = Kp·e + Ki·Σe + Kd·Δe`) | 1 accumulator + 1 delay |
| `RawBitsCtor<T>` | Customization point: build a coefficient from a raw, pre-scaled bit pattern | — |

All three are ordinary HAPI `Chain<>` compositions under the hood (see [include/oneHLS/oneHLS.h](include/oneHLS/oneHLS.h)) — `Biquad`'s feedforward taps *are* `Fir`'s `Tap` alias, reused verbatim.

---

## Quick Usage

```cpp
#include <oneHLS/oneHLS.h>
#include <oneHLS/ac_types_support.h>   // opt-in: Siemens HLSLibs ac_types
#include <ac_fixed.h>

using Sample = ac_fixed<16, 8, true>;   // Q8.8
using Accum  = ac_fixed<32, 16, true>;  // Q16.16

// 4-tap FIR, coefficients as raw Q8.8 bit patterns (see "Why RawBitsCtor" below)
oneHLS::Fir<Sample, Accum, 10, 118, 118, 10> fir;
Accum y = fir.filter(Sample(x));

// Direct-form-I biquad: b1=0.5(128), b2=0.25(64); -a1=0.5(128), -a2=-0.25(-64)
oneHLS::Biquad<Sample, Accum, 128, 64, 128, -64> biquad;
Sample y2 = biquad.step(Sample(x));

// PID: Kp=1.0(256), Ki=0.25(64), Kd=0.5(128)
oneHLS::Pid<Sample, Accum, 256, 64, 128> pid;
Accum u = pid.step(Sample(error));
```

Swapping vendors is a one-line change — everything else, including the coefficients, is unchanged:

```cpp
#include <oneHLS/ap_types_support.h>   // instead of ac_types_support.h
#include <ap_fixed.h>
using Sample = ap_fixed<16, 8>;
using Accum  = ap_fixed<32, 16>;
// oneHLS::Fir<Sample, Accum, 10, 118, 118, 10> — identical instantiation
```

---

## Why `RawBitsCtor<T>`

Bit-accurate fixed-point coefficients need to be built from a pre-scaled *raw* integer (e.g. Q8.8's `128` means the real value `0.5`) — not from a `double` literal. Empirically, under Bambu HLS, a `static const Sample coeff = 0.5;` pattern does **not** fold to a constant: it synthesizes a real stateful lazy-init guard (an extra register + runtime branch) instead. Raw-bit construction (`ac_fixed::set_slc` / `ap_fixed::range()=`) avoids this entirely and is the one place a vendor's API genuinely differs — isolated behind the `RawBitsCtor<T>` trait so the FIR/IIR/PID logic itself never needs to know which vendor it's running on.

```cpp
template<typename T>
struct RawBitsCtor {
  template<int32_t RawBits> static constexpr T make() { return T(RawBits); }
};
// specialized per vendor in ac_types_support.h / ap_types_support.h
```

The default (`T(RawBits)`) is correct as-is for plain arithmetic types, where "raw bits" and "value" coincide — only bit-accurate fixed-point types need their own specialization.

---

## Vendor support headers

Opt-in, included alongside the vendor's own headers — `oneHLS.h` itself names no vendor type:

| Header | Vendor | Notes |
|---|---|---|
| `oneHLS/ac_types_support.h` | [hlslibs/ac_types](https://github.com/hlslibs/ac_types) | `RawBitsCtor` via `set_slc`. Full native + Bambu synthesis verified. |
| `oneHLS/ap_types_support.h` | [Xilinx/HLS_arbitrary_Precision_Types](https://github.com/Xilinx/HLS_arbitrary_Precision_Types) | `RawBitsCtor` via `.range(Hi,Lo)=`. Native/composition verified only — real upstream `ap_types` has been observed not to synthesize under Bambu in practical time; this is a Bambu limitation, not a OneHLS or `ap_types` one. |

Neither vendor library is vendored into this repo — clone them yourself and point your include path at them.

---

## Verified results

Native sequences (hand-derived, exact — no rounding, all coefficients are powers of two), reproduced bit-for-bit under **both** `ac_fixed` and `ap_fixed`:

| Component | Sequence |
|---|---|
| `Fir<>` impulse response | `0 10 118 118 10 0 0 0` |
| `Biquad<>` impulse response | `0 128 128 32 -16 -16 -4 2` |
| `Pid<>` constant error (e=1 ×5) | `448 384 448 512 576` |
| `Pid<>` impulse disturbance | `448 -64 64 64 64` |

Bambu HLS synthesis (`xc7a100t-1csg324-VVD`, 10 ns clock, `ac_fixed` instantiation) — zero warnings, real RTL, and **identical** resource counts to the hand-written, non-generic components this library replaces:

| Component | Flip-flops | Area | DSPs |
|---|---|---|---|
| `Fir<>` (4-tap) | 62 | 7679 | 0 |
| `Biquad<>` | 101 | 6855 | 0 |
| `Pid<>` | 32 | 3841 | 0 |

See [test/test.cpp](test/test.cpp) for the native regression suite and [.RnD/hls/](.RnD/hls/) for the Bambu synthesis targets.

---

## Dependencies

- C++17 or later
- [HAPI](https://github.com/InternetOfPins/HAPI) — `Chain<>`/`APIOf<>` composition
- [OneData](https://github.com/InternetOfPins/OneData) — `Data<T>` state storage
- A bit-accurate fixed-point library on the include path if using `ac_types_support.h` / `ap_types_support.h` (not vendored)
- No dynamic allocation; no exceptions; no RTTI

---

## License

MIT — see [LICENSE](LICENSE).
