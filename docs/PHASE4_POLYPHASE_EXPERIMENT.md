# Phase 3/4/6: Polyphase FIR Decimator Experiment

Second, independent multirate domain for the CuTe/CUTLASS hypothesis (see
[CUTE_STUDY.md](CUTE_STUDY.md), [HIERARCHY_SKETCHES.md](HIERARCHY_SKETCHES.md)
§6, [PHASE4_CIC_EXPERIMENT.md](PHASE4_CIC_EXPERIMENT.md)). Deliberately
chosen because it replicates a **composite** (`oneHLS::Fir<>`) rather than
a raw atom (`oneHLS::Accumulator`) — the one thing CIC didn't test. Code
in [`.RnD/polyphase_experiment/`](../.RnD/polyphase_experiment/) — not in
`include/oneHLS/oneHLS.h`, same discipline as the CIC experiment.

---

## 1. What was built

`polyphase::PolyphaseFirDecim2<Sample,Accum,B0C0,B0C1,B1C0,B1C1>` — a
Type-1 polyphase decimator, M=2, decomposing README's own 4-tap `Fir<>`
coefficients (10,118,118,10):

- **Branch 0**: `oneHLS::Fir<Sample,Accum,10,118>` (even-indexed taps
  h0,h2), reused completely unmodified.
- **Branch 1**: `oneHLS::Fir<Sample,Accum,118,10>` (odd-indexed taps
  h1,h3), same unmodified `Fir<>` template.
- **Commutator**: alternates each incoming high-rate sample to branch 1
  then branch 0 (verified assignment, see §2); once both branches have
  produced a new output, sums them and marks `valid` — same `bool&
  valid`/hold-last-output contract as `cic::CicDecimator`.

**The predicted structural difference from CIC showed up immediately, at
the design stage, before any synthesis:** CIC's N stages are identical
(same `Accumulator<Accum,Accum>` type, same coefficients-less shape), so
a plain array (`Accumulator[N]`) held them. Polyphase's two branches are
structurally identical (both a 2-tap `Fir<>`) but *parametrically*
different — different coefficients — and under OneHLS's existing
raw-bits-as-NTTP convention, different coefficients means a genuinely
different C++ type: `Fir<Sample,Accum,10,118>` and
`Fir<Sample,Accum,118,10>` are unrelated types. **A homogeneous array
cannot hold them at all** — not "wasn't used," structurally can't compile.
Named members (`branch0`, `branch1`) are the smallest-possible-experiment
answer for M=2. This is the first real point in either domain where a
heterogeneous-tuple-style composition mechanism (closer to what CuTe
actually needed) might earn its keep for larger M — not confirmed, since
M=2 didn't need it, but for the first time not obviously unnecessary
either.

---

## 2. Ground truth — and a real derivation bug, caught by native testing

Two mistakes were made and corrected while building this, both worth
recording precisely rather than smoothing over:

1. **An idealized reference model gave the wrong answer.** The first
   ground-truth derivation modeled the branches as combinatorial FIRs
   (`y[n] = Σ h[k]·x[n-k]`, computed same-cycle). The native test failed
   immediately. Root cause: `oneHLS::Fir<>`'s real `Tap` implementation
   reads its *old* stored value before storing the new sample — a genuine
   extra cycle of pipeline delay, which is exactly why README's own
   verified `Fir<>` impulse response is `0 10 118 118 10 0 0 0` (leading
   zero) and not `10 118 118 10 0 0 0 0`. An idealized model doesn't
   reproduce that leading zero. The reference had to be re-derived from
   the real, already-verified `Fir<>` sequence, downsampled — not from a
   textbook FIR formula. Corrected expected sequence: impulse decimated
   output `0, 118, 10, 0, 0, 0, ...` (not `118, 10, 0, 0, ...`).
