# hls_cic_decimator

A [Hogenauer CIC decimator](https://en.wikipedia.org/wiki/Cascaded_integrator%E2%80%93comb_filter)
(N=2 integrator/comb stages, decimation factor R=4), built from
`oneHLS::Accumulator<>` — reused completely unmodified — plus one new,
small `Comb` atom, composed over HAPI the same way as every other
component in this library. Genuinely different in shape from `Fir<>`/
`Biquad<>`/`Pid<>`/`Accumulator<>`/`ComplexMac<>`: this is the first
**multirate** component (N samples in, 1 sample out), and it breaks the
1-in/1-out `.step()` contract every other component in this library
holds — resolved here with a `bool& valid` out-param: call `.step()`
every high-rate cycle, `valid` is `true` exactly 1-in-`R` calls.

**This is research code, not yet part of the stable library.**
`CicDecimator`/`Comb` (`src/cic_decimator.h`) aren't in
`include/oneHLS/oneHLS.h` — see the main [README.md](../../README.md)
for the shipped, documented components. This example demonstrates a
real, working, Bambu-synthesized decimator, and is where the promotion
decision would start if it's ever made.

## Why this one's interesting

Every other component in this library is `.step()`-in/`.step()`-out at
a fixed rate. Building a genuinely multirate structure was the actual
test: does HAPI composition (`Accumulator<>` reused, one new `Comb` atom)
stay zero-cost once the shape gets harder than "filter one sample,
return one sample"? It does — see Results below, a **byte-for-byte
diffed** comparison against a hand-written, non-generic monolithic
implementation (identical resource counts, not just similar).

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

`ac_int.h` (part of `ac_types`) needs the same opt-in include as
everywhere else in this library — see the main README's "Vendor support
headers" section:

```
git clone --depth 1 https://github.com/hlslibs/ac_types
export AC_TYPES_INCLUDE=$(pwd)/ac_types/include
```

## Running it

**Native**:

```
pio run -e native -t exec
```

**HLS synthesis** (also requires `BAMBU_APPIMAGE` — see `extra_hls.py`):

```
export BAMBU_APPIMAGE=/path/to/bambu-2024.10.AppImage
pio run -e hls -t synthesize-cic-decimator
```

(A plain `pio run -e hls`, with no `-t`, tries to compile `src/main.cpp`
as a normal build — that's fine here since `src/main.cpp` needs no
vendor headers beyond `ac_int`, unlike `hls_float_fir`'s `ac_std_float`
demo.)

*Verification note: this example's underlying logic (native correctness
and the Bambu synthesis numbers below) was verified directly against
`g++`/the real `bambu` binary while building it, not through a `pio run`
— `pio` wasn't available in the environment this was written in. The
`platformio.ini`/`extra_hls.py` scaffolding follows `hls_float_fir`'s
already-working template exactly; if `pio run` itself behaves
differently than expected here, that's a gap in this note, not in the
underlying synthesis result.*

## Results (verified, not estimated)

Raw Bambu synthesis log backing the numbers below: [results/bambu_synthesis.txt](results/bambu_synthesis.txt)
(the full `.hls_out_cic_decimator/` build output itself is gitignored/regenerable — only the log is preserved here).

Native, exact — both sequences independently derived by simulating the
architecture directly (not a textbook formula), cross-checked against a
separate DC-gain identity ((R·M)^N):

| | Sequence |
|---|---|
| Impulse response (decimated) | `4 0 0 0` |
| DC step response (decimated) | `10 16 16 16 16`, settles to `(R·M)^N = 16` |

Bambu HLS synthesis — clean, real RTL, one expected/benign "unknown
addresses" note (the `bool*` out-param, same class of note the main
README documents for `ComplexMac<>`'s pointer output):

| | Flip-flops | Area | DSPs | Slack (10ns budget) |
|---|---|---|---|---|
| `CicDecimator<N=2,R=4>` | 226 | 9557 | **0** | 3.10ns |

Zero DSPs confirms the theory: CIC is pure add/subtract, no multiplies
anywhere. Every synthesized operation traces to real source — no
unexplained bloat (see `PHASE4_CIC_EXPERIMENT.md` §3 for the full
resource breakdown).

**Composed vs. hand-written, diffed, not estimated**: a separate
hand-written monolithic version (plain scalar registers, zero HAPI
types, same N/R) was synthesized under identical settings and diffed
against this one line-for-line — **every non-cosmetic metric matched
exactly**: 226 FF, area 9557, 0 DSPs, 33 operations, 6 states, 144.898MHz
max frequency, 3.0986ns slack, byte-identical 12-line resource-summary
breakdown. Full diff in `PHASE4_CIC_EXPERIMENT.md` §5.

## Files

- `src/cic_decimator.h` — `CicDecimator<>`/`Comb<>`, shared by both
  targets below so they compile the identical definition.
- `hls/cic_decimator_top.cpp` — the Bambu synthesis target.
- `src/main.cpp` — native-only demo, prints the impulse and DC step
  responses above.
