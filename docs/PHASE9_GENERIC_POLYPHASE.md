# Phase 9: Generic Polyphase Decimator — Closing the M=4 Boilerplate Boundary

Closes the authoring-ergonomics boundary `PHASE4_POLYPHASE_EXPERIMENT.md`
§6 found: scaling the hand-written polyphase decimator from M=2 to M=4
needed O(M) call-site boilerplate (M named branches, M flags, an M-way
switch, an M-term sum) with no sign of leveling off, even though the
*synthesized hardware* stayed clean at both scales. This is the first
genuinely new piece of composition machinery this whole CuTe-hypothesis
study has produced — see [CUTE_STUDY.md](CUTE_STUDY.md),
[HIERARCHY_SKETCHES.md](HIERARCHY_SKETCHES.md),
[PHASE4_POLYPHASE_EXPERIMENT.md](PHASE4_POLYPHASE_EXPERIMENT.md). Code in
[`.RnD/polyphase_experiment/`](../.RnD/polyphase_experiment/) — `polyphase_generic.h` —
not in `include/oneHLS/oneHLS.h`, same discipline as every prior
experiment in this study.

This document went through two design passes; both are recorded, because
the wrong-turn is as informative as the fix.

---

## 1. Design pass 1 (superseded): `std::tuple`, then `hapi::At<idx,O>`