2. **Function-local `ac_fixed`/`ap_fixed` component instances reproduced
   an already-documented pitfall.** OneHLS's own `test/test.cpp` header
   comment already warns that `ac_fixed`/`ap_fixed` leave default
   construction's bits indeterminate, and only static storage duration
   gets language-mandated zero-init. The first test draft declared `dec`
   as a function-local and got garbage output on the first two decimated
   samples. Fixed by moving to file-scope instances, per the existing
   convention — not a new bug class, a recurrence of a documented one.

Both were caught by actually running the native test against real
computed values, not by re-reading the derivation — which is what native
verification is for. Final ground truth (both cross-checked against an
independent DC/step-input derivation, not just impulse):

| Input | Expected (low-rate output) |
|---|---|
| Impulse | `0, 118, 10, 0, 0, 0, ...` |
| DC step | `0, 128, 256, 256, 256, ...` |

---

## 3. Native correctness (verified, both vendors)

`ac_fixed<16,16,true>`/`ac_fixed<32,32,true>` and `ap_fixed<16,16>`/
`ap_fixed<32,32>` — same types as README's own `Fir<>` test (no
fractional bits; coefficients are plain integers). Both pass, both test
cases:

```
ac_fixed PolyphaseFirDecim2 impulse: ok (0 118 10 0 0 0 0 0)
ac_fixed PolyphaseFirDecim2 DC step: ok (0 128 256 256 256 256)
ap_fixed PolyphaseFirDecim2 impulse: ok (0 118 10 0 0 0 0 0)
ap_fixed PolyphaseFirDecim2 DC step: ok (0 128 256 256 256 256)
```

See [`native_test.cpp`](../.RnD/polyphase_experiment/native_test.cpp).

---

## 4. Phase 6: composed vs. monolithic, real synthesis

Baseline: [`polyphase_monolithic.h`](../.RnD/polyphase_experiment/polyphase_monolithic.h)
— `PolyphaseFirDecim2Monolithic<Sample,Accum,...>`, plain scalar tap-delay
registers per branch (no `Fir<>`, no HAPI types at all), built
operation-for-operation identical to the composed version (same
commutator, same coefficient-times-old-delay-before-shift order `Tap`
itself uses). Native-verified against the same corrected ground truth,
then both synthesized under identical settings (`xc7a100t-1csg324-VVD`,
10ns, `ac_fixed<16,16,true>`/`ac_fixed<32,32,true>`).

**Result: not bit-identical this time (unlike CIC) — but very close, and
notably not worse:**

| | Composed | Monolithic | Diff |
|---|---|---|---|
| Flip-flops | **550** | 582 | composed **−32 (−5.5%)** |
| Total area | 1895 | 1894 | +1 (~0%) |
| DSPs | 0 | 0 | — |
| Operations | 113 | 113 | 0 |
| Basic blocks / states | 9 / 18 | 9 / 18 | 0 |
| Cycles | 12 | 12 | 0 |
| Max frequency | 115.103 MHz | 115.103 MHz | 0 |
| Slack (10ns budget) | 1.312ns | 1.312ns | 0 |
| MUX_GATE | 28 | 26 | +2 |
| register_STD | 5 | 6 | −1 |

Same operation count, same STG shape, same timing — but a real, small
divergence in register/mux binding, unlike CIC's byte-identical result.
The composed version came out *ahead* on flip-flops, not behind, which
rules out the obvious worry (composition adding overhead); it does not
mean composition is free here the way it was for CIC — it means Bambu's
register allocator found a different, not-strictly-worse solution for
the two structurally different inputs. Given `ARRAY_1D_STD_BRAM_NN`
appears in both resource summaries (both bind `dec`'s ~1KB combined state
to real BRAM, not distributed RAM — the memory footprint here is larger
than CIC's, from two full 2-tap `Fir<>` delay lines plus commutator
state), this reflects real memory-binding decisions, not measurement
noise; the two logs' raw diff (numbering/timing lines only, checked the
same way as CIC's) confirms every non-cosmetic line accounted for above
is a genuine difference, not a diffing artifact.

---

## 5. Where this leaves the hypothesis

Two domains now built end-to-end, independently derived (Hogenauer's
integrator-comb identity for CIC, Type-1 polyphase decomposition here),
both multirate, both predicted in HIERARCHY_SKETCHES.md to need new
composition machinery:

