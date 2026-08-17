# OneHLS Component Hierarchy Sketches (Phase 2)

Phase 2 of the CuTe/CUTLASS hypothesis (see [CUTE_STUDY.md](CUTE_STUDY.md)).
The instruction for this phase is explicit: draw each domain's natural
component hierarchy first, and ask *"is this hierarchy already present in
the problem?"*, not *"can HAPI represent this?"*. Accordingly, every
hierarchy below is derived from the real signal-processing/control theory
for that structure (cited), not from HAPI's vocabulary — HAPI/`Chain<>`
naming is deliberately absent until the closing section.

**Scope note:** the plan's own Phase 2 examples include image processing
and a parser. Both are skipped here on purpose — OneHLS's declared scope is
synthesizable DSP/control components, and neither problem exists in this
repo. Sketching a hierarchy for a problem OneHLS doesn't have would be
inventing one, which is exactly what this phase is supposed to avoid.
Cross-domain generality (Phase 10's question) is instead tested with two
*independently derived* DSP sub-theories — Hogenauer's integrator-comb
decomposition and polyphase filter-bank theory — which share no common
ancestor besides both being multirate DSP.

---

## TL;DR

Six real hierarchies sketched: three already built (`Fir`, `Biquad`,
`Pid`), three not (CIC, NCO, polyphase FIR). Finding:

- **Atom reuse already happens, unplanned.** `Tap` is shared between `Fir`
  and `Biquad`; `Accumulator` is shared between `Pid`, CIC's integrator,
  and NCO's phase register. This isn't a HAPI feature being tested — it's
  what the components' own math already does, confirmed by re-reading the
  existing code, not designed for this exercise.
- **A cascade/replicate operator is only needed where the sample rate
  changes.** `Fir`/`Biquad`/`Pid`/NCO are all fixed-rate — cascading them
  (SOS filter design) is a convenience, never a structural requirement.
  CIC and polyphase FIR are both **multirate**, and both independently
  need a real "replicate this atom N times, cascaded" operator plus a
  rate-changing junction that breaks the current 1-in/1-out `.step()`
  shape. Two unrelated multirate derivations landing on the same
  requirement is the actual Phase-2 signal, not an invented convenience.
- **"Cascade" is not one operator.** SOS filter cascading (signal flows
  straight through, `y = stage2.step(stage1.step(x))`) and industrial
  cascade *control* (an outer PID's output becomes an inner PID's
  *setpoint*, not its input) are structurally different compositions that
  happen to share a name. Don't let the CuTe-inspired "replicate/cascade"
  operator absorb both — only the first is a replication in CuTe's sense.

---

## 1. FIR — already built

Direct-form FIR, textbook structure: one multiply-accumulate per tap, each
holding one delay register.

```
FIR filter
  tap × N   (delayed sample × coefficient, accumulate, shift delay)
```

`oneHLS::Tap` already **is** this atom; `Fir<Sample,Accum,Coeffs...>`
already **is** `Chain<Tap<C0>,...,Tap<Cn>>` (README, `oneHLS.h`). No gap —
this is the case where the existing implementation and the natural
hierarchy already coincide.

---

## 2. Biquad — already built

Direct-form-I 2nd-order IIR section:

```
Biquad section
  feedforward tap × 2   (b1, b2 on x[n-1], x[n-2])
  feedback tap × 2      (a1, a2 on y[n-1], y[n-2])
  summing junction
```

README states the feedforward taps *are* `Fir`'s `Tap` alias, reused
verbatim — i.e. the atom-reuse claim above isn't hypothetical, it's already
shipped for two of these six domains.

**Cascading N biquads (SOS — second-order sections)** is not a convenience
invented for this document: it's the standard way real IIR filters
(Butterworth, Chebyshev, elliptic) are implemented in every DSP library —
SciPy's `sos` format, MATLAB's `dfilt.df2sos`, etc. all decompose a
higher-order filter into cascaded biquads for numerical stability. README
already shows this pattern by hand (`y = stage2.step(stage1.step(x))`).
Structurally: **plain linear chaining**, not replication — each section
has different coefficients, so there's no "atom × N with a repetition
layout" to generalize, just N distinct instances in series. No gap.

---

## 3. PID — already built

```
PID controller
  proportional term   (Kp × e)
  integral term        (Accumulator over e)
  derivative term       (Kd × delta-e, one delay tap)
  summing junction
```

README confirms the integral term is "an inline `Accumulator`-shaped
chain" — `Accumulator` reused, not reimplemented.

**Does PID cascade the way Biquad does? No — and that's a useful negative
result.** There's no standard "N PIDs in signal-chain series" pattern.
There *is* a real, standard "cascade control" pattern in industrial
control theory (e.g. motor control's position→velocity→current loop
nesting, or temperature control with an inner flow loop) — but it composes
by **setpoint nesting**: the outer loop's *output* becomes the inner loop's
*target*, not its input signal. That's a different composition shape than
SOS chaining or CIC's atom replication — worth naming distinctly rather
than folding into one "cascade" operator (see TL;DR). Not pursued further
here since no OneHLS component currently needs it; flagged so a future
"cascade" operator doesn't quietly assume it means "signal chaining" when
a caller means "setpoint nesting."

---

## 4. CIC decimator/interpolator — candidate, real gap

Hogenauer's integrator-comb decomposition (the standard CIC structure,
already summarized in [ECOSYSTEM.md](../ECOSYSTEM.md)):