First draft used `std::tuple`+`std::get<I>` for the M heterogeneous
branches (genuinely different `Fir<>` types — different coefficients
under OneHLS's NTTP convention). Flagged during review as skipping HAPI's
own "exhaust HAPI machinery before `std::` alternatives" principle.
Redirected toward `hapi::At<idx,O>` instead — a minimal recursive-
inheritance `BranchList` walkable the same way `At<>` walks a `Chain<>`.

**That redirect was itself wrong**, caught on a second review: confirmed
against `HAPI/docs/REFERENCE.md` and the real code that `At<idx,O>` walks
**one assembled object's own inheritance chain**, `idx` levels up via
`::Base`, returning an *ancestor-type view of that same object* — the
right tool for reaching a specific layer of a `Chain<>` mono_block
composition, not for looking up one of several independent siblings.
Using it for "pick branch `i` out of M distinct `Fir<>` instances" worked
mechanically (the recursive-inheritance shape happens to satisfy what
`At<>` expects) but was fighting the tool's actual purpose, not using it
correctly.

Trying it anyway did surface one real, separate finding, kept for the
record: `At<idx,O>`'s `std::conditional_t<(idx>0), At<idx-1,O::Base>::Type, O>`
eagerly instantiates **both** `conditional_t` branches regardless of
which is selected (ordinary C++ semantics) — so even the `idx==0`
terminal case needs `O::Base` to exist, which `hapi::Nil` (the real
`Chain<>` terminal) does not define either. This would misfire on any
real `Chain<>` access at `idx==0`, not only this experiment's discarded
`BranchList`. Apparently never hit because `At<idx,O>`/`at<idx,ref>()`
have zero call sites anywhere in the IOP tree. Not fixed here — a real
HAPI-level finding, out of scope for an OneHLS-local file.

---

## 2. Design pass 2 (adopted): `oneMenu::StaticBody`'s head/tail/`visit()`

The right precedent, pointed out directly: `oneMenu::StaticBody`
(`OneMenu/include/oneMenu/menu/body/staticBody.h`) already solves exactly
this shape of problem — "M genuinely heterogeneous items, dispatch to
one of them by a *runtime* index" — for real menu items in production
code. It holds items via plain **data-member composition**, not
inheritance (`Head head; Tail tail;`), and its `visit(Sz i, Fn&& fn)`
recurses by decrementing `i`, calling `fn(head)` once `i` reaches 0:

```cpp
template<typename Fn>
auto visit(Sz i, Fn&& fn) {
  if constexpr (sizeof...(OO)==0) { (void)i; return fn(head); }
  else return i ? tail.visit(i-1,std::forward<Fn>(fn)) : fn(head);
}
```

`BranchList` in this file is a small, standalone analog of exactly that
shape (OneHLS's declared dependencies are HAPI+OneData only — pulling in
all of OneMenu for one data structure would be the wrong kind of reuse):

```cpp
template<typename O, typename... OO>
struct BranchList<O,OO...> {
  using Head = O;
  using Tail = BranchList<OO...>;
  Head head{};
  Tail tail{};

  template<typename Fn>
  void visit(int32_t i, Fn&& fn) {
    if constexpr (sizeof...(OO) == 0) { (void)i; fn(head); }
    else { if (i) tail.visit(i-1, std::forward<Fn>(fn)); else fn(head); }
  }
};
```

Dispatch in `PolyphaseFirDecim::step()` collapses to one call — no
separate `if constexpr` recursion needed in the decimator itself, `visit`
already generalizes "runtime index → call this operation on the right
compile-time-typed item" for *any* operation, not just `.filter()`:

```cpp
branches.visit(phase, [&](auto& branch) { outputs[phase] = branch.filter(x); });
```

Coefficient-pack slicing (`NthCoeff`/`BranchFir`/`BranchListBuilder`) is
unchanged from pass 1 — pure compile-time metaprogramming, independent of
the storage-container question. A general lazy `Zip`+`Nats` mechanism
(the "proper" general HAPI-native answer for pairing an arbitrary
sequence with its index) still doesn't exist in HAPI — confirmed by
search, zero hits anywhere in `HAPI/include`. Out of scope here:
`StaticBody`'s own head/tail/`visit` pattern already closes this
problem's actual shape without that larger investment.

---

## 3. Native correctness: bit-exact, both design passes

`native_test_generic.cpp` instantiates `PolyphaseFirDecim<Sample,Accum,2,
10,118,118,10>` and `PolyphaseFirDecim<Sample,Accum,4,1,2,3,4,4,3,2,1>` —
the exact same coefficients as the hand-written `polyphase_atoms.h`
(M=2) and `polyphase_atoms_m4.h` (M=4) — against the exact same
already-verified ground truth (M=2: impulse `0,118,10,0,0,0,0,0` / DC
`0,128,256,256,256,256`; M=4: impulse `0,4,1,0,0,0` / DC
`0,10,20,20,20,20,20,20`), both `ac_fixed` and `ap_fixed`. Passed under
both design passes — the storage/dispatch mechanism changed, the
branch-slicing math and phase logic didn't, and both stayed correct.

---

## 4. Bambu synthesis: corrected numbers (design pass 2 supersedes pass 1)

Same device/clock convention as every prior target
(`xc7a100t-1csg324-VVD`, 10ns, `ac_fixed<16,16,true>`/`ac_fixed<32,32,true>`).
**The numbers below are from the adopted `StaticBody`-style design
(§2) — an earlier report of this phase's results, based on the
discarded `At<>`-based design (§1), is superseded and should not be
relied on.**

| | Generic (pass 2) | Hand-written | Diff |
|---|---|---|---|
| M=2 flip-flops | 546 | 550 | generic **−4 (−0.7%, essentially tied)** |
| M=2 area | 1689 | 1895 | generic −206 (−10.9%) |
| M=2 max frequency | 101.904 MHz | 115.103 MHz | generic −11.5% |
| M=2 slack (10ns budget) | **0.187ns** | 1.312ns | generic **−85.8%** |
| M=4 flip-flops | 648 | 866 | generic **−218 (−25.2%)** |
| M=4 area | 1854 | 2497 | generic **−643 (−25.8%)** |
| M=4 max frequency | 101.904 MHz | 115.103 MHz | generic −11.5% |
| M=4 slack (10ns budget) | **0.187ns** | 1.312ns | generic **−85.8%** |
| DSPs (both M) | 0 | 0 | — |

Both scales synthesized cleanly (exit 0, real RTL, 0 DSPs).

**FF/area are now close to — and at M=4, meaningfully better than — the
hand-written baseline**, unlike pass 1's design (which had M=2 FF
*worse* than hand-written, +4.4%). The resource-category list also now
matches the hand-written baseline closely (`ADDRESS_DECODING_LOGIC_NN`,
`ARRAY_1D_STD_BRAM_NN`, `ARRAY_1D_STD_BRAM_NN_SP`,
`TRUE_DUAL_PORT_BYTE_ENABLING_RAM`), unlike pass 1's design (which bound
to a qualitatively different, more elaborate BRAM structure —
`STD_DP_BRAM`/`STD_NRNW_BRAM_GEN`/etc.). Likely explanation: `head`/`tail`
as plain data members, nested, produces a struct layout much closer to
the hand-written named-members struct than pass 1's recursive
*inheritance* did — Bambu's memory allocator appears to treat the two
similarly as a result.

