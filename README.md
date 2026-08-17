# OneHLS

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Type-agnostic, HLS-synthesizable DSP and control components for **HAPI** + **OneData**.

`Fir<>`, `Biquad<>`, `Pid<>`, `Accumulator<>`, and `ComplexMac<>` are written once against a caller-chosen `Sample`/`Accum` type — the composition never names a vendor fixed-point type directly. The same unmodified templates have been verified against two independent bit-accurate fixed-point libraries (Siemens HLSLibs `ac_types` and Xilinx/AMD `ap_types`), both natively and synthesized to real RTL via [Bambu HLS](https://release.bambuhls.eu/), with bit-identical numeric results and gate-identical resource counts across both vendors.

---

## Features

| Component | Description | State |
|---|---|---|
| `Fir<Sample,Accum,Coeffs...>` | N-tap FIR filter, direct form | one `oneData::Data<Sample>` delay per tap |
| `Biquad<Sample,Accum,B1,B2,FBA1,FBA2>` | 2nd-order IIR section, direct form I | 2 feedforward + 2 feedback delays |
| `Pid<Sample,Accum,Kp,Ki,Kd>` | PID controller (`u = Kp·e + Ki·Σe + Kd·Δe`) | 1 accumulator + 1 delay |
| `Accumulator<Sample,Accum>` | Running sum; `Sample==Accum` gives 2's-complement wraparound instead of headroom | 1 `oneData::Data<Accum>` |
| `Complex<T>` / `ComplexMac<Sample,Accum,CoeffRe,CoeffIm>` | Complex multiply-accumulate over OneHLS's own vendor-agnostic complex struct | 1 `oneData::Data<Complex<Accum>>` — **not zero-cost, see below** |
| `RawBitsCtor<T>` | Customization point: build a coefficient from a raw, pre-scaled bit pattern | — |

All are ordinary HAPI `Chain<>` compositions under the hood (see [include/oneHLS/oneHLS.h](include/oneHLS/oneHLS.h)) — `Biquad`'s feedforward taps *are* `Fir`'s `Tap` alias, reused verbatim, and `Pid`'s integral term is an inline `Accumulator`-shaped chain.

**Cascading:** there's no separate "cascade" type — `Fir`/`Biquad` are plain value types, so N sections in series is just N instances with one `.step()`/`.filter()` output feeding the next's input:

```cpp
oneHLS::Biquad<Sample, Accum, 128, 64, 128, -64> stage1, stage2;
Sample y = stage2.step(stage1.step(x));  // 4th-order filter from two 2nd-order sections
```

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

// Running sum -- Sample==Accum gives 2's-complement wraparound
oneHLS::Accumulator<ac_int<8,true>, ac_int<8,true>> acc;
ac_int<8,true> total = acc.step(x);

// Complex multiply-accumulate, coefficient 2-1j
oneHLS::ComplexMac<Sample, Accum, 2, -1> cmac;
oneHLS::Complex<Accum> r = cmac.step(oneHLS::Complex<Sample>{Sample(xr), Sample(xi)});
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
| `Accumulator<ac_int<8,true>>` (+50 ×3, wraps) | `50 100 -106` |
| `ComplexMac<>` (coeff `2-1j`, input `3+4j` ×2) | `10+5j` then `20+10j` |

Bambu HLS synthesis (`xc7a100t-1csg324-VVD`, 10 ns clock, `ac_fixed` instantiation) — clean, real RTL, and **identical** resource counts to the hand-written, non-generic components this library replaces (`Fir`/`Biquad`/`Pid`/`Accumulator` synthesize with zero warnings; `ComplexMac<>`'s pointer-output params trigger one expected, benign "unknown addresses" note, not an error):

| Component | Flip-flops | Area | DSPs | State binding |
|---|---|---|---|---|
| `Fir<>` (4-tap) | 62 | 7679 | 0 | distributed RAM |
| `Biquad<>` | 101 | 6855 | 0 | distributed RAM |
| `Pid<>` | 32 | 3841 | 0 | distributed RAM |
| `Accumulator<>` (8-bit) | 32 | 1880 | 0 | distributed RAM |
| `ComplexMac<>` | 287 | 1108 | 0 | **BRAM** (see caveat below) |

See [test/test.cpp](test/test.cpp) for the native regression suite and [.RnD/hls/](.RnD/hls/) for the Bambu synthesis targets.

**`ComplexMac<>` is not zero-cost.** At 64 raw bits (`Complex<ac_fixed<32,32,true>>`), Bambu binds the accumulator to a real BRAM primitive (dual-port controller, address decoding) instead of the lightweight distributed RAM every other component here gets — reproducible and correct, just a different resource profile, not a defect. This was first observed with the vendor's own `ac_complex<T>` and has been re-confirmed to reproduce identically with OneHLS's own `Complex<T>`, so it's a property of the width/access pattern, not of any one struct definition.

Tested directly (2026-08-17) whether Bambu's own memory-allocation flags explain it — they don't, cleanly. Raising `--distram-threshold` (the size cutoff Bambu uses to pick distributed RAM, default 256 bits) to 4096 — far past the actual 64-bit state — changed nothing: identical BRAM binding, ruling out a simple size threshold outright. `--memory-allocation-policy=NO_BRAM` does remove the BRAM primitives, but doesn't reproduce the other components' clean internal distributed RAM either: it reports the state as *external* to the top module (a real memory-mapped interface, not self-contained) and costs *more* flip-flops, not fewer (287 → 383). The underlying trigger in Bambu's own memory-classification logic remains untraced — a real, tested negative result, not an unexamined assumption.

---

## Experimental: `ac_std_float`

`oneHLS::Fir<>` has also been verified — natively and synthesized — over `ac_std_float<W,E>`, HLSLibs' real IEEE754 bit-accurate floating point (not a fixed-point type at all). This is **not** a supported vendor path (no `ac_std_float_support.h` ships), just a documented finding:

- **Native:** `Fir<ac_std_float<32,8>, ac_std_float<64,11>, 10,118,118,10>` reproduces `0 10 118 118 10 0 0 0` exactly, with **zero changes** to `oneHLS.h` — the default `RawBitsCtor<T>` already does the right thing, since `ac_std_float`'s `explicit ac_std_float(int)` constructor treats an integer coefficient as its literal value. This only covers the whole-number-coefficient case; `Biquad`/`Pid`'s fractional Q8.8 raw-bit convention is fixed-point-specific and doesn't carry over as-is.
- **Bambu synthesis:** clean, and fast (16.4s, nowhere near a 500s bound). Unlike `ac_fixed`/`ap_fixed` — where `+`/`*` inline directly — Bambu keeps `ac_std_float`'s `operator+`, `operator*`, and its widening constructor as separate, shared hardware modules the top-level FSM calls into sequentially (confirmed via the generated Verilog's actual module instantiations, not just the synthesis log, since the log alone was ambiguous about whether these were real hardware or inlined-away build artifacts). Summed across all four real modules:

  | | Flip-flops | Area | DSPs | State binding |
  |---|---|---|---|---|
  | `Fir<>` over `ac_fixed` (baseline) | 62 | 7679 | 0 | distributed RAM |
  | `Fir<>` over `ac_std_float` | 3670 | 22682 | 12 | **BRAM** (17 controller instances) |

  ~59× the flip-flops, ~3× the area, and real DSP usage where fixed-point used none. This **confirms** conventional floating-point-on-FPGAs cost expectations rather than contradicting them — a real, expected finding, not an anomaly.

  The BRAM binding looks superficially like the same pattern seen with `ComplexMac<>`, but tested (2026-08-17) rather than assumed identical: the same `--distram-threshold`/`--memory-allocation-policy=NO_BRAM` experiments produce the same qualitative result (threshold has zero effect; `NO_BRAM` removes BRAM but nearly doubles the top function's flip-flops, 614 → 1123) — except `NO_BRAM` here externalizes **21 variables**, including an internal `ac_types` library lookup table (`ac_private::iv_leading_bits`) used by `ac_std_float`'s own normalization logic, versus exactly one (`cmac` itself) for `ComplexMac<>`. That's a much broader footprint, so despite the resemblance this is more likely a *different* root cause rooted in `ac_std_float`'s own arithmetic implementation, not the same wide/complex-type mechanism as `ComplexMac<>`.

See [.RnD/hls/fir_std_float_top.cpp](.RnD/hls/fir_std_float_top.cpp).

---

## Dependencies

- C++17 or later
- [HAPI](https://github.com/InternetOfPins/HAPI) — `Chain<>`/`APIOf<>` composition
- [OneData](https://github.com/InternetOfPins/OneData) — `Data<T>` state storage
- A bit-accurate fixed-point library on the include path if using `ac_types_support.h` / `ap_types_support.h` (not vendored)
- No dynamic allocation; no exceptions; no RTTI

---

## See also

[ECOSYSTEM.md](ECOSYSTEM.md) — where OneHLS sits relative to `ac_math`/`ac_dsp`, AMD/Intel HLS tooling, MatchLib, and FINN, plus a shortlist of algorithms worth borrowing next.

**Examples** — real, Bambu-synthesized, PlatformIO-buildable:
- [`examples/hls_float_fir`](examples/hls_float_fir) — `Fir<>` over real IEEE754 floating point (`ac_std_float`), same template unmodified.
- [`examples/hls_cic_decimator`](examples/hls_cic_decimator) — a genuinely multirate (N-in/1-out) CIC decimator, byte-for-byte diffed zero-cost vs. a hand-written monolithic version.
- [`examples/hls_polyphase_fir`](examples/hls_polyphase_fir) — a polyphase FIR decimator built on `oneHLS::StaticList<>`, this library's general-purpose heterogeneous-list utility.

**Research** — [docs/CUTE_STUDY.md](docs/CUTE_STUDY.md) is the entry point into an ongoing study of what OneHLS/HAPI can borrow from NVIDIA CuTe/CUTLASS's composition discipline; links from there cover the full derivation behind the CIC and polyphase examples above, including a real HAPI bug found along the way.

---

## License

MIT — see [LICENSE](LICENSE).