```
CIC filter
  integrator section   (N cascaded Accumulators, running at the HIGH rate)
  rate-change junction  (decimate: keep 1 sample in every R
                          interpolate: zero-stuff R-1 samples between real ones)
  comb section          (N cascaded delay-and-subtract stages, LOW rate:
                          y[n] = x[n] - x[n-M])
```

Two findings, both already flagged in ECOSYSTEM.md and reconfirmed here
against the real theory:

- **Atom reuse**: the integrator is literally N cascaded `Accumulator`s —
  the same atom already used by `Pid` and (below) NCO. The comb is a new,
  small atom (delay + subtract) not currently in OneHLS, but it's an
  atom, not a composite.
- **Real gap**: this is the first domain where "cascade N copies of the
  *same* atom" is a structural requirement (not a convenience — the whole
  point of Hogenauer's structure is that all N integrator stages and all N
  comb stages are identical), and where the rate-change junction breaks
  the 1-in/1-out `.step()` contract every current OneHLS component
  assumes (N-in/1-out for a decimator, 1-in/N-out for an interpolator).

---

## 5. NCO — candidate, no gap

Phase accumulator feeding a sin/cos generator (CORDIC or LUT):

```
NCO
  phase accumulator   (Accumulator, same-type wraparound)
  angle -> sin/cos      (stateless function call, e.g. ac_math CORDIC)
```

Shallow hierarchy, atom reuse confirmed again (`Accumulator`), but **no
cascading and no rate change** — a domain where the CuTe-style algebra
adds nothing. Included specifically as a negative data point: not every
DSP primitive needs the machinery CIC needs.

---

## 6. Polyphase FIR — candidate, second independent multirate case

Standard multirate filter-bank theory (polyphase decomposition — Harris,
*Multirate Signal Processing for Communication Systems*; Vaidyanathan,
*Multirate Systems and Filter Banks*), flagged in ECOSYSTEM.md as
structurally distinct from CIC:

```
Polyphase FIR (rate-change factor M)
  polyphase branch × M   (each an independent Fir<>, built from every
                            Mth coefficient of the original filter)
  commutator              (rate-change junction: routes each incoming/
                            outgoing sample to/from the next branch in
                            rotation)
```

The decomposition's whole premise is that the M branches are structurally
identical (same tap count, different coefficient subsets) — so, like CIC,
this needs a real "replicate N times" operator. The difference: here the
thing being replicated is a **composite** (`Fir<>` itself), not a raw
`Tap` — the operator has to work one level up the hierarchy from where
CIC needs it, over whatever atom/composite is handed to it, not just over
`Tap`. Second independent confirmation of the rate-change-junction problem
from CIC, arrived at by unrelated theory (polyphase decomposition, not
Hogenauer's integrator-comb identity).

---

## Cross-domain comparison

| Domain | Atomic unit(s) | Atom reused from elsewhere? | Needs replicate-N? | Needs rate-change (N-in/1-out or 1-in/N-out)? |
|---|---|---|---|---|
| `Fir` | `Tap` | — (originates here) | no | no |
| `Biquad` | `Tap` | yes, `Fir`'s | optional (SOS, linear chain not replication) | no |
| `Pid` | `Accumulator` | yes | no (setpoint-nesting is a different op, see §3) | no |
| CIC | `Accumulator` + new `Comb` | partial | **yes** | **yes** |
| NCO | `Accumulator` + CORDIC (stateless, external) | yes | no | no |
| Polyphase FIR | `Fir<>` (composite, one level up) | yes | **yes** | **yes** |

---

## Recommendation for Phase 3

The plan's Phase 3 says pick one domain "where the hierarchy is
undeniable" and explicitly steers away from starting with something already
well-organized (its own GEMM warning). CIC is that domain here, for a
narrower reason than "it looks well-structured": it's the *simplest* of
the two domains that actually need the new operator (replicate-N +
rate-change junction), it reuses an atom (`Accumulator`) already proven in
this codebase, and it only needs one new atom (`Comb`). Polyphase FIR
needs the same operator one hierarchy level higher (replicating a
composite, not a raw atom) — a good Phase-3-follow-up once CIC has
validated the operator at the simpler level, not a reason to start there.
