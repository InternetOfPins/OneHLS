/**
 * @file staticList.h
 * @author Rui Azevedo (neu-rah) (ruihfazevedo@gmail.com)
 * @brief General-purpose, HLS-scoped heterogeneous list: build a compile-
 * time list of independently-typed components and either dispatch to one
 * by a runtime index (visit()) or access one by a compile-time index
 * (getAt<I>()). A factory function, staticList(a,b,c,...), builds one
 * from already-constructed items with types deduced -- the same idiom as
 * OneMenu's own staticBody(a,b,c) (OneMenu/include/oneMenu/menu/body/staticBody.h),
 * which this is a standalone, menu-independent analog of (stripped of
 * every menu-specific member -- printMenu/printBody/nav/setStr/changed/
 * sync -- OneHLS's declared dependencies are HAPI+OneData only).
 *
 * Grew out of the polyphase FIR decimator experiment
 * (.RnD/polyphase_experiment/, see docs/PHASE9_GENERIC_POLYPHASE.md),
 * where a component needing M independently-typed sub-components (a
 * polyphase decimator's branches -- distinct Fir<> instantiations, one
 * per coefficient subset) needed exactly this shape and OneHLS had
 * nothing for it. Promoted here because a second, independent use case
 * showed up: composing an arbitrary heterogeneous set of a caller's OWN
 * components (not just this one internal need) is a real, general OneHLS
 * requirement, not a one-off.
 *
 * Two differences from oneMenu::StaticBody, both deliberate:
 *
 * 1. Non-empty by construction: the base case (StaticList<O>) holds one
 *    real element (Head head;), never an empty terminal. StaticBody<>
 *    (the empty specialization) always exists as SOME node's Tail even
 *    for the very last real item; this type never instantiates a
 *    zero-element node at all. Confirmed to matter under real Bambu HLS
 *    synthesis, not just a style preference: PHASE9_GENERIC_POLYPHASE.md
 *    §6 found removing the empty terminal cut area ~34% and roughly
 *    tripled timing margin, when paired with visit()-style dispatch (no
 *    measurable effect paired with compile-time getAt<I>() dispatch --
 *    the two interact, see that doc for the full comparison).
 * 2. Provides BOTH visit(i,fn) (runtime i, kept for interface parity
 *    with OneMenu's own body family -- StaticBody/CArrayBody/
 *    CPtrArrayBody/JoinBody all share this exact signature) and
 *    getAt<I>() (NTTP i, resolved into a direct chain of .tail accesses
 *    entirely at compile time). Same PHASE9 doc found neither dispatch
 *    style dominates uniformly -- which one synthesizes better is
 *    scale-dependent, not a fixed choice -- so both are exposed rather
 *    than picking one.
 *
 * Requires at least one element -- unlike staticBody(), there is no
 * zero-argument staticList(): a StaticList<> (empty) has no meaningful
 * definition under design (1) above.
 */
#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>
#include <type_traits>

namespace oneHLS {

  template<typename... OO> struct StaticList;

  /// @brief Base case: a single element. Never empty -- see file header.
  template<typename O>
  struct StaticList<O> {
    using Head = O;
    Head head{};

    constexpr StaticList() {}
    template<typename I>
    constexpr StaticList(I&& o) : head{std::forward<I>(o)} {}

    template<typename Fn>
    void visit(int32_t /*i*/, Fn&& fn) { fn(head); }  // i must be 0 for a valid call

    template<size_t I>
    auto& getAt() {
      static_assert(I == 0, "StaticList::getAt index out of range");
      return head;
    }
  };

  /// @brief Recursive case: 2+ elements.
  template<typename O, typename O2, typename... OO>
  struct StaticList<O,O2,OO...> {
    using Head = O;
    using Tail = StaticList<O2,OO...>;  // itself non-empty, down to its own base case
    Head head{};
    Tail tail{};

    constexpr StaticList() {}
    template<typename I, typename... II>
    constexpr StaticList(I&& o, II&&... oo)
      : head{std::forward<I>(o)}, tail{std::forward<II>(oo)...} {}

    template<typename Fn>
    void visit(int32_t i, Fn&& fn) {
      if (i) tail.visit(i-1, std::forward<Fn>(fn));
      else   fn(head);
    }

    template<size_t I>
    auto& getAt() {
      if constexpr (I == 0) return head;
      else                  return tail.template getAt<I-1>();
    }
  };

  /// @brief Factory: staticList(a,b,c) deduces StaticList<TypeOf<a>,TypeOf<b>,TypeOf<c>>
  /// from already-constructed items -- same idiom as oneMenu::staticBody(a,b,c).
  /// std::decay_t, deliberately NOT plain forwarding-reference deduction
  /// (OO&&...'s own deduced type): called with an lvalue, forwarding-
  /// reference rules deduce a REFERENCE type, which would make the
  /// returned StaticList hold references instead of owned copies --
  /// silently correct if the lvalue outlives the list (e.g. a file-scope
  /// component, as in test/staticList_test.cpp), but a real dangling-
  /// reference trap the moment a caller passes a local, non-static
  /// variable. decay_t forces value semantics unconditionally, matching
  /// std::make_tuple's own well-established reason for doing the same.
  template<typename... OO>
  constexpr StaticList<std::decay_t<OO>...> staticList(OO&&... oo) {
    return StaticList<std::decay_t<OO>...>{std::forward<OO>(oo)...};
  }

}
