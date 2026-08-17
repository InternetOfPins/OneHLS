# Phase 3/4: CIC Decimator Experiment

Phase 3 ("pick one domain, build it") and Phase 4 ("implement the smallest
possible experiment, measure it") of the CuTe/CUTLASS hypothesis — see
[CUTE_STUDY.md](CUTE_STUDY.md) and [HIERARCHY_SKETCHES.md](HIERARCHY_SKETCHES.md),
which picked CIC as the domain. Code lives in
[`.RnD/cic_experiment/`](../.RnD/cic_experiment/) — **not** in
`include/oneHLS/oneHLS.h` yet, per ECOSYSTEM.md's own verified/cited
discipline: this hasn't earned a place in the stable library until the
open questions in §4 are resolved, not before.

---

## 1. What was built

`cic::CicDecimator<Sample,Accum,N,R>` — a Hogenauer CIC decimator:

- **Integrator section** (N stages, high rate): `oneHLS::Accumulator`,
  reused completely unmodified — zero new code.
- **Comb section** (N stages, low rate, differential delay M=1): one new
  atom, `cic::Comb` — structurally identical to `Tap`'s delay-register
  shape (one `Data<Accum>`, read-old/store-new/forward), subtracting
  instead of multiply-accumulating. M is fixed at 1 (Hogenauer's own most
  common recommendation) — general M via a cascaded delay line is a real
  follow-on, not attempted here, since it wouldn't test anything `Tap`/
  `FBTap`'s existing single-register chain hasn't already proven
  zero-cost.
- **Rate-change junction**: resolved the open design question
  ECOSYSTEM.md flagged (N-in/1-out breaks every other component's
  1-in/1-out `.step()`) with a `bool& valid` out-param — call every
  high-rate cycle, `valid` is true exactly 1-in-`R` calls.
- **Stage replication**: `Accumulator integrator[N]` and `Comb comb[N]`
  — **plain fixed-size arrays with a compile-time-bounded loop**, not a
  new HAPI "replicate atom N times" primitive.

That last point directly tests HIERARCHY_SKETCHES.md's prediction. That
document predicted CIC would need a real CuTe-style `product`-equivalent
operator because "all N stages are identical." Building it revealed
otherwise: a plain array of N already-zero-cost atoms was sufficient —
no new composition mechanism, no HAPI change. **The Phase 2 prediction
was too strong; this is the correction, not a confirmation.**

---

## 2. Native correctness (verified, both vendors)

Ground truth was generated independently — direct simulation of the exact
architecture (integrate at high rate, decimate, comb at low rate), not
copied from a textbook formula, plus a separate DC-gain cross-check
(steady-state output settles to `(R·M)^N`, the standard CIC DC-gain
identity, for N=2/R=4/M=1 → 16) to catch an architecture bug independent
of any C++ implementation bug.

For N=2, R=4:

| Input | Expected (low-rate output) | `ac_int<32,true>` | `ap_int<32>` |
|---|---|---|---|
| Impulse (`1,0,0,0,...`) | `4, 0, 0, 0, ...` | ✅ | ✅ |
| DC step (`1,1,1,...`) | `10, 16, 16, 16, ...` (settles to `(R·M)^N=16`) | ✅ | ✅ |

Both vendor instantiations are the exact same template, only `Sum`
changes — same type-genericity discipline as the rest of OneHLS's own
`test.cpp`. See [`native_test.cpp`](../.RnD/cic_experiment/native_test.cpp).

---

## 3. Bambu synthesis (real, not estimated)

Same device/clock convention as every other OneHLS target (README's
"Verified results" table): `xc7a100t-1csg324-VVD`, 10ns clock (100MHz),
`ac_int<32,true>` throughout. Clean synthesis, real RTL
([`cicDecimatorTop.v`](../.RnD/cic_experiment/.hls_out_cic_decimator/cicDecimatorTop.v),
93KB), one expected/benign "unknown addresses" note (the `bool*` out-param
— the same class of note README already documents for `ComplexMac<>`'s
pointer output, not a new issue):

| | Flip-flops | Area | DSPs | Slack (10ns budget) |
|---|---|---|---|---|
| `CicDecimator<N=2,R=4>` | 226 | 9557 | **0** | 3.10ns (met) |

For scale, against README's existing verified numbers (different
components, not a like-for-like comparison yet — see §4):

| Component | FF | Area | DSPs |
|---|---|---|---|
| `Accumulator<>` (8-bit) | 32 | 1880 | 0 |
| `Pid<>` | 32 | 3841 | 0 |
| `Fir<>` (4-tap) | 62 | 7679 | 0 |
| `Biquad<>` | 101 | 6855 | 0 |
| **`CicDecimator<N=2,R=4>`** | **226** | **9557** | **0** |

