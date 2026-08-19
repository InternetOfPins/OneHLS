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

## Post-route verification: Bambu vs. Vitis, both through real Vivado P&R

The tables above (and the main README's) report each tool's own HLS-stage
numbers — for Bambu that's pre-route, for Vitis that's a pre-implementation
estimate. Neither is "achieved clock frequency" in the sense a downstream
FPGA engineer means it. Four of the five components (`Fir<>`, `Biquad<>`,
`Pid<>`, `ComplexMac<>` — `Accumulator<>` not included in this pass) were
additionally pushed through a **real Vivado 2026.1 synthesis → place →
route flow**, for both tools' RTL output, same part
(`xc7a100tcsg324-1`), same 10ns target clock, same synthesis/implementation
strategy (`-directive Explore`) on both sides — the only thing that varies
between the two rows per component is which tool produced the RTL:

| Component | Tool → Vivado | Achieved clock | Fmax | Cycles | Latency |
|---|---|---|---|---|---|
| `Fir<>` (4-tap) | Bambu | 6.791 ns | 147.25 MHz | 2 | 13.58 ns |
| `Fir<>` (4-tap) | Vitis | 6.959 ns | 143.70 MHz | 4 | 27.84 ns |
| `Biquad<>` | Bambu | 6.860 ns | 145.77 MHz | 3 | 20.58 ns |
| `Biquad<>` | Vitis | 4.123 ns | 242.54 MHz | 1 | 4.12 ns |
| `Pid<>` | Bambu | 5.851 ns | 170.91 MHz | 2 | 11.70 ns |
| `Pid<>` | Vitis | 2.255 ns | 443.46 MHz | 0 (combinational) | 2.26 ns* |
| `ComplexMac<>` | Bambu | 5.654 ns | 176.87 MHz | 5 | 28.27 ns |
| `ComplexMac<>` | Vitis | 2.954 ns | 338.53 MHz | 0 (combinational) | 2.95 ns* |

\* Vitis schedules `Pid<>`/`ComplexMac<>` with zero register stages — there's
no cycle count to multiply by the clock period. The number shown is the
single-pass combinational propagation delay (the achieved clock period
itself), not a rounding of a literal "0 ns".

**Which tool wins depends on the component, not a blanket answer.**
`Biquad<>`/`Pid<>`/`ComplexMac<>` all route faster under Vitis's flatter
scheduling. `Fir<>` is the exception: the two tools' real routed clocks
land within 3% of each other there, so Bambu's latency win on `Fir<>`
comes almost entirely from scheduling into 2 cycles against Vitis's 4, not
from a faster clock.

**Bambu's own pre-route frequency estimate is conservative across all
four components** (12%–54% below the real routed Fmax) — see each
target's own log below for the exact "Estimated max frequency" line vs.
the post-route result.

**II is not answerable from either tool's post-route data.** Vitis reports
an initiation interval at the HLS-scheduling stage only (5/2/1/1 cycles for
Fir/Biquad/Pid/ComplexMac) — a scheduler output, not something the routed
timing data verifies. Bambu exposes no II-equivalent for these targets at
all; the closest its log gets is a qualitative "function pipelining may
come for free" note, no number attached.

**Methodology**: Bambu's own `--evaluation=` flags drive it to generate and
run a full Vivado out-of-context synth→place→route TCL script
automatically (not something this project added). The Vitis side reuses
that exact same Bambu-generated TCL flow verbatim — only the RTL source
file(s), top module name, and clock port name (`clock` → `ap_clk`) are
swapped; part number, clock period, and every `-directive Explore` setting
are untouched, so the comparison isolates "which tool's RTL routes better"
from "which Vivado settings were used."

Raw logs and every Vivado report (`post_synth_*`, `post_place_*`,
`post_route_*`) for both sides live in [`bambu_vivado_check/`](bambu_vivado_check/)
and [`vitis_vivado_check/`](vitis_vivado_check/), one subfolder per
component — not regenerated by `extra_hls.py` (that would additionally
require Vivado on `PATH`, out of scope for the standard `synthesize-*`
targets above), preserved here as the actual evidence behind the table.

## Files

- `hls/fir_top.cpp`, `hls/biquad_top.cpp`, `hls/pid_top.cpp`,
  `hls/accumulator_top.cpp`, `hls/complex_mac_top.cpp` — the five Bambu
  synthesis targets, one `oneHLS::` instantiation each, same
  coefficients/types as the main README's tables.
- `src/main.cpp` — native-only demo, prints all five components' verified
  sequences.
- `bambu_vivado_check/`, `vitis_vivado_check/` — post-route Vivado logs
  and reports backing the table above.
