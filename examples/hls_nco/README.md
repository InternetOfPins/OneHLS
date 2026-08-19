# hls_nco

A numerically controlled oscillator (`Nco<Sample,Accum,PhaseIncRawBits>`):
a phase accumulator (`oneHLS::Accumulator<>`, reused completely
unmodified) driving [Siemens HLSLibs `ac_math`](https://github.com/hlslibs/ac_math)'s
`ac_sin_cordic`/`ac_cos_cordic` directly. Genuinely different in kind
from every other example in this library — it's the one case that
**composes an existing external HLS primitive as-is**, rather than
reimplementing the algorithm in OneHLS's own idiom the way
`Fir`/`Biquad`/`Pid`/`Accumulator`/`ComplexMac`/the CIC decimator/the
polyphase FIR all do.

**This is research code, not yet part of the stable library.** `Nco`
(`src/nco.h`) isn't in `include/oneHLS/oneHLS.h` — see the main
[README.md](../../README.md) for the shipped, documented components.
Unlike those, `Nco` is also **not vendor-generic**: `ac_sincos_cordic`
is `ac_math`-specific (no `ap_fixed` equivalent), so this component is
necessarily tied to `ac_fixed`. That's the honest cost of composing a
borrowed primitive directly, not hidden.

## Why this one's interesting

The main [ECOSYSTEM.md](../../ECOSYSTEM.md) survey found that
`ac_math`/`ac_dsp`'s actual code is Catapult/MatchLib-shaped — raw
C-array state, `ac_channel<>` SystemC-style streaming I/O,
`mc_scverify.h`/`#pragma hls_unroll` Catapult-specific directives —
architecturally incompatible with HAPI's `Chain<>` composition. Using
them was expected to mean reimplementing the algorithm fresh, not
wrapping the original code. `ac_sincos_cordic` is the one exception:
pure and stateless (no internal state, no `ac_channel<>`, no
`mc_scverify.h`), so it's a real direct-composition candidate. This
example is that test, built and verified — see Results below.

## A real gotcha found verifying this

Two independent type requirements, neither documented upstream:

1. **`Sample`** (the phase accumulator's own type) must be
   `ac_fixed<W,1,true,...>` — exactly 1 integer bit — so 2's-complement
   wraparound on overflow lands exactly on `ac_sincos_cordic`'s own
   "angle scaled by π, range [-1,1)" convention for free: one full
   revolution is `angle_over_pi` advancing by 2.0, and wrapping mod 2 is
   precisely what an `AI=1` signed accumulator already does on overflow
   (same mechanism the main README's `Accumulator<ac_int<8,true>>`
   wraparound test already demonstrates).
2. **`Accum`** (the sin/cos *output* type) independently needs `AI>=2`.
   `ac_sin_cordic`/`ac_cos_cordic` each build an internal
   `ac_fixed<OW,OI,...> scale = 1.0;` constant, typed from the *output*
   reference argument. At `OI=1` the representable range is `[-1,1)` and
   `1.0` itself is out of range — it silently **wraps to `-1.0`** under
   the type's default `AC_WRAP`, negating (180°-rotating) every result.
   Isolated directly: the angle argument can stay `AI=1` unmodified —
   only the output type's `AI` matters. This maps exactly onto OneHLS's
   pre-existing `Sample`/`Accum` headroom convention (`Accum` always
   wider than `Sample` elsewhere in this library too) — no new shape
   needed, just applied correctly.

See `src/nco.h`'s header comment for the full derivation.

## Target device

Same convention as every other synthesis target in this library:
`--device-name=xc7a100t-1csg324-VVD --clock-period=10` (Xilinx Artix-7,
Digilent Arty A7/Nexys A7 — widely owned, not a special-order part; 10ns
targets 100MHz).

## Dependencies

`platformio.ini` declares HAPI and OneData as real `lib_deps` pointing at
their published repos — this example has to build for anyone who clones
just this repo, not only someone with the whole IOP monorepo on disk.
OneHLS's own current code (this repo, not a re-fetched copy of itself)
resolves locally via `file://../..`.

This example needs **two** opt-in vendor headers, not one — the only
example in this library that does:

```
git clone --depth 1 https://github.com/hlslibs/ac_types
export AC_TYPES_INCLUDE=$(pwd)/ac_types/include

git clone --depth 1 https://github.com/hlslibs/ac_math
export AC_MATH_INCLUDE=$(pwd)/ac_math/include
```

## Running it

**Native**:

```
pio run -e native -t exec
```

**HLS synthesis** (also requires `BAMBU_APPIMAGE` — see `extra_hls.py`):

```
export BAMBU_APPIMAGE=/path/to/bambu-2024.10.AppImage
pio run -e hls -t synthesize-nco
```

*Verification note: this example's underlying logic (native correctness
and the Bambu synthesis numbers below) was verified directly against
`g++`/the real `bambu` binary while building it, not through a `pio
run` — `pio` wasn't available in the environment this was written in.
The `platformio.ini`/`extra_hls.py` scaffolding follows this library's
already-working template exactly; if `pio run` itself behaves
differently than expected here, that's a gap in this note, not in the
underlying synthesis result.*

## Results (verified, not estimated)

Raw Bambu synthesis log backing the numbers below: [results/bambu_synthesis.txt](results/bambu_synthesis.txt)
(the full `.hls_out_nco/` build output itself is gitignored/regenerable — only the log is preserved here).

Native, exact — the standard unit circle at 8 evenly-spaced points (45°
steps), hand-derived, `PhaseInc=8192` (Q1.15 raw, `0.25` in
`angle_over_pi` units, 8 steps/revolution). All 8 steps land within 3 LSB
of ground truth and the sequence closes the loop back to the start value
— a real closed-loop wraparound check, not 8 independent points:

| Step (× 45°) | cos | sin |
|---|---|---|
| 0–7 | `.7071 0 -.7071 -1 -.7071 0 .7071 1` | `.7071 1 .7071 0 -.7071 -1 -.7071 0` |

Bambu HLS synthesis — clean, zero real warnings (only the same benign
vendor-header noise seen elsewhere in this library: unknown `#pragma
hls_waive`, `ac_std_float.h`'s `bfloat16` deprecated-copy, plus the same
expected "unknown addresses" note the main README documents for
`ComplexMac<>`'s pointer output — re-run directly against the files in
this directory, not just the `.RnD/` prototype, confirming promotion
changed nothing):

| | Flip-flops | Area | DSPs | States | Slack (10ns budget) | State binding |
|---|---|---|---|---|---|---|
| `Nco<>` | 1015 | 5186 | **0** | 20 | 2.91ns (140.98MHz) | **BRAM** |

**`Nco<>` is not zero-cost** — it's BRAM-bound (2× `ARRAY_1D_STD_BRAM_NN`),
almost certainly `ac_sincos_cordic`'s own internal 72-entry
`atan_pi_pow2_table`/`K_table` lookup tables (the CORDIC algorithm's own
constant tables), not OneHLS's `Data<T>` wrapper state — consistent with
the wide-state-binds-BRAM pattern the main README documents for
`ComplexMac<>`/`ac_std_float`, but the root cause here is inside the
borrowed primitive itself, not anything OneHLS added. Zero DSPs is
expected: real CORDIC is pure shift/add, no multiplies.

## Files

- `src/nco.h` — `Nco<>`, shared by both targets below so they compile
  the identical definition.
- `hls/nco_top.cpp` — the Bambu synthesis target.
- `src/main.cpp` — native-only demo, prints the 8-step unit-circle
  sequence above.