Every synthesized operation traces to real source: `ui_plus_expr_FU: 3`
(2 integrator adds + 1 counter increment), `ui_minus_expr_FU: 2` (2 comb
subtracts), `ui_eq_expr_FU: 1` (the `counter==R` check) — no unexplained
bloat. Zero DSPs confirms the theory: CIC is pure add/subtract, no
multiplies anywhere, unlike `Fir`/`Biquad`/`Pid` which at least have the
option of DSP-mapped multiplies at runtime-configurable coefficients. The
integrator/comb arrays bound to distributed RAM
(`ARRAY_1D_STD_DISTRAM_NN_SDS: 6`), the same resource class every other
zero-DSP OneHLS component already uses (per README's "State binding"
column) — not BRAM, so not the `ComplexMac<>`-style resource-cost
surprise.

---

## 4. What this does and doesn't prove yet

**Proven:** the architecture is correct (native, 2 vendors), it
synthesizes cleanly to real RTL with 0 DSPs and no unexplained resources,
and array-based stage replication needs no new HAPI machinery.

§3 alone proved the composed version is clean and explicable — it did not
yet prove it's *free*. That's what §5 settles.

---

## 5. Phase 6: is the composed version zero-cost?

Baseline: [`cic_monolithic.h`](../.RnD/cic_experiment/cic_monolithic.h) —
`CicDecimatorMonolithic<Sum>`, plain scalar members (`integ1`, `integ2`,
`comb1Prev`, `comb2Prev`, `counter`, `lastValid`), N=2/R=4 hand-unrolled
directly in the code, zero HAPI/OneHLS types anywhere. Written
operation-for-operation identical to §1's composed version (same add/
subtract order, same state-update order — traced by hand against
`AccumulateLogic`/`CombLogic`'s exact semantics, not just "does the same
thing" by eye), so any resource difference is attributable to the
composition mechanism itself, not to incidentally computing something
slightly different.

Native-verified against the exact same ground truth as §2 (both vendors,
[`native_test_monolithic.cpp`](../.RnD/cic_experiment/native_test_monolithic.cpp)),
then synthesized under identical settings
([`cic_decimator_monolithic_top.cpp`](../.RnD/cic_experiment/hls/cic_decimator_monolithic_top.cpp)).

**Result: the two Bambu logs are identical on every synthesis metric that
isn't a cosmetic difference in internal variable numbering or wall-clock
compile-time noise** — diffed line-for-line, not eyeballed:

| | Composed (§3) | Monolithic |
|---|---|---|
| Flip-flops | 226 | **226** |
| Total area | 9557 | **9557** |
| DSPs | 0 | **0** |
| Operations | 33 | **33** |
| Basic blocks / states | 6 / 6 | **6 / 6** |
| Cycles | 5 | **5** |
| Max frequency | 144.898 MHz | **144.898 MHz** |
| Slack (10ns budget) | 3.0986ns | **3.0986ns** |
| Resource summary (12-line breakdown: `ARRAY_1D_STD_DISTRAM_NN_SDS`, `ui_plus_expr_FU`, etc.) | — | **byte-identical** |

The only diffs in the two raw logs are internal variable IDs (memory
addresses assigned in declaration order — cosmetic) and `Time to perform
X: 0.01s` vs `0.00s` noise (wall-clock scheduling-pass timings, not a
hardware metric). Bambu's own optimizer folds the `Accumulator`/`Comb`
array-of-atoms composition down to the exact same STG, the exact same
register/module binding, and the exact same RTL resource count as the
hand-written version — this **is** the zero-cost abstraction claim,
earned the same way as every other resource number in this library
(README's Fir/Biquad/Pid/Accumulator table): by diffing two real Bambu
runs, not by asserting it from the composed version's clean synthesis
alone.

---

## 6. Where this leaves the hypothesis

Two real findings out of this domain, both corrections to what the
earlier phases predicted rather than confirmations of them:

- §1: the predicted "need a new CuTe-`product`-style replicate-N
  primitive" turned out false — a plain array sufficed.
- §5: the *composed* version (HAPI atoms in that plain array) costs
  literally nothing next to a from-scratch monolithic implementation —
  not "close," identical down to the resource-summary line.

Put together: for this domain, HAPI's existing composition machinery
(`Chain<>`/`APIOf<>`/`Data<T>`, plus a plain array for the one place
stages needed to repeat) was already sufficient to reach a genuinely
multirate, N-in/1-out component with zero synthesis cost — no CuTe-style
algebra needed to be built. That's a real answer to this document's
original question, not a partial one: for CIC specifically, the gap
predicted in Phase 2 doesn't exist once you build the thing.

**Recommendation:** the CIC domain has answered what it can. Per the
original plan's Phase 10 test ("if two unrelated domains independently
produce a hierarchy, that's stronger evidence"), the next real step is
polyphase FIR (HIERARCHY_SKETCHES.md §6) — the second, independently-
derived multirate domain — to see whether it *also* turns out not to need
new machinery, or whether replicating a composite (`Fir<>`) instead of a
raw atom is where a real gap finally shows up.
