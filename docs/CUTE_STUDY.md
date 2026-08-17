# What OneHLS Can Learn from CuTe

Phase 1 of the CuTe/CUTLASS hypothesis (see conversation record — no separate
ticket yet). Source: NVIDIA's own CuTe tutorial docs (`01_layout.md`,
`02_layout_algebra.md`, `03_tensor.md`, `0t_mma_atom.md`, read directly from
`NVIDIA/cutlass` on 2026-08-17), not CUTLASS as a whole. No code in this
document — terminology mapping only, to decide in Phase 2 whether the
hierarchy actually recurs in OneHLS's own problems before building anything.

---

## 1. CuTe in one paragraph

A `Layout` is a pair `(Shape, Stride)` and is, formally, **a function from
integers to integers** — it maps a logical coordinate to a memory index.
Everything else is built from three primitive operations on that function:
**composition** (`A∘B`, feed `B`'s output into `A`), **complement** (`A*`,
the "rest" of the codomain `A` doesn't touch), and from those two,
**divide** (split a layout into "the tile `B` selects" + "the repetitions of
that tile", i.e. `A ⊘ B := A ∘ (B, B*)`) and **product** (replicate a tile
`A` according to a layout `B`, i.e. `A ⊗ B := (A, A*∘B)`). A `Tensor` is a
`Layout` plus a data iterator (an `Engine`). An `Atom` is a hardware
instruction's physical register interface (`Operation`) plus a separate
metadata struct (`Traits`: logical shape, types, thread/value layout) —
kept deliberately apart so the same physical instruction can be reused
under different logical framings. A `TiledMMA`/`TiledCopy` is an `Atom`
replicated and interleaved across a larger shape via the product/divide
algebra above — tiling and partitioning are not separate machinery, they're
the same three primitives applied to different `Layout`s.

The thing worth transplanting is not `Shape`/`Stride`/`Layout` themselves —
it's that **tiling, partitioning, and cascading are all derived from two
primitives (composition + complement), not three separate hand-built
mechanisms.**

---

## 2. Terminology translation

| CuTe | OneHLS / HAPI equivalent | Fit |
|---|---|---|
| `Shape`/`Stride`/`IntTuple` (recursive index-space description) | *nothing* | **No fit — different domain, see §4** |
| `Layout` (function int→int, memory address space) | *nothing directly* — closest cousin is a `Chain<>` type list (a structure, not an index map) | **No fit** |
| `composition(A,B)` = `A∘B`, functional composition of two layouts | A `Chain<>`'s `Part<O>:O` call folding — `Tap<C>::mac()` calls `I::mac()` which calls the next layer's `mac()` | **Real structural parallel** — HAPI already composes *functions*, just not *index maps* (§3) |
| `Atom` (`Operation` + `Traits`, physical/logical split) | `Tap<Coeff>`, `Accumulator<Sample,Accum>` — already the smallest reusable state+op unit | **Already present, unnamed** |
| `Traits` struct (logical metadata decoupled from the physical instruction) | `RawBitsCtor<T>` (isolates the one vendor-specific physical detail — raw-bit construction — from the generic MAC/accumulate logic) | **Already present, unnamed** |
| `Tensor` = `Layout` + `Engine` (structure + storage) | a OneHLS component = `Chain<>` (structure) + `oneData::Data<T>` (storage) | **Real structural parallel** |
| `complement(A, M)` — "the rest" `A` doesn't cover | *nothing* | **No fit — no index space to complement** |
| `logical_divide` / tiling (split into tile + rest) | *nothing generic* — no OneHLS component currently has an N-in/1-out or 1-in/N-out shape to divide | **Open question, see §5 and ECOSYSTEM.md's CIC gap** |
| `logical_product` / `blocked_product` / `raked_product` — replicate a tile according to a layout | README's manual cascading: `y = stage2.step(stage1.step(x))`, N `Biquad` instances written out by hand | **Gap — this is exactly the ad hoc case CuTe's `product` generalizes** |
| `TiledMMA`/`TiledCopy` (Atom × ThrLayout × ValLayout) | *nothing* — no generic "replicate this Atom N times with this ordering" operator | **Gap, candidate for Phase 4** |
| `HAPI::Traverse`/`Map`/`Filter`/`FindFirst` (structural transforms over a `Chain<>`'s type list) | — | Already HAPI's own analog of "algebra over a nested structure", but it queries/transforms *existing* structure; it doesn't *derive new structure* the way `product`/`divide` do |

---

## 3. What already transfers (unnamed, already present)

Two of CuTe's design habits are already how OneHLS is built — they just
aren't named that way:

- **Atom / Traits split.** `RawBitsCtor<T>` is precisely CuTe's
  Operation/Traits separation: the one thing that's physically
  vendor-specific (how a raw bit pattern becomes a `T`) is isolated from
  the logic that's vendor-agnostic (the MAC, the delay line, the
  accumulate). `Fir`/`Biquad`/`Pid`/`Accumulator` are all, in CuTe's
  vocabulary, Atoms: a fixed hardware-instruction-shaped unit (one MAC +
  one delay register, for `Tap`) that composes into larger structures.
- **Composition as function-composition.** A HAPI `Chain<Tap<1>,Tap<3>,...>`
  calling `top.mac(x, 0)` really does unfold as `Tap1::mac ∘ Tap2::mac ∘ ...`
  in the same sense CuTe means `A∘B` — each layer's `mac()` calls
  `Base::mac()`, and the compiler folds the whole chain into one flat
  sequence (verified end-to-end for `hls_fir` in HAPI's `INDUSTRY.md`).
  CuTe composes *index-transforming functions*; HAPI composes
  *state-transforming functions*. Different codomain, same operation.

---

## 4. What doesn't transfer, and why

CuTe's algebra solves a **spatial** problem: given a logical coordinate,
where does the corresponding element live in memory? `Shape`/`Stride`
exist because GPU tensors are big, strided, multi-dimensional views over
flat memory, and the same logical tile needs to be addressable from
global memory, shared memory, and per-thread registers simultaneously.

OneHLS pipelines solve a **temporal** problem: given a new input sample
each clock, what state-carrying transform is applied, in what order? There
is no "index space" to complement or divide — a `Fir<>`'s delay line isn't
a strided view into a larger array, it's `N` registers each holding last
cycle's value. Importing `Shape`/`Stride`/`Layout`/`complement` literally
onto that would be solving a problem OneHLS doesn't have.

**The lesson to take is the discipline, not the machinery**: a *closed,
small algebra* — a couple of primitives with checked pre/post-conditions
(CuTe: `compatible()`, `congruent()`, the stride/shape divisibility
conditions) — from which every higher-level operation (tiling,
partitioning, in CuTe's case) is *derived*, rather than each higher-level
operation being its own hand-written special case.

---

## 5. The one gap worth testing

OneHLS already has a concrete, unplanned instance of exactly the problem
CuTe's `product`/`divide` solves generically. Two places in the existing
repo need "replicate this Atom N times, cascaded" and currently do it by
hand, one case at a time:

- README's cascading example: N `Biquad` sections in series is N instances
  and N manually-written `.step()` calls — correct, but not a reusable
  operator.
- ECOSYSTEM.md's CIC candidate (`## CIC decimator/interpolator — strongest
  fit`): the integrator section is explicitly described as "N cascaded
  `Accumulator`s: `y_i[n] = y_i[n-1] + y_{i-1}[n-1]`" — i.e. the exact
  "product of an Atom with a layout of repetitions" shape, plus that same
  doc's flagged open question that CIC breaks the "1 input in, 1 output
  out" `.step()` contract every current OneHLS component assumes (N-in/1-out
  for a decimator, 1-in/N-out for an interpolator) — which is a real,
  already-identified analog of CuTe's tile/rest split, not a hypothetical
  one invented for this document.

This makes CIC a stronger Phase-3 candidate than a from-scratch synthetic
pipeline: the hierarchy-discovery question ("is a cascade-of-atoms operator
actually needed, or would one more get written by hand and be fine?") is
already sitting in this repo's own backlog, unresolved, rather than needing
to be invented.

---

## 6. Candidate names (tentative — validate in Phase 2–4, not decided here)

Per the plan: don't define categories in advance, let the experiments say
whether they emerge. These are placeholders for what Phase 2's hierarchy
sketches would need to name, not a proposal to build:

| Role | Candidate | Status |
|---|---|---|
| Smallest reusable state+op unit | *Atom* (already exists: `Tap`, `Accumulator`) | Already present |
| Chaining atoms' `.step()` calls | *Chain* (HAPI's, already exists) | Already present |
| Replicate an Atom/Chain N times, cascaded | **unnamed — the actual gap** | To test in Phase 4, against CIC |
| Splitting a pipeline into "processed tile" + "the rest" (CIC's N-in/1-out problem) | **unnamed — open question** | Flagged in ECOSYSTEM.md already; not clearly needed elsewhere yet |

Explicitly avoid porting `Shape`/`Stride`/`Tensor`/`Engine` as names —
per §4 they describe a problem OneHLS's components don't have, and reusing
the names would misleadingly imply OneHLS has a memory-index algebra it
doesn't.

---

## 7. Recommendation for Phase 2

Sketch the natural hierarchy (per the plan's Phase 2 instructions) for the
streaming-DSP domain first, using CIC as the concrete test case instead of
a synthetic pipeline — it's the one place OneHLS's own backlog already
independently arrived at "N cascaded atoms" and "tile vs. rest" as real
open questions, before this document ever mentioned CuTe. If the
cascade-operator gap and the tile/rest split turn out to generalize past
CIC to a second, unrelated domain, that's the actual signal the plan is
looking for — not whether CuTe's specific machinery can be reproduced.

**Status: done, see [HIERARCHY_SKETCHES.md](HIERARCHY_SKETCHES.md).** Six
domains sketched (three already-built, three candidate); the gap does
generalize to a second, independently-derived domain (polyphase FIR, via
filter-bank theory rather than Hogenauer's integrator-comb identity), and
three domains (`Fir`, `Pid`, NCO) confirm it's *not* universally needed —
the operator earns its keep specifically where the sample rate changes.

**Phase 3/4/6 status: done for both domains — see
[PHASE4_CIC_EXPERIMENT.md](PHASE4_CIC_EXPERIMENT.md) and
[PHASE4_POLYPHASE_EXPERIMENT.md](PHASE4_POLYPHASE_EXPERIMENT.md).**

CIC: built and Bambu-synthesized a real decimator, then a hand-written
monolithic baseline. Two corrections to §5's prediction, not
confirmations: (1) stage replication (N identical integrators/combs)
needed no new HAPI "replicate-N" primitive — a plain array sufficed; (2)
the composed version is **line-for-line identical** in every Bambu
resource metric to the monolithic version (226 FF, area 9557, 0 DSPs,
same 144.898MHz Fmax — diffed, not eyeballed).

Polyphase FIR (the composite-replication case flagged above as the real
test): built and synthesized the same way. Real, unplanned finding at the
design stage, before any synthesis: replicating a *parametrized composite*
under OneHLS's raw-bits-as-NTTP convention is a **type-level** problem —
two `Fir<>` instances with different coefficients are different C++
types, so a homogeneous array literally cannot hold them; named members
were used instead (fine at M=2). Synthesis result: not byte-identical
like CIC, but composed came out **ahead** on flip-flops (550 vs. 582,
−5.5%) at ~identical area, same 0 DSPs, same timing — not worse on
anything.

**Net for both domains: no CuTe-style replicate/tile algebra needed
building.** HAPI's existing `Chain<>`/`APIOf<>`/`Data<T>` plus (array or
named members, chosen per whether the atoms are parametrically identical)
was sufficient, and composition cost nothing or came out ahead, never
behind.

**Phase 8 (find the boundary): pushed polyphase to M=4.** Same result at
2× scale — composed still ahead on flip-flops (866 vs. 898, −3.6%),
same area/DSPs/timing, and compile time (measured for the first time,
`/usr/bin/time -v`) tied at ~10s either way. No synthesis-cost boundary
found at this scale. But the *code* did degrade: M=4 needs 4 named
branches, 4 flags, a 4-way switch — linear boilerplate growth with no
sign of leveling off. That's the actual boundary this study has found so
far: not a resource-cost one (none has appeared anywhere yet), an
authoring-ergonomics one, exactly where predicted — replicating a
parametrized composite, once the replication count gets large. See
[PHASE4_POLYPHASE_EXPERIMENT.md](PHASE4_POLYPHASE_EXPERIMENT.md) §6.

**Phase 9 (close the boundary, build real new machinery): done, two
design passes — see [PHASE9_GENERIC_POLYPHASE.md](PHASE9_GENERIC_POLYPHASE.md).**
Built `PolyphaseFirDecim<Sample,Accum,M,Coeffs...>`, generalizing the
M=2/M=4 hand-written decimators into one instantiation.

Pass 1 used `hapi::At<idx,O>` for the M heterogeneous branches — this
surfaced a real, previously-latent bug in HAPI itself (`At<idx,O>`'s
`conditional_t` eagerly instantiates both branches, so even `idx==0`
needs `O::Base`, which `hapi::Nil` doesn't define — never hit because
`At`/`at()` have zero call sites anywhere in the IOP tree), but was
itself the wrong tool: `At<idx,O>` walks *one object's own* inheritance
chain (an ancestor-type view), not a lookup among independent siblings.
Redirected to pass 2: `oneMenu::StaticBody`'s already-proven head/tail
data-member composition + `visit(i,fn)` — the real, established pattern
for exactly this problem, one library over.

Pass 2's Bambu result **supersedes pass 1's** (that earlier report
should not be relied on): FF/area now land at parity-or-better vs.
hand-written at both scales (M=4: −25% FF, −26% area), and the timing
cost — a real ~11.5% `Fmax` penalty, tightest margin seen anywhere in
this study (0.187ns/10ns) — is **flat across M=2 and M=4**, not
worsening with scale the way pass 1's was. A bounded, one-time dispatch
cost, not a compounding one — a materially better-understood result than
the first pass gave, not just a cleaner implementation. The original
call-site-boilerplate problem is closed either way (an M=8 instantiation
is a template-argument change, not hand-duplication).

**Pass 3, two sub-passes, isolating list-shape from dispatch-mechanism**
(`PHASE9_GENERIC_POLYPHASE.md` §6): built `StaticList<O,OO...>` — a
standalone, HLS-scoped analog of `oneMenu::StaticBody`'s head/tail shape
(confirmed `visit(i,fn)` is a real, uniform interface `StaticBody`/
`CArrayBody`/`CPtrArrayBody`/`JoinBody` all share, not just one type's
own method), non-empty by construction (base case holds one real
element, no empty terminal anywhere), with both `visit(i,fn)` (3a) and a
new compile-time `getAt<I>()` (3b) as alternate dispatch mechanisms.

Five real data points now, not two, and the picture is an *interaction*,
not two independent effects: list shape helps a lot when paired with
runtime `visit()` (pass 2→3a: −34% area, timing margin +196% at M=4) but
has **zero measurable effect** paired with compile-time `getAt<I>()`
(pass 3b matches pass 1's numbers exactly — Bambu appears to normalize
away storage representation once every access is compile-time-resolved).
Dispatch mechanism itself does not uniformly win — it flips direction
with scale (`getAt<I>` ahead at M=2, `visit()` ahead at M=4) — the
original "compile-time access should just be faster" hypothesis was too
simple. Best overall generic variant found so far: **pass 3a**
(non-empty list + runtime `visit()`), not the compile-time mechanism
initially favored — smallest area, second-best FF, best timing margin of
any generic variant, though still short of hand-written's flat 1.3ns.

**Promoted to a general OneHLS utility** (`PHASE9_GENERIC_POLYPHASE.md`
§7): `StaticList` graduated from `.RnD/` to
[`include/oneHLS/staticList.h`](../include/oneHLS/staticList.h) — real,
public, opt-in header (not merged into `oneHLS.h`, not yet in README's
Features table). Added `oneHLS::staticList(a,b,c,...)`, matching
`oneMenu::staticBody()`'s exact factory idiom; caught and fixed a real
latent bug while building it (plain forwarding-reference deduction
silently produces reference members for lvalue arguments — a dangling-
reference trap — fixed with `std::decay_t`, matching `std::make_tuple`'s
own reasoning, verified with a dedicated mutate-after-build regression
test). Bambu graduation check confirms zero synthesis change from
promotion (exact match to pass 3a's numbers).

**2D "matrix" exploration** (§8): storage is already solved with zero new
code — `StaticList` nests (`staticList(staticList(...), staticList(...))`,
even with ragged row lengths, verified). `oneMenu::Row`/`Rows` turned out
to contribute screen-positioning logic, not storage — no OneHLS analog.
The real open problem, if a matrix use case ever appears, is neighbor-
aware 2D dataflow (e.g. a systolic array's PE[r][c] reading PE[r-1][c]/
PE[r][c-1]) — genuinely new machinery, and the one place this whole
detour reconnects to CuTe's own original motivation (`Layout`'s
`Shape`/`Stride`) rather than `oneMenu`'s precedent. Not built — flagged
as real future work.
