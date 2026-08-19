# hls_core_components

Reproducible Bambu synthesis targets for OneHLS's five core, shipped
components — `Fir<>`, `Biquad<>`, `Pid<>`, `Accumulator<>`, `ComplexMac<>`
(all in `include/oneHLS/oneHLS.h`) — plus a native demo of all five. These
are the exact targets behind the main [README.md](../../README.md)'s
"Verified results" tables; this example exists so those numbers are
reproducible from a clean clone instead of only from local `.RnD/` scratch.

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

`ac_fixed`/`ac_int` (part of `ac_types`) need the same opt-in include as
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
pio run -e hls -t synthesize-fir
pio run -e hls -t synthesize-biquad
pio run -e hls -t synthesize-pid
pio run -e hls -t synthesize-accumulator
pio run -e hls -t synthesize-complex-mac
```

(A plain `pio run -e hls`, with no `-t`, tries to compile `src/main.cpp` as
a normal build — that's fine here, `src/main.cpp` needs no vendor headers
beyond `ac_fixed`/`ac_int`.)

## Results (verified, not estimated)

Native sequences and Bambu HLS resource counts are already published in
the main README's ["Verified results"](../../README.md#verified-results)
section — not reproduced here to avoid a second copy drifting out of sync.
Run the targets above to reproduce them directly.

## Files

- `hls/fir_top.cpp`, `hls/biquad_top.cpp`, `hls/pid_top.cpp`,
  `hls/accumulator_top.cpp`, `hls/complex_mac_top.cpp` — the five Bambu
  synthesis targets, one `oneHLS::` instantiation each, same
  coefficients/types as the main README's tables.
- `src/main.cpp` — native-only demo, prints all five components' verified
  sequences.
