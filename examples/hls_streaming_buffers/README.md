# hls_streaming_buffers

**A composition / HLS result about buffer sizing — not a CNN inference
library.** It shows that a *hand-composed* streaming spatial pipeline
(sliding-window filters, pooling, small conv layers) needs **no
inter-stage FIFO**, and that every buffer it does need has a depth that
is a compile-time function of the layer shapes — no conservative
default, no measure-then-resize step.

The primitives are not CNN-specific; a convolutional layer is just the
first thing built from them. `test/test_morphology.cpp` builds a
streaming erode/dilate from the same `WindowExtract` + `ReduceTree` with
no convolution anywhere, to make that concrete.

## The claim, as a formula

A pipeline of streaming windowed stages holds, total:

```
Σ_stages  ( (K_s − 1) · W_s + 1 ) · C_s      line-buffer elements
        +  (small fixed per-stage: the K·K window + a few counters)
        +  0                                  inter-stage FIFO
```

`W_s`, `C_s`, `K_s` are **that stage's own** input width, channel count
and kernel size. `W_s` and `C_s` change down the pipeline (pooling
shrinks them, upsampling grows them) — walk each stage at its own
values, don't multiply one number by the stage count.

Contrast: **hls4ml**'s `io_stream` CNN path, by default, puts an
inter-layer FIFO of depth `out_H · out_W` between every pair of layers
(source: `hls4ml/backends/fpga/fpga_types.py`, `depth =
np.prod(shape) // shape[-1]`). Each FIFO element is an `n_chan`-wide
vector, so that is `Σ_seams (out_H · out_W · C · bits)` of buffering —
the full feature map per seam. For a 32×32×64 two-layer net that is
≈ 1.15 Mbit of FIFO; the hand composition's line buffers for the same
shapes are ≈ 8 KB per stage and there is no FIFO term at all.

## Why hls4ml needs the FIFOs and this doesn't

hls4ml generates one `#pragma HLS DATAFLOW` process **per layer**
(see `results/hls4ml/twolayer.cpp` — a real generated file):

```
#pragma HLS DATAFLOW
hls::stream<layer2_t> layer2_out;  #pragma HLS STREAM variable=layer2_out depth=100
hls::stream<layer3_t> layer3_out;  #pragma HLS STREAM variable=layer3_out depth=25
nnet::conv_2d_cl <...>(in,          layer2_out);
nnet::pooling2d_cl<...>(layer2_out, layer3_out);
nnet::conv_2d_cl <...>(layer3_out,  layer4_out);
```

Separate dataflow processes need FIFOs between them, and their depth is
sized conservatively because an imported graph can't know the real
requirement. The hand composition is **one function** — `l1.step()`
feeds `pool.step()` feeds `l2.step()`, each returning a `fire` strobe;
the next stage is only called on a cycle the previous one produced
output. No processes, no streams between stages, nothing to size.

```cpp
bool step(Sample in, Accum& out) {
  Accum a;  if (!l1.step(in, a))        return false;
  Sample b; if (!pool.step(Sample(a), b)) return false;
  return l2.step(b, out);
}
```

## Run it

```
# native demo: builds conv->maxpool->conv, prints the per-stage line
# buffers, checks against a nested-loop reference
pio run -e native -t exec

# per-primitive verification suite
for t in test/*.cpp; do g++ -std=c++17 -Iinclude ... "$t" -o /tmp/t && /tmp/t; done

# HLS synthesis (needs BAMBU_APPIMAGE + AC_TYPES_INCLUDE)
pio run -e hls -t synthesize-two-layer
pio run -e hls -t synthesize-axis
# ... see extra_hls.py for the full target list
```

## Verified results

Native: every primitive in `test/` matches a plain nested-loop
reference. Synthesis: PandA-Bambu 2024.10 (`xc7a100t-1csg324-VVD`,
10 ns) and AMD Vitis HLS 2026.1 (`xc7a100tcsg324-1`, 10 ns). Logs in
`results/`.

| design | inter-stage FIFO | line buffer | note |
|---|---|---|---|
| `LineBuffer<ac16,8,1,3>` | — | **17 words** = `(3−1)·8+1`, exact on both tools | |
| `WindowExtract` on `LineBuffer` | **none** | nested `LineBuffer` byte-identical to standalone | no buffering added at the seam |
| conv → 2×2 maxpool → conv | **none** (`FIFO: N/A`, both tools) | `25 + 11 + 11` elements, one per stage | one `step()` chain |
| same + `#pragma HLS PIPELINE II=1` | **none** | | `Final II = 1` on Vitis (Bambu `--pipelining` crashes — see below) |
| `MultiConv` Cin=4 Cout=4 | **none** | `((K−1)·W+1)·Cin`, channel-linear | |
| `Pad2D` (SAME) → conv | **none** | scales to padded width; **0** border storage | |
| AXI4-Stream adapter | skid FIFO = **pipeline `LatencyBound`** (~20), at the boundary only | | `tready` backpressure is not free, but bounded and static |
| `Transpose2D` (NHWC↔NCHW) | — | **`H·W·C`** — the whole frame | the one case where the buffer is not small (see scope) |
| streaming erode / dilate | **none** | same shape as the conv front-end | no CNN, no new code |

### DSP / FF numbers depend on the weight source — not a toolchain verdict

With weights as **compile-time literals**, Bambu strength-reduces the
multiplies to shift/add and reports **0 DSP**; Vitis maps them to DSP48.
With weights from **ROM** (how a trained layer's weights actually
arrive) neither tool can strength-reduce and the comparison **reverses
direction** — same 2-layer net: Bambu 0→6 DSP, Vitis 11→2 DSP
(`synthesize-two-layer` vs `synthesize-two-layer-rom`). DSP count is not
part of the claim here; read every DSP/FF figure with its
literal-vs-ROM label.

## Scope — results that hold only within stated bounds

| result | holds only when |
|---|---|
| `II = 1` for the multi-layer pipeline | on **Vitis** — Bambu `--pipelining` segfaults, filed: [ferrandi/PandA-bambu#403](https://github.com/ferrandi/PandA-bambu/issues/403) |
| the line-buffer formula | walked **per stage at its own `W`, `C`** — resolution-changing stages must be accounted for explicitly |
| `Requant` composes as a layer-output op | verified vs. an **integer-quantized scalar reference**, not against Brevitas / QONNX |
| `Transpose2D` is avoidable | the **whole pipeline is hand-composed end to end** — any external producer/consumer (a sensor's native layout, a fixed downstream block, an hls4ml-generated stage) makes the `H·W·C` frame buffer real |
| `ChannelConcat` costs nothing | branches have **equal spatial shape and arrive on the same cycle** — latency skew needs a `Delay` |
| AXIS `SkidDepth` is correct | `≥ Pipe::LatencyBound` — a `static_assert` enforces this; `LatencyBound` is a conservative *upper bound* (2-layer: bound 40 vs. real pipeline depth 18), not a description of the achieved depth |

## Not done (roadmap, not part of the claim)

- multi-channel `Conv2D` wired into an end-to-end net (`MultiConv`
  exists; `TwoLayerNetK3` is still `Cin = Cout = 1`)
- `Requant` / `Activation` integrated into a layer type
- a real trained / quantized network, end to end
- board bring-up over the AXIS adapter
- the Bambu `--pipelining` fix (upstream)

## Files

```
src/     primitives (header-only) + main.cpp native demo
test/    one file per primitive, each vs a nested-loop reference
hls/     one synthesis top per design (see extra_hls.py)
results/ hls4ml-generated comparison net + representative synth logs
```