- **CIC**: needed no new machinery (plain array), zero-cost, byte-identical
  to hand-written.
- **Polyphase**: needed no new machinery for M=2 (named members, since a
  homogeneous array structurally cannot hold heterogeneous-coefficient
  `Fir<>` instances), and the composed version is *not worse* than
  hand-written on any synthesized metric — better on flip-flops, a
  rounding error worse on area.

Neither domain required building the CuTe-inspired replicate/tile
algebra HIERARCHY_SKETCHES.md predicted. The one real, load-bearing
finding specific to this domain (not present in CIC): OneHLS's existing
raw-bits-as-NTTP coefficient convention means "replicate a parametrized
composite N times" is a **type-level** problem, not just a runtime-array
problem — worth remembering if a future domain needs M large enough that
named members (`branch0..branchM-1`) stop being the reasonable answer.
For M=2, they were.

---

## 6. Phase 8: pushing to M=4 — where does "named members are fine" stop?

§5 flagged named members as workable at M=2 but flagged as the one place
a heterogeneous-tuple mechanism might eventually earn its keep at larger
M. Rather than leave that as a guess, built the same decimator at M=4: an
8-tap symmetric FIR (`h=[1,2,3,4,4,3,2,1]`, chosen for easy exact-integer
verification, unrelated to README's 4-tap coefficients), 4 branches, each
still a genuinely different `Fir<>` type
([`polyphase_atoms_m4.h`](../.RnD/polyphase_experiment/polyphase_atoms_m4.h)).
Commutator re-derived by the same brute-force method against the real
Tap-delayed reference (not assumed to generalize from M=2's `offset=M-1`
pattern) — it does generalize, confirmed independently rather than
guessed. Native-verified (impulse + DC step), then both composed and a
hand-written monolithic M=4 baseline synthesized under identical settings,
this time with wall-clock/CPU compile time captured too (`/usr/bin/time
-v`) — a metric the original plan asked for and earlier phases here
hadn't reported.

**Resource result: the M=2 pattern holds, not degrades, at M=4.**

| | Composed | Monolithic | Diff |
|---|---|---|---|
| Flip-flops | **866** | 898 | composed **−32 (−3.6%)** |
| Total area | 2497 | 2495 | +2 (~0%) |
| DSPs | 0 | 0 | — |
| Operations / states / cycles | 176 / 26 / 12 | 176 / 26 / 12 | 0 |
| Max frequency | 115.103 MHz | 115.103 MHz | 0 |
| Compile time (wall) | 9.85s | 9.67s | +0.18s (~2%, noise) |
| Compile time (CPU) | 7.27s | 7.05s | +0.22s (~3%, noise) |

Same shape of result as M=2 (composed ahead on flip-flops, essentially
tied on everything else), and compile time did **not** blow up or even
meaningfully diverge between composed and monolithic — both ways of
writing this took the same ~10 seconds through Bambu.

**But the code did degrade, even though the synthesis numbers didn't.**
`polyphase_atoms_m4.h` already needs 4 named branch members, 4 named
`got*` flags, a 4-way `switch`, and a 4-term sum — roughly double
`polyphase_atoms.h`'s M=2 code for double the branches, which is the
expected scaling for hand-duplicated boilerplate, not for a real
composition primitive (a working `product`-style operator should scale
sub-linearly in code size, or at least not require touching N call sites
per N branches). At M=8 or M=16 this pattern stops being reasonable to
write by hand. **This is the actual boundary this study has found so
far** — not a synthesis-cost boundary (none has appeared in either domain
at any scale tried) but an authoring-ergonomics one, and it shows up
exactly where HIERARCHY_SKETCHES.md and §5 above predicted it might:
replicating a *parametrized composite*, at a large enough replication
count. The fix would be a real heterogeneous-tuple/fold-expression
mechanism (much closer to what CuTe's `Tiler`/`product` actually are) —
not attempted here; M=4 was chosen specifically to find this line, not
to cross it and build the fix in the same pass.
