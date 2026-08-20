/**
 * @file oneHLS.h
 * @author Rui Azevedo (neu-rah) (ruihfazevedo@gmail.com)
 * @brief HAPI/OneData-based HLS-synthesizable DSP and control components.
 *
 * Type-agnostic: Sample/Accum are caller-chosen (HLSLibs ac_fixed, Xilinx
 * ap_fixed, or a plain arithmetic type) -- the composition below never
 * names a vendor type directly. The one place a vendor's API genuinely
 * differs (constructing a coefficient from a raw, pre-scaled bit pattern,
 * to avoid the static-const-double-ctor lazy-init-guard trap found under
 * Bambu HLS -- see acTypesHLS/HANDOFF.md) is isolated behind RawBitsCtor,
 * specialized per vendor in ac_types_support.h / ap_types_support.h
 * (included separately, opt-in, alongside the vendor's own headers).
 */
#pragma once
#include <hapi/hapi.h>
#include <oneData/oneData.h>
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {
  using namespace hapi;
  using oneData::Data;

  /// @brief constructs a Sample from a raw, pre-scaled bit pattern (e.g.
  /// Q8.8 128 -> the real value 0.5), not a value-converting constructor.
  /// Default assumes T(RawBits) is the right construction (true for
  /// plain arithmetic types, where "raw bits" and "value" coincide) --
  /// vendor fixed-point types need their own specialization.
  template<typename T>
  struct RawBitsCtor {
    template<int32_t RawBits>
    static constexpr T make() { return T(RawBits); }
  };

  template<typename T, int32_t RawBits>
  static inline T rawCoeff() { return RawBitsCtor<T>::template make<RawBits>(); }

  /// @brief shared terminal: every method any component below might
  /// reach is a harmless default here. Unused template methods are never
  /// instantiated, so an unused method here costs nothing.
  template<typename Sample, typename Accum>
  struct Terminal {
    static Accum mac(Sample, Accum acc) { return acc; }
    static Accum fbSum(Accum acc) { return acc; }
    static void  fbPush(Sample) {}
    static Accum integrate(Sample) { return Accum(0); }
    static Accum differentiate(Sample) { return Accum(0); }
  };

  // ── FIR ──────────────────────────────────────────────────────────────
  // Feedforward tap: reads and updates its own delay in one pass,
  // forwards the running sum.
  template<typename Sample, typename Accum, int32_t RawBits>
  struct TapLogic {
    template<typename I>
    struct Part : I {
      using Base = I;
      using Base::Base;
      Accum mac(Sample x, Accum acc) {
        Accum sum   = acc + Accum(rawCoeff<Sample,RawBits>()) * Accum(Base::get());
        Sample prev = Base::get();
        Base::set(x);
        return I::mac(prev, sum);
      }
    };
  };
  template<typename Sample, typename Accum, int32_t RawBits>
  using Tap = Chain<TapLogic<Sample,Accum,RawBits>, Data<Sample>>;

  template<typename Sample, typename Accum, int32_t... Coeffs>
  struct Fir : APIOf<Terminal<Sample,Accum>, Chain<Tap<Sample,Accum,Coeffs>...>> {
    using Base = APIOf<Terminal<Sample,Accum>, Chain<Tap<Sample,Accum,Coeffs>...>>;
    using Base::Base;
    Accum filter(Sample x) { return Base::mac(x, Accum(0)); }
  };

  // ── IIR (direct form I biquad) ──────────────────────────────────────
  // Feedback tap: contributes via fbSum() using its OLD delayed output;
  // its delay only shifts via fbPush(), once the final output is known.
  template<typename Sample, typename Accum, int32_t RawBits>
  struct FBTapLogic {
    template<typename I>
    struct Part : I {
      using Base = I;
      using Base::Base;
      Accum fbSum(Accum acc) {
        return I::fbSum(acc + Accum(rawCoeff<Sample,RawBits>()) * Accum(Base::get()));
      }
      void fbPush(Sample newVal) {
        Sample prev = Base::get();
        Base::set(newVal);
        I::fbPush(prev);
      }
    };
  };
  template<typename Sample, typename Accum, int32_t RawBits>
  using FBTap = Chain<FBTapLogic<Sample,Accum,RawBits>, Data<Sample>>;

  // y[n] = b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]; FBA1/FBA2 are
  // -a1/-a2 (the sign the accumulate step actually uses), not a1/a2.
  template<typename Sample, typename Accum, int32_t B1, int32_t B2, int32_t FBA1, int32_t FBA2>
  struct Biquad : APIOf<Terminal<Sample,Accum>,
                         Chain<Tap<Sample,Accum,B1>, Tap<Sample,Accum,B2>,
                               FBTap<Sample,Accum,FBA1>, FBTap<Sample,Accum,FBA2>>> {
    using Base = APIOf<Terminal<Sample,Accum>,
                        Chain<Tap<Sample,Accum,B1>, Tap<Sample,Accum,B2>,
                              FBTap<Sample,Accum,FBA1>, FBTap<Sample,Accum,FBA2>>>;
    using Base::Base;
    Sample step(Sample x) {
      Accum ff = Base::mac(x, Accum(0));
      Accum fb = Base::fbSum(Accum(0));
      Sample y = Accum(ff + fb);  // narrow Accum -> Sample, quantized correctly
      Base::fbPush(y);
      return y;
    }
  };

  // ── PID ──────────────────────────────────────────────────────────────
  // Integral term: accumulates error every call, returns the Ki-scaled
  // sum. Derivative term: same read-old/update-new shape as a FIR tap's
  // delay, returns the Kd-scaled delta between this call and the last.
  template<typename Sample, typename Accum, int32_t KiRawBits>
  struct IntegLogic {
    template<typename I>
    struct Part : I {
      using Base = I;
      using Base::Base;
      Accum integrate(Sample e) {
        Base::set(Accum(Base::get()) + Accum(e));
        return Accum(rawCoeff<Sample,KiRawBits>()) * Base::get();
      }
    };
  };
  template<typename Sample, typename Accum, int32_t KiRawBits>
  using Integrator = Chain<IntegLogic<Sample,Accum,KiRawBits>, Data<Accum>>;

  template<typename Sample, typename Accum, int32_t KdRawBits>
  struct DerivLogic {
    template<typename I>
    struct Part : I {
      using Base = I;
      using Base::Base;
      Accum differentiate(Sample e) {
        Sample prev = Base::get();
        Base::set(e);
        return Accum(rawCoeff<Sample,KdRawBits>()) * Accum(e - prev);
      }
    };
  };
  template<typename Sample, typename Accum, int32_t KdRawBits>
  using Differentiator = Chain<DerivLogic<Sample,Accum,KdRawBits>, Data<Sample>>;

  // u[n] = Kp*e[n] + Ki*sum(e) + Kd*(e[n]-e[n-1]).
  template<typename Sample, typename Accum, int32_t KpRawBits, int32_t KiRawBits, int32_t KdRawBits>
  struct Pid : APIOf<Terminal<Sample,Accum>,
                      Chain<Integrator<Sample,Accum,KiRawBits>, Differentiator<Sample,Accum,KdRawBits>>> {
    using Base = APIOf<Terminal<Sample,Accum>,
                        Chain<Integrator<Sample,Accum,KiRawBits>, Differentiator<Sample,Accum,KdRawBits>>>;
    using Base::Base;
    Accum step(Sample e) {
      Accum p = Accum(rawCoeff<Sample,KpRawBits>()) * Accum(e);
      Accum i = Base::integrate(e);
      Accum d = Base::differentiate(e);
      return p + i + d;
    }
  };

  // ── Accumulator ──────────────────────────────────────────────────────
  // Plain running sum: Accum(x) added to the stored total every step. No
  // separate mac()/Terminal routing needed -- a single component, no
  // chain to fall through past -- so this closes over hapi::Nil directly.
  // Instantiate with Sample==Accum (e.g. a narrow ac_int) to get 2's-
  // complement wraparound instead of headroom; this is the same
  // accumulate-and-return-the-running-total shape Pid's Integrator uses
  // internally, exposed here as its own reusable component.
  template<typename Sample, typename Accum>
  struct AccumulateLogic {
    template<typename I>
    struct Part : I {
      using Base = I;
      using Base::Base;
      Accum accumulate(Sample x) {
        Base::set(Base::get() + Accum(x));
        return Base::get();
      }
    };
  };
  template<typename Sample, typename Accum>
  using AccumulatorChain = Chain<AccumulateLogic<Sample,Accum>, Data<Accum>>;

  template<typename Sample, typename Accum>
  struct Accumulator : APIOf<Nil, AccumulatorChain<Sample,Accum>> {
    using Base = APIOf<Nil, AccumulatorChain<Sample,Accum>>;
    using Base::Base;
    Accum step(Sample x) { return Base::accumulate(x); }
  };

  // ── ComplexMac ───────────────────────────────────────────────────────
  // Complex<T> is OneHLS's own plain struct-of-two-fields -- deliberately
  // NOT a vendor complex type (ac_complex<T> is HLSLibs-only, ap_types has
  // no equivalent), so ComplexMac stays exactly as vendor-agnostic as
  // every other component here. Both re/im arithmetic go through T's own
  // +/-/* operators, which ac_fixed and ap_fixed both provide natively.
  //
  // Resource note: at wide enough Sample/Accum (e.g. 32-bit complex, 64
  // raw bits total) this is the one component in this library observed
  // to bind its Data<Complex<Accum>> state to a real BRAM primitive under
  // Bambu instead of the lightweight distributed RAM every other
  // component gets -- correct, reproducible, just NOT zero-cost the way
  // Fir/Biquad/Pid/Accumulator are. See README's verified-results table.
  template<typename T>
  struct Complex {
    T re{};
    T im{};
  };
  template<typename T>
  constexpr Complex<T> operator+(Complex<T> a, Complex<T> b) {
    return Complex<T>{ T(a.re + b.re), T(a.im + b.im) };
  }
  template<typename T>
  constexpr Complex<T> operator*(Complex<T> a, Complex<T> b) {
    return Complex<T>{ T(a.re*b.re - a.im*b.im), T(a.re*b.im + a.im*b.re) };
  }

  template<typename Sample, typename Accum, int32_t CoeffReRawBits, int32_t CoeffImRawBits>
  struct ComplexMacLogic {
    template<typename I>
    struct Part : I {
      using Base = I;
      using Base::Base;
      Complex<Accum> cmac(Complex<Sample> x) {
        Complex<Accum> coeff{ Accum(rawCoeff<Sample,CoeffReRawBits>()), Accum(rawCoeff<Sample,CoeffImRawBits>()) };
        Complex<Accum> xw{ Accum(x.re), Accum(x.im) };
        Base::set(Base::get() + xw * coeff);
        return Base::get();
      }
    };
  };
  template<typename Sample, typename Accum, int32_t CoeffReRawBits, int32_t CoeffImRawBits>
  using ComplexMacChain = Chain<ComplexMacLogic<Sample,Accum,CoeffReRawBits,CoeffImRawBits>, Data<Complex<Accum>>>;

  template<typename Sample, typename Accum, int32_t CoeffReRawBits, int32_t CoeffImRawBits>
  struct ComplexMac : APIOf<Nil, ComplexMacChain<Sample,Accum,CoeffReRawBits,CoeffImRawBits>> {
    using Base = APIOf<Nil, ComplexMacChain<Sample,Accum,CoeffReRawBits,CoeffImRawBits>>;
    using Base::Base;
    Complex<Accum> step(Complex<Sample> x) { return Base::cmac(x); }
  };
}
