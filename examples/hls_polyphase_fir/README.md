# hls_polyphase_fir

A Type-1 polyphase FIR decimator (M=2 branches), decomposing the same
4-tap coefficients as the main [README](../../README.md)'s own `Fir<>`
example (`10,118,118,10`) into two independently-typed `oneHLS::Fir<>`
sub-filters at compile time. The interesting part isn't the DSP theory —
it's what holds the branches: `oneHLS::StaticList<>`
(`include/oneHLS/staticList.h`), this library's general-purpose
heterogeneous-list utility, used here for its actual intended job.

**Why a plain array can't do this**: the two branches have different
coefficients — under this library's NTTP-coefficient convention, that
makes `Fir<Sample,Accum,10,118>` and `Fir<Sample,Accum,118,10>`
genuinely different C++ types. A homogeneous array (as used by this
library's [`hls_cic_decimator`](../hls_cic_decimator/) example, where
every stage really is the same type) can't hold them. `StaticList`
exists for exactly this case.

## Why this one's interesting

This example — specifically, the boilerplate problem of hand-writing a
new decimator's branches/flags/dispatch by hand for every `M` — is what
`StaticList` was built and promoted to solve. See
[docs/PHASE9_GENERIC_POLYPHASE.md](../../docs/PHASE9_GENERIC_POLYPHASE.md)
for the full derivation: five different internal designs were tried and
Bambu-synthesized before landing on this one (`StaticList`'s non-empty
list shape + runtime `visit()` dispatch), including a real HAPI bug found
and worked around along the way, and a genuine, still-open resource
trade-off (see Results below) — this example is the "graduated" result,
not the whole story.

**This is research code, not yet part of the stable library.**
`StaticList` itself is real and promoted (`include/oneHLS/staticList.h`)
— `PolyphaseFirDecim` (`src/polyphase_fir.h`) is one real, working use of
it, demonstrated here, not (yet) a second shipped Feature alongside
`Fir<>`/`Biquad<>`/`Pid<>`.

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
pio run -e hls -t synthesize-polyphase-fir
```

*Verification note: this example's underlying logic (native correctness
and the Bambu synthesis numbers below) was verified directly against
`g++`/the real `bambu` binary while building it, not through a `pio
run` — `pio` wasn't available in the environment this was written in.
The `platformio.ini`/`extra_hls.py` scaffolding follows
`hls_float_fir`'s already-working template exactly.*

## Results (verified, not estimated)

Native, exact — the real, Tap-delayed `Fir<>` impulse/DC response for
these coefficients, downsampled (an idealized, non-pipelined reference
model gives the wrong answer here — see
`docs/PHASE4_POLYPHASE_EXPERIMENT.md` §2 for why):

| | Sequence |
|---|---|
| Impulse response (decimated) | `0 118 10 0 0 0 0 0` |
| DC step response (decimated) | `0 128 256 256 256 256` |

Bambu HLS synthesis — clean, real RTL:

| | Flip-flops | Area | DSPs | Slack (10ns budget) |
|---|---|---|---|---|
| `PolyphaseFirDecim<M=2>` | 578 | 1030 | **0** | 0.497ns |

For scale, against the hand-written (non-generic, no `StaticList`) M=2
decimator this library's own research arrived at first: 550 FF / area
1895 / slack 1.312ns. This generic version wins substantially on area
(−46%) but gives up real timing margin (0.497ns vs. 1.312ns) — a genuine,
still-open trade-off, not glossed over. `docs/PHASE9_GENERIC_POLYPHASE.md`
§6 diffs five different internal designs against each other and explains
why: list representation and dispatch mechanism interact in a way that
isn't simply additive, and the variant used here (non-empty list +
`visit()`) is the best balance found so far — not a fully closed
question.

## Files

- `src/polyphase_fir.h` — `PolyphaseFirDecim<>` (coefficient-pack
  slicing + `oneHLS::StaticList<>` branch storage + commutator), shared
  by both targets below so they compile the identical definition.
- `hls/polyphase_fir_top.cpp` — the Bambu synthesis target.
- `src/main.cpp` — native-only demo, prints the impulse and DC step
  responses above.
