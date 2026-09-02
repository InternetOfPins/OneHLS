# Bambu `--pipelining` segfault — two chained windowed stages

**Status: FIXED upstream — https://github.com/ferrandi/PandA-bambu/issues/403**
(filed 2026-09 with all four repro files inlined; fixed by `0c00896`,
PR #369, merged 2025-11-17).

- **Segfaults** on Bambu ≤ `c2ba6936` (2024.10 AppImage).
- **Fixed** on `dev/panda` `0c00896` and later — verified on `380f327`
  (PandA 2026.06): `--pipelining` on the real 2-layer top exits 0 and
  generates RTL, no crash. The fix guards
  `tree_manager::RecursiveReplaceTreeNode` against replacing a tree node
  with a non-SSA node.
- **The II=1 claim is still Vitis-only, though** — for a different
  reason now (see "Impact" below): dev/panda declines to
  function-pipeline any body containing loops (*"Disabled function
  pipelining — not possible when one or more loops are present"*), where
  Vitis's `PIPELINE II=1` flattens them. Crash gone ≠ II=1 delivered.

Self-contained repro (no HAPI, no OneData, no fixed-point — `<cstdint>`
only) in `bambu_repro/` (`ISSUE.md`, `repro.cpp`, `line_buffer.h`,
`window_extract.h`, `stride2d.h`, `topWW_crash.log`).

## Environment

- Bambu **2024.10**, `Revision c2ba6936ca2ed63137095fea0b630a1c66e20e63-main`
  (official AppImage, https://release.bambuhls.eu/bambu-2024.10.AppImage)
- `--compiler=I386_CLANG16`, `--std=gnu++17`
- `--device-name=xc7a100t-1csg324-VVD --clock-period=10`
- Host: Ubuntu 24.04, x86-64

## Symptom

```
...
Time to perform register binding: 0.02 seconds
/tmp/.mount_bambu-*/usr/bin//tool_select.sh: line 14: NNNNNN Segmentation fault (core dumped) $BINARY_PATH "$@"
```

Exit 139. Only with `--pipelining` (or a `#pragma HLS PIPELINE II=1` on
the top). Without forced pipelining the same source synthesises cleanly
(II ≈ 50).

## Minimisation (`bambu_repro/repro.cpp` — self-contained)

| top design (all `--pipelining`) | result |
|---|---|
| plain chained circular buffers (2 rings, no window shift, no gate) | OK |
| two chained bare `WindowExtract` (line buffer + KxK window shift, **no** `Stride2D`) | OK |
| **one** `Stage` = `WindowExtract` + `Stride2D` | OK |
| **two chained `Stage`** = each (`WindowExtract` + `Stride2D`) | **SEGV** |
| the fully self-contained inline (ring flattened into `WindowExtract`) | OK — nesting matters |

Trigger: **two chained stages, each = a circular-indexed windowed line
buffer (`WindowExtract`, with its own nested `LineBuffer` sub-struct)
*plus* a multi-counter gate with a nested reset (`Stride2D`)**, under
forced pipelining. Remove either ingredient, or one of the two stages,
and it synthesises. Crash is after `register binding`, in the pipelining
stage (STG built at 29 states unpipelined, then segfault).

```
cd bambu_repro
bambu -I. --std=gnu++17 --compiler=I386_CLANG16 \
  --device-name=xc7a100t-1csg324-VVD --clock-period=10 \
  --top-fname=topWW --pipelining repro.cpp      # SIGSEGV
bambu -I. ... --top-fname=topW1 --pipelining repro.cpp   # OK
```

## Impact on the cnn_streaming R&D

Bambu can synthesise the multi-layer composition (proves zero inter-stage
FIFO) but **cannot force it to II=1**. Vitis HLS 2026.1 pipelines the
identical source to `Final II = 1, Depth = 18` with no trouble. So the
II=1 throughput claim in HANDOFF Round 8a rests on Vitis alone; the
zero-FIFO resource claim holds on both.

**After the fix (dev/panda `380f327`):** unchanged conclusion. The crash
is gone, but `--pipelining` on `oneHlsTwoLayerPipeTop` now reports
*"Disabled function pipelining — not possible when one or more loops are
present"* and falls back to per-loop II (inner conv loop II=1, pool /
outer loops II=3). Bambu's `--pipelining` is function-level and refuses
loop-containing bodies; Vitis flattens them under `PIPELINE II=1`. II=1
for the whole pipeline stays Vitis-only — now a pipelining-model gap,
not a defect.

## Draft confirmation comment for #403 (post + close)

> Confirmed fixed. Rebuilt `dev/panda` at `380f327` (PandA 2026.06) and
> re-ran the repro plus our real 2-layer streaming-CNN top with
> `--pipelining` on `xc7a100t-1csg324` — both exit 0 and generate RTL,
> no segfault after register binding. Matches your result. The `0c00896`
> guard on `RecursiveReplaceTreeNode` lines up with the crash site (the
> pipelining/register stage). Thanks — closing.
>
> (Note for anyone finding this later: the crash is gone, but a
> function body with loops still isn't function-pipelined — Bambu
> prints "Disabled function pipelining … one or more loops are
> present". That's expected behaviour, not this bug.)
