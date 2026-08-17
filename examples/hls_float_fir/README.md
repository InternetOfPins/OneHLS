# hls_float_fir

`oneHLS::Fir<>` — the exact same template used with `ac_fixed`/`ap_fixed`
everywhere else in this library — instantiated with `ac_std_float`, real
IEEE754 bit-accurate floating point, and synthesized to real RTL via
[Bambu HLS](https://release.bambuhls.eu/). No code changes were needed to
get here: the default `RawBitsCtor<T>` already does the right thing for
`ac_std_float`'s integer-valued coefficients.

**This is not the primary or recommended way to use OneHLS.** `ac_fixed`
(Siemens HLSLibs) and `ap_fixed` (Xilinx/AMD) are the well-supported,
zero-cost backends — see the main [README.md](../../README.md). This
example demonstrates a real, working, but deliberately secondary
capability, and its own resource cost reflects that honestly (see
Results below): floating point on an FPGA costs what floating point on
an FPGA costs, and this doesn't try to hide it.

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

`ac_std_float.h` is handled differently, and deliberately not auto-fetched
even though it structurally could be: Bambu bundles its own older,
modified `ac_types` fork on its own default include path, so this
library's whole `AC_VERSION` guard convention exists to keep whoever's
building this consciously choosing which copy of `ac_types` is in play,
rather than have a dependency manager silently resolve "some" copy for
them. Same as `ac_types_support.h`/`ap_types_support.h` in the main
library — opt-in, point an include path at it yourself:

```
git clone --depth 1 https://github.com/hlslibs/ac_types
export AC_TYPES_INCLUDE=$(pwd)/ac_types/include
```

## Running it

**Native** (unlike HAPI's own `hls_fir` example, this one's native demo
uses `ac_std_float` directly, not a hand-rolled `int16_t`/`int32_t`
stand-in, so `AC_TYPES_INCLUDE` above is required even without Bambu):

```
pio run -e native -t exec
```

`extra_hls.py` picks up `AC_TYPES_INCLUDE` automatically for both
`env:native` and `env:hls` — no manual `platformio.ini` editing needed.

**HLS synthesis** (also requires `BAMBU_APPIMAGE` — see `extra_hls.py`).
No need for a normal build to succeed first: PlatformIO resolves
`lib_deps` during environment setup regardless of which target runs, so
the custom target below fetches HAPI/OneData into `.pio/libdeps/hls/`
(the same resolved copies a native build would use) as a side effect,
without ever needing `src/main.cpp` itself to compile:

```
export BAMBU_APPIMAGE=/path/to/bambu-2024.10.AppImage
pio run -e hls -t synthesize-float-fir
```

(A plain `pio run -e hls`, with no `-t`, tries to compile `src/main.cpp`
as a normal build and fails on `ac_std_float.h` unless `AC_TYPES_INCLUDE`
is also set — expected, and not something the custom target above needs.)

## Results (verified, not estimated)

Native, exact — both sequences hand-derivable and checked bit-for-bit:

| | Sequence |
|---|---|
| Impulse response | `0 10 118 118 10 0 0 0` |
| Step response (0→1 at sample 2) | `0 0 0 10 128 246 256 256` |

Bambu HLS synthesis — clean, ~16s, no errors. Unlike `ac_fixed`/`ap_fixed`
(where `+`/`*` inline directly), Bambu keeps `ac_std_float`'s arithmetic
operators and widening constructor as separate, shared hardware modules
the top-level FSM calls into sequentially. Summed across all four real
modules (confirmed via the generated Verilog's actual module
instantiations, not just the synthesis log):

| | Flip-flops | Area | DSPs |
|---|---|---|---|
| `Fir<>` over `ac_fixed` (this library's normal path) | 62 | 7679 | 0 |
| `Fir<>` over `ac_std_float` (this example) | 3670 | 22682 | 12 |

~59× the flip-flops, ~3× the area, and real DSP usage where fixed-point
used none — this **confirms** conventional floating-point-on-FPGA cost
expectations, it isn't a surprise or a defect. See the main README's
[Experimental: ac_std_float](../../README.md#experimental-ac_std_float)
section for the full story, including why this component's state ends
up BRAM-bound rather than the lightweight distributed RAM every
`ac_fixed`/`ap_fixed` component in this library gets.

## Files

- `hls/fir_std_float_top.cpp` — the Bambu synthesis target. Returns the
  raw IEEE754 bit pattern (what a real output port would give you), not
  a convenience-converted value.
- `src/main.cpp` — native-only demo, same composition, decodes to
  human-readable `double`s via `.to_double()` instead.
