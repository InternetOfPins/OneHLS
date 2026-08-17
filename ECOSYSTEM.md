# OneHLS in context

Where OneHLS sits relative to the numeric-type libraries it's built on,
the algorithm libraries it could borrow from, and the broader HLS/FPGA
ecosystem — plus a shortlist of algorithms worth reimplementing in
OneHLS's own idiom in a future round.

Three kinds of claim appear below, kept visibly distinct: **verified**
(tested in this repo, or read directly from the cited upstream source as
part of this survey), **investigated but blocked** (actually checked —
docs read, a real repo cloned and inspected — but the thing itself
turned out to require a resource, usually a proprietary toolkit, not
obtained here), and **cited** (reported as external context, not
independently checked against upstream source at all). Don't upgrade a
cited or blocked claim to verified without actually doing the work.

---

## Numeric backends: `ac_types` and `ap_types`, co-equal

OneHLS's core (`Fir<>`, `Biquad<>`, `Pid<>`, `Accumulator<>`,
`ComplexMac<>`) is written once against a caller-chosen `Sample`/`Accum`
type and demonstrated — **verified**, per [README.md](README.md) — against
two independent bit-accurate fixed-point libraries:

| Backend | Vendor | Status |
|---|---|---|
| `ac_fixed`/`ac_int` | Siemens HLSLibs `ac_types` | Native **and** Bambu HLS synthesis verified, resource-identical across every component |
| `ap_fixed`/`ap_int` | Xilinx/AMD `ap_types` | Native and composition verified; real upstream `ap_types` doesn't synthesize under Bambu in practical time (a Bambu limitation, observed directly, not a OneHLS or `ap_types` one) |

