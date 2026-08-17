/**
 * @file polyphase_fir.h
 * @brief Type-1 polyphase FIR decimator: PolyphaseFirDecim<Sample,Accum,
 * M,Coeffs...> decomposes a flat K-tap coefficient list into M
 * independently-typed oneHLS::Fir<> branches (different coefficients ->
 * different C++ types under this library's NTTP-coefficient convention
 * -- a plain array literally cannot hold them) at compile time, and
 * dispatches each incoming sample to the right branch by a runtime
 * commutator.
 *
 * The M branches are held in oneHLS::StaticList<> (include/oneHLS/
 * staticList.h) -- this library's general-purpose heterogeneous-list
 * utility, promoted out of this exact research (see
 * ../../../docs/PHASE9_GENERIC_POLYPHASE.md). Shared by src/main.cpp
 * (native demo) and hls/polyphase_fir_top.cpp (Bambu synthesis target)
 * so both compile the identical definition.
 *
 * Not yet promoted to include/oneHLS/oneHLS.h itself -- StaticList is
 * the general utility; PolyphaseFirDecim is one real, working use of it,
 * demonstrated here, not (yet) a second shipped Feature.
 */
#pragma once
#include <oneHLS/oneHLS.h>
#include <oneHLS/staticList.h>
#include <cstdint>
#include <utility>
#include <array>

namespace polyphase_example {
  using namespace oneHLS;

  // Compile-time coefficient-pack slicing: e_i[n] = h[i + n*M], the
  // standard Type-1 polyphase decomposition.
  template<size_t N, int32_t First, int32_t... Rest>
  struct NthCoeff { static constexpr int32_t value = NthCoeff<N-1,Rest...>::value; };
  template<int32_t First, int32_t... Rest>
  struct NthCoeff<0,First,Rest...> { static constexpr int32_t value = First; };

  template<typename Sample, typename Accum, int32_t M, int32_t Branch, typename KSeq, int32_t... Coeffs>
  struct BranchFir;
  template<typename Sample, typename Accum, int32_t M, int32_t Branch, size_t... KIs, int32_t... Coeffs>
  struct BranchFir<Sample,Accum,M,Branch,std::index_sequence<KIs...>,Coeffs...> {
    using Type = Fir<Sample,Accum, NthCoeff<(size_t)Branch + KIs*(size_t)M, Coeffs...>::value...>;
  };

  template<typename Sample, typename Accum, int32_t M, typename MSeq, int32_t... Coeffs>
  struct BranchListBuilder;
  template<typename Sample, typename Accum, int32_t M, size_t... MIs, int32_t... Coeffs>
  struct BranchListBuilder<Sample,Accum,M,std::index_sequence<MIs...>,Coeffs...> {
    using Type = oneHLS::StaticList<
      typename BranchFir<Sample,Accum,M,(int32_t)MIs,
                          std::make_index_sequence<sizeof...(Coeffs)/M>, Coeffs...>::Type...>;
  };

  // Commutator: sample n=0 -> branch M-1, cyclic +1 each call (verified
  // by brute-force search against the real, Tap-delayed Fir<> reference
  // for M=2,3,4 -- see PHASE9_GENERIC_POLYPHASE.md and
  // PHASE4_POLYPHASE_EXPERIMENT.md for the derivation). bool& valid:
  // call every HIGH-rate cycle, valid==true exactly 1-in-M calls.
  template<typename Sample, typename Accum, int32_t M, int32_t... Coeffs>
  struct PolyphaseFirDecim {
    static_assert(M >= 1, "M must be >= 1");
    static_assert(sizeof...(Coeffs) % M == 0, "coefficient count must be divisible by M");

    using Branches = typename BranchListBuilder<Sample,Accum,M,std::make_index_sequence<M>,Coeffs...>::Type;
    Branches branches{};
    int32_t phase{M-1};
    int32_t receivedCount{0};
    std::array<Accum,M> outputs{};
    Accum lastValid{0};

    Accum step(Sample x, bool& valid) {
      int32_t p = phase;
      branches.visit(p, [&](auto& branch) { outputs[p] = branch.filter(x); });
      phase = (phase + 1) % M;
      valid = false;
      if (++receivedCount == M) {
        receivedCount = 0;
        Accum sum(0);
        for (int32_t i = 0; i < M; ++i) sum = Accum(sum + outputs[i]);
        lastValid = sum;
        valid = true;
      }
      return lastValid;
    }
  };
}