**But timing margin is the tightest seen anywhere in this entire study —
0.187ns out of a 10ns budget, 1.9% margin — and, unlike pass 1's result,
it does not degrade further with scale.** Pass 1's design showed slack
worsening from M=2 to M=4 (1.093ns → 0.349ns, a widening problem); this
design's slack is **identical at M=2 and M=4** (0.1868ns both times, same
101.904 MHz `Fmax` both times) — a flat, fixed cost paid once by the
dispatch mechanism itself, not a compounding one. That is a materially
different, more usable finding than pass 1's: a constant ~11.5% frequency
penalty from the `visit()`-based dispatch is a real, bounded cost to
budget for, not an open-ended one that gets worse as M grows. Still
positive slack — timing is met at every M tested — but with much less
room than the hand-written version's comfortable, also-flat 1.3ns margin.

---

## 5. Where this leaves the hypothesis

Real, measured trade-off, not a clean win — but a *better-understood* one
than the first pass suggested. The corrected design gets FF/area to
parity-or-better with hand-written code at both scales tested, at the
cost of a real but **flat, non-compounding** ~11.5% timing-margin penalty
from the generic dispatch mechanism itself. That flatness is itself new
information: it suggests the cost is inherent to *having* a
runtime-indexed dispatch through `visit()` at all (paid once, at the
mechanism's first use), not to how many branches it dispatches among —
consistent with, though not proof of, the cost being genuinely bounded
rather than open-ended as M grows further.

**Update: §7 below refines this "flat ~11.5% penalty" framing** — a third
design pass isolating list-shape from dispatch-mechanism shows the
penalty is specific to pass 2's particular combination, not an inherent
property of generic dispatch in general. Read on before treating §5's
conclusion as final.

**Process lesson, independent of the resource numbers**: the first
design pass used mechanically-working-but-wrong-purpose HAPI machinery
(`At<idx,O>`) instead of the actually-matching, already-proven pattern
(`StaticBody`'s head/tail/`visit`) that existed one library over. Getting
redirected onto it produced a materially different, better result — not
just a cleaner implementation. Worth remembering the next time a new
composition problem in this family comes up: check `OneMenu`'s body
types for precedent before reaching for `std::` or building something
from scratch.

**Payoff delivered regardless of the resource trade-off**: the call-site
boilerplate problem is closed. Adding an M=8 instantiation is a
template-argument change (`PolyphaseFirDecim<Sample,Accum,8,h0,...,h7>`),
not hand-duplicating 8 branches/flags/switch-cases/sum-terms — the actual
authoring-ergonomics boundary this phase set out to close.

---

## 6. Pass 3: two follow-up questions, asked directly

Two concrete design questions, raised directly rather than left as open
threads: (1) pass 2's `visit(i,fn)` recurses by decrementing a *runtime*
`int32_t` — up to M−1 sequential compare+recurse steps for the last
branch. Would a **compile-time-indexed** accessor (`I` an NTTP, resolved
into a direct field-access chain at compile time, zero runtime branching
*inside* the access itself) avoid that cost? (2) confirmed directly:
*"recursing while size>0 => non-empty."* Pass 2's list always
instantiates `Head head; Tail tail;` even for the last real branch, whose
`Tail` is the empty `StaticList<>`-equivalent terminal — one extra
type/recursion level carrying no real data. Would restructuring so the
**base case is a single-element node** (no empty specialization anywhere
in the chain) do better?

A third question, about scope rather than mechanism: `oneMenu::StaticBody`
(the precedent pass 2 adopted) carries `printMenu`/`printBody`/`nav`/
`setStr`/`changed`/`sync` — pure menu baggage OneHLS doesn't need and
shouldn't depend on (OneHLS's declared dependencies are HAPI+OneData
only). Reading the rest of `OneMenu`'s body family
(`cArrayBody.h`, `joinBody.h`, `partitionBody.h`) confirmed
`visit(Sz i, Fn&& fn)` isn't just `StaticBody`'s own method — it's a
uniform interface contract `CArrayBody`/`CPtrArrayBody` (homogeneous-type
array bodies — the same array-vs-heterogeneous-list split this study
already found between CIC and polyphase, now confirmed as real
established precedent) and `JoinBody` all share, used generically by
`EventDispatch`'s own recursive descent. That's the right shape to keep;
the menu-specific parts are what to drop.

### `StaticList<O,OO...>`

[`static_list.h`](../.RnD/polyphase_experiment/static_list.h) — a small,
standalone analog scoped to OneHLS (not a shared HAPI/OneMenu type; a
second real OneHLS use case would be the trigger to promote it out, same
discipline this whole study has followed). Non-empty by construction —
the base case holds one real element, not an empty terminal — and
provides *both* `visit(i,fn)` (kept for interface parity with the
`OneMenu` family) and a new `getAt<I>()` (NTTP-indexed, fully
compile-time-resolved):

```cpp
template<typename O> struct StaticList<O> {           // base: single element, non-empty
  Head head{};
  template<typename Fn> void visit(int32_t, Fn&& fn) { fn(head); }
  template<size_t I> auto& getAt() { static_assert(I==0); return head; }
};
template<typename O, typename O2, typename... OO>
struct StaticList<O,O2,OO...> {                        // recursive: 2+ elements
  Head head{}; Tail tail{};
  template<typename Fn> void visit(int32_t i, Fn&& fn) {
    if (i) tail.visit(i-1, std::forward<Fn>(fn)); else fn(head);
  }
  template<size_t I> auto& getAt() {
    if constexpr (I==0) return head; else return tail.template getAt<I-1>();
  }
};
```

Two sub-passes, deliberately kept separate to isolate the two variables
cleanly rather than changing both at once and conflating them:

- **3a**: `StaticList` (non-empty) + `visit(i,fn)` dispatch, unchanged
  from pass 2's mechanism — isolates the list-shape change alone.
- **3b**: `StaticList` (non-empty) + new `getAt<I>()`/`if constexpr`
  dispatch — isolates the dispatch-mechanism change, holding the
  (already-tested-in-3a) list shape fixed.

Both native-verified bit-exact against the same ground truth as every
prior pass, both vendors — a representation/dispatch change only, not a
logic change, and both stayed correct.

### Results: five real variants now, on one table

| | Hand-written | Pass 1 (`At<>`, wrong tool) | Pass 2 (empty-terminal + `visit`) | Pass 3a (non-empty + `visit`) | Pass 3b (non-empty + `getAt<I>`) |
|---|---|---|---|---|---|
| M=2 FF | 550 | 574 | 546 | 578 | 574 |
| M=2 area | 1895 | 1035 | 1689 | **1030** | 1035 |
| M=2 Fmax | 115.103 | 112.266 | 101.904 | 105.235 | **112.266** |
| M=2 slack | **1.312** | 1.093 | 0.187 | 0.497 | 1.093 |
| M=4 FF | 866 | 834 | **648** | 712 | 834 |
| M=4 area | 2497 | 1228 | 1854 | **1222** | 1228 |
| M=4 Fmax | 115.103 | 103.612 | 101.904 | **105.859** | 103.612 |
| M=4 slack | **1.312** | 0.349 | 0.187 | **0.553** | 0.349 |
| DSPs (all) | 0 | 0 | 0 | 0 | 0 |

Pass 1's numbers here are its previously-published, recorded metrics —
not a fresh re-run. Its raw Bambu log was overwritten when pass 2 was
synthesized into the same output directory (a real process mistake,
worth naming: use a distinct output directory per design pass from the
start, which pass 3a/3b did correctly). Pass 3b's numbers match every
one of pass 1's recorded metrics exactly (FF, area, Fmax, slack, at both
M) — strong evidence, but a match against recorded values, not a
line-for-line raw-log diff the way every other comparison in this study
has been verified.