Both are real, both are type-generic drop-ins for the same templates —
this is deliberately not "an AC-types library that also happens to
compile against ap_fixed." A third axis, `ac_std_float` (real IEEE754
floating point), is documented as an experimental finding in
[README.md](README.md#experimental-ac_std_float): `Fir<>` synthesizes
over it with zero code changes, at a resource cost matching conventional
floating-point-on-FPGA expectations (~59× the flip-flops, ~3× the area,
real DSP usage, vs. the `ac_fixed` baseline).

---

## Borrowed-algorithm shortlist (candidates, not yet built)

Siemens HLSLibs also publishes `ac_math` (parameterized synthesizable
math: elementary functions, CORDIC trig, PWL approximations, linear
algebra, NN activations) and `ac_dsp` (DSP building blocks: FIR variants,
CIC filters, polyphase resamplers, FFT). Both were cloned and read
directly as part of this survey — **verified** reading, not going by
filenames or repo descriptions.

Their actual code is Catapult/MatchLib-shaped: raw C-array state,
`ac_channel<>` SystemC-style streaming I/O, `mc_scverify.h`/
`#pragma hls_unroll` Catapult-specific directives — architecturally
incompatible with HAPI's `Chain<>` composition. Using them means
reimplementing the *algorithm*, fresh, in OneHLS's own logic/`Data<T>`
idiom — the same way `Fir`/`Biquad`/`Pid`/`Accumulator` were always
original designs, never ports of someone else's exact code. Three
candidates surfaced, in order of fit:

### CIC decimator/interpolator — strongest fit

A Cascaded Integrator-Comb filter is, structurally, two things OneHLS
already has:
- **Integrator section** = N cascaded `Accumulator`s: `y_i[n] = y_i[n-1]
  + y_{i-1}[n-1]`.
- **Comb section** = an M-deep delay-and-subtract per stage: `y[n] =
  x[n] - x[n-M]` — a `Tap`-shaped variant that subtracts instead of
  multiply-accumulating. Confirmed rate-agnostic in the real source: the
  same comb code runs unmodified for both decimation and interpolation.

All rate-change logic (decimator: keep 1 integrator output in every R;
interpolator: zero-stuff R-1 samples between real ones) lives entirely
in the integrator side, as a counter-gated `if`/`else`. Topology mirrors:
decimator = Integrators(high rate) → rate-change → Comb(low rate);
interpolator = Comb(low rate) → rate-change → Integrators(high rate).
Bit growth follows the standard Hogenauer gain: `(R·M)^N` for the
decimator, `R^(N-1)·M^N` for the interpolator.

**Real open design question**: every existing OneHLS component is "1
input in, 1 output out" per `.step()`. CIC breaks that — a decimator is
N-in/1-out, an interpolator is 1-in/N-out. A `bool& valid` out-param (or
some other departure from the current calling shape) is a real design
decision for whoever implements this, not a mechanical detail.

### NCO (numerically controlled oscillator)

A phase accumulator feeding `ac_math::ac_sincos_cordic` — a pure,
stateless function (no internal state to reimplement, just call it).
One call returns both sine and cosine by reference. Its angle convention
is *radians scaled by 1/π* (range ≈ [-1,1) ≡ [-π,π)), with range
reduction done by truncating the angle type's integer width to 1 bit —
an ordinary 2's-complement wraparound. That means a phase accumulator
sized exactly to this angle type wraps into the correct domain for free
via `Accumulator<Sample,Accum>` with `Sample==Accum` — the exact
same-type-wraparound behavior already shipped and tested
(`Accumulator<ac_int<8,true>>`'s `50 100 -106`). Output naturally fits
the existing `Complex<Sample>` type from `ComplexMac`.

Interesting because it's signal *generation*, complementing the
library's existing signal-*processing* components — and because it
needs zero new state-holding machinery, only the CORDIC call itself
wrapped around two primitives OneHLS already has.

### Polyphase FIR decimation/interpolation — a different family

`ac_poly_dec`/`ac_poly_intr` were initially assumed to be a CIC variant;
reading them in full shows otherwise — they're coefficient/MAC-driven
tapped delay lines, structurally close to `Fir<>`/`Tap<>`, not to
`Accumulator`/CIC's integrator-comb shape. Worth a real look in a later
round as a rate-changing sibling of `Fir<>`, not bundled with the CIC
work above.

---

## Intel/Altera HLS — real, but proprietary-toolkit-gated

Unlike the rest of this document's ecosystem table, this one got a real
investigation, not just a citation — worth documenting precisely,
including a correction to an earlier, too-hasty read of it.

**Confirmed real** (Intel's own documentation): the Intel HLS Compiler
and oneAPI FPGA add-on expose `ac_int`, `ac_fixed`, `ac_complex` (stated
as "based on Algorithmic C data types provided by Mentor Graphics"), an
`ac_fixed_math.hpp` with `sqrt_fixed`/`sincos_fixed`/`log_fixed`-style
functions, and a fourth, genuinely distinct type, `ap_float` (its own
`ap_float.hpp`/`ap_float_math.hpp`) — **not** the same as `ac_std_float`
or Xilinx's `ap_fixed` despite the similar name.

**Confirmed NOT a drop-in equivalent to what's already tested here** —
this is the correction. The initial read of Intel's docs ("based on
Algorithmic C data types... references github.com/hlslibs/ac_types")
was taken to mean OneHLS's existing, already-verified `ac_types` testing
transfers directly. Direct investigation shows otherwise:
- The real include path is namespaced under Intel's own SYCL extension
  tree — `<sycl/ext/altera/ac_types/ac_int.hpp>`,
  `<sycl/ext/altera/ac_types/ac_fixed.hpp>` — not the plain `<ac_int.h>`/
  `<ac_fixed.h>` real upstream `hlslibs/ac_types` (and OneHLS's
  `ac_types_support.h`) actually use.
- Cloned and inspected `altera-fpga/hls-samples` (Intel/Altera's own
  public samples repo) directly, looking for a vendored copy of the
  actual header implementation: none exists. Every sample only
  `#include`s the SYCL-namespaced path and assumes the real
  implementation is already installed — it ships **only** inside the
  Intel oneAPI DPC++/C++ Compiler, not as a separately obtainable
  package.
- "Based on Algorithmic C data types" is Intel's own soft framing, not
  a byte-identical guarantee — Bambu's own bundled `ac_types` fork used
  exactly this kind of language too, and turned out to be an older,
  modified copy (`AC_VERSION 3` vs. real upstream's `4`), the whole
  reason this investigation's `AC_VERSION` guard exists in the first
  place. Assuming Intel's copy is equivalent without checking would be
  repeating that exact mistake.

**Net status: real, but genuinely untested, not "effectively already
covered."** Actually testing either `ac_int`/`ac_fixed`/`ac_complex` or
`ap_float` under Intel's own toolchain would require installing the
Intel oneAPI DPC++/C++ Compiler — a multi-gigabyte, license-gated
install, not something to do without an explicit decision to spend that
effort.

---

## Where OneHLS sits (ecosystem comparison — cited, not independently verified)

The following is reported as external context, gathered outside this
repo's own testing — treat it as a starting map, not a verified claim,
until someone actually checks it the way `ac_types`/`ac_math`/`ac_dsp`
(and, above, Intel/Altera's packaging) were checked.

| Project | What it is | Relevance to OneHLS |
|---|---|---|
| NVIDIA MatchLib / Connections | C++/SystemC library of synthesizable hardware components — channels, FIFOs, arbiters, AXI components, memories — built on the Connections latency-insensitive channel model | A good architectural neighbor, not a competitor: MatchLib composes hardware *modules and communication*; OneHLS composes parameterized *algorithms/datapath components*. Different axis, same neighborhood |
| FINN | Generates customized FPGA dataflow architectures from quantized neural network models | A much higher abstraction level (NN model → dataflow architecture → FPGA) than OneHLS (C++ types + components → statically composed algorithm → HLS). Worth an ecosystem mention, not a comparison of equals |

---

## Architecture: how the three IOP HLS-relevant libraries divide the problem

- **HAPI** — how components compose (`Chain<>`/`APIOf<>`, compile-time,
  zero-overhead).
- **OneData** — how data is represented/composed (`Data<T>` and its
  modifiers — change tracking, ranges, defaults, translation).
- **OneHLS** — how synthesizable DSP/control *algorithms* compose,
  built on top of both.

AC types, AP types, and the experimental `ac_std_float` axis sit
underneath all three as interchangeable numeric backends — none of them
are named anywhere in HAPI or OneData's own code, and OneHLS only names
them in its opt-in `*_support.h` headers. That separation — composition
engine, data representation, and algorithm library as three independent
layers, with vendor numeric types as swappable backends rather than a
foundation any layer is built to assume — is the actual architectural
claim being made here, not just "another HLS component library."