### What this actually shows — more nuanced than either single-variable guess

**Neither hypothesis was simply "confirmed."** Both were real, and the
truth is an interaction, not two independent, additive effects:

- **List shape (empty-terminal vs. non-empty) matters a lot — but only
  when paired with runtime `visit()` dispatch.** Pass 2 → 3a (same
  dispatch mechanism, list shape only): at M=4, area drops 1854→1222
  (−34%) and slack improves 0.187ns→0.553ns (+196%) for a small FF cost
  (648→712, +10%). The non-empty list is a real, substantial win here.
- **List shape has *no measurable effect* when paired with compile-time
  `getAt<I>()` dispatch.** Pass 3b's numbers match pass 1's exactly, at
  both M — despite pass 3b using the new non-empty `StaticList` and pass
  1 using the old empty-terminal inheritance-based holder. Once every
  branch access is fully resolved to a flat field-read at compile time,
  Bambu's optimizer appears to normalize away the underlying storage
  representation entirely — it only "sees" the list's shape when genuine
  *runtime* indexing (visit's decrementing recursion) is involved.
- **Dispatch mechanism (`visit` vs. `getAt<I>`) does not uniformly win —
  it flips direction with scale.** Holding list shape fixed (3a vs. 3b):
  at M=2, `getAt<I>` wins on timing (slack 1.093 vs. 0.497); at M=4,
  `visit` wins (slack 0.553 vs. 0.349). The original hypothesis —
  "compile-time access avoids a serial runtime chain, so it should just
  be faster" — was too simple: `dispatch<I>()`'s *outer* `if constexpr`
  chain still does up to M runtime comparisons, same as `visit()`'s
  recursion; the only real difference is whether the *leaf* access is a
  single flat compile-time-resolved read or a further runtime-recursive
  hop. At M=4 specifically, `getAt<I>()`'s deepest case folds a 3-hop
  `.tail.tail.tail.head` chain directly into one comparison's body — a
  larger single expression for Bambu's scheduler to place — while
  `visit()` spreads that same work across recursive calls one hop at a
  time. Which one schedules better is not obviously predictable in
  advance, and it isn't in fact monotonic here.

**Best overall generic variant found so far: pass 3a**, by a rounded
balance — smallest area at both scales (even edging out 3b/pass 1
slightly), second-best FF (behind only pass 2), and the best timing
margin of any generic variant at M=4 (0.553ns — still well short of
hand-written's flat 1.312ns, but more than 3× pass 2's 0.187ns and
notably better than pass1/3b's 0.349ns). Non-empty list + runtime
`visit()` — not the compile-time mechanism the initial hypothesis
favored.

### Revised conclusion

§5's "flat, non-compounding ~11.5% penalty" framing was accurate *for
pass 2's specific combination*, not a general property of "generic
dispatch." The real picture, now with five data points instead of two:
list representation and dispatch mechanism interact, the effect of each
depends on the other, and no single generic variant tested yet matches
hand-written's comfortable, flat 1.3ns margin — pass 3a gets closest.
Whether a further combination (e.g. `getAt<I>()` restructured so each
case's expression stays as shallow as `visit()`'s, rather than folding
the full hop-chain into one comparison) could close the remaining gap is
a real, open question — not attempted here.

---

## 7. Promotion: `StaticList` becomes a general OneHLS utility

The experiment-local `static_list.h` (§2/§6) has been promoted to
[`include/oneHLS/staticList.h`](../include/oneHLS/staticList.h) — a
real, public `oneHLS::` header, not merged into `oneHLS.h` itself (kept
opt-in, same pattern as `ac_types_support.h`/`ap_types_support.h`) and
not yet added to README's Features table (that decision — "is this a
documented, shipped Feature" — is separate from "is this real, tested
code," and hasn't been made). Trigger for promotion: a second, genuinely
independent use case — an end user composing their *own* heterogeneous
components, not just this one internal decimator need — which is exactly
the condition this whole study's own stated discipline set for promoting
anything out of `.RnD/`.

Added a factory function, `oneHLS::staticList(a,b,c,...)`, matching
`oneMenu::staticBody(a,b,c)`'s exact idiom (deduce types from
already-constructed arguments). One real bug caught and fixed while
building it, not left in: plain forwarding-reference deduction
(`OO&&...`) deduces a *reference* type when called with an lvalue — the
returned list would silently hold references instead of owned copies,
fine only if the lvalue outlives the list (as in a file-scope component)
and a real dangling-reference trap the moment a caller passes a local,
non-static variable. Fixed with `std::decay_t`, matching `std::make_tuple`'s
own established reason for doing the same — verified with a dedicated
regression test (`test/staticList_test.cpp`'s `test_lvalue_decay`:
mutate the original local *after* building the list, confirm the list's
own copy is unaffected — not just "it compiles").

**Native tests** (`test/staticList_test.cpp`): POD multi-/single-element
correctness, the lvalue-decay regression above, and the intended real
use case — two distinct `oneHLS::Fir<>` instantiations (different
coefficients, different types) composed via `staticList()`, driven
through both `visit()` and `getAt<I>()` on the same list instance,
checked against `Fir<>`'s real (Tap-delayed) impulse response. All pass.

**Bambu graduation check**: rebuilt pass 3a's `PolyphaseFirDecim` on the
promoted public header instead of the experiment's local copy
(`.RnD/polyphase_experiment/polyphase_generic_promoted.h`), synthesized
at M=2 under identical settings. Result: **578 FF / area 1030 / 105.235
MHz / 0.497ns slack — an exact match to pass 3a's own numbers.**
Confirms promotion didn't change synthesis; the two implementations are
the same design, but "same design" and "verified identical after
promotion" are different claims, and this study hasn't accepted the
former as proof of the latter anywhere else.

---

## 8. Exploring a 2D "matrix": what's already there, what isn't

A natural follow-on question: does OneHLS need a genuine 2D composition
primitive — the kind of thing `oneMenu`'s `Row<Left,Center,Right>` /
`Rows<N,method,Top,Body,Bottom>` provide (fixed 3-slot layout, nestable
into real 2D structure)?

**Storage: already solved, zero new code.** `StaticList` is generic over
its item types — nothing stops an item from itself being a `StaticList`.
Verified directly:

```cpp
auto matrix = oneHLS::staticList(
  oneHLS::staticList(A{1}, B{2}),
  oneHLS::staticList(C{3}, D{4}, E{5})   // rows may even differ in length
);
matrix.getAt<0>().getAt<0>();  // row 0, col 0
matrix.getAt<1>().getAt<2>();  // row 1, col 2
matrix.visit(1, [&](auto& row){ row.visit(1, [&](auto& cell){ /* row1,col1 */ }); });
```

Compiles and runs correctly, including *ragged* rows (different lengths
per row) — a genuine, already-working 2D (or N-D, nesting further)
heterogeneous structure with nothing built for this document beyond
`StaticList` itself.

**What `Row`/`Rows` actually contribute is NOT storage — it's screen
positioning.** Re-reading `item.h`'s `Row`/`Rows` with this question in
mind: their real content is measuring/placing text (`rowPrint`'s
pixel-offset math, `fillRect`, `padWith`, vertical anchoring within
`bottomLines`) — a `oneMenu`-display-domain concern with no OneHLS
analog (a hardware component doesn't have a screen position). Porting
`Row`/`Rows` themselves would be porting the wrong half of what they do.

**The real open question, if a matrix need ever materializes**: not
storage (solved) or display layout (not applicable), but *neighbor-aware,
2D-coordinate dataflow* — e.g. a systolic array, where PE\[r\]\[c\] needs
to read from PE\[r-1\]\[c\] and PE\[r\]\[c-1\] each cycle, not just "get
me the item at (r,c)." That's a materially different problem from
anything built in this whole study so far, and it's the one place this
exploration actually reconnects to CuTe's own original motivation
(`Layout`'s `Shape`/`Stride`, 2D coordinate-to-index mapping) rather than
`oneMenu`'s. Not attempted here — flagged as the genuinely new piece of
machinery a real future matrix use case would need, distinct from (and
harder than) what this phase built.
