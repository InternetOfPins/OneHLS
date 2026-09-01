/**
 * @file upsample2d.h
 * @brief Round 12 — spatial upsampling by S: the rate-INCREASING mirror of
 * Stride. Unlike everything else in the set it holds a value across
 * multiple output cycles and (nearest mode) replays a row.
 *
 * Gate first (HANDOFF R12): does it fail the "cheap" half of the
 * static-depth claim the way Transpose2D did?
 *
 *   Upsample2D<T,W,C,S, NEAREST>:  each input pixel -> an SxS block of
 *     copies. Needs the current input row buffered (W*C) to replay it for
 *     the S-1 duplicate output rows. One row -- small, static.
 *   Upsample2D<T,W,C,S, ZERO>:     insert S-1 zero rows/cols between real
 *     pixels (transposed-conv prep). Emits the input value immediately at
 *     the S-multiple positions, 0 elsewhere. NO buffer.
 *
 * Driven by the OUTPUT (upsampled) raster: step() is called S*S*W*H times;
 * `wantInput` says which of those cycles consume a real input pixel.
 */
#pragma once
#ifdef __AVR__
  #include <stdint.h>
#else
  #include <cstdint>
#endif

namespace oneHLS {

  enum class UpMode { Nearest, Zero };

  template<typename T, int32_t W, int32_t C, int32_t S, UpMode Mode>
  struct Upsample2D {
    static_assert(S >= 1, "S >= 1");
    static constexpr int32_t Wout   = W * S;
    static constexpr int32_t RowBuf = (Mode == UpMode::Nearest) ? W * C : 1;

    T       row[RowBuf]{};        // Nearest: current input row, for replay
    int32_t ocol{0}, orow{0};     // output raster cursor
    int32_t rbw{0};               // row-buffer write cursor

    /// out[C]. wantInput=true on cycles that consume in[C] (caller advances
    /// its source); on other cycles in[] is ignored.
    void step(const T in[C], T out[C], bool& wantInput) {
      int32_t icol = ocol / S;
      int32_t phaseCol = ocol - icol * S;
      int32_t phaseRow = orow % S;

      if (Mode == UpMode::Zero) {
        bool real = (phaseCol == 0) & (phaseRow == 0);
        wantInput = real;
        for (int32_t c = 0; c < C; ++c) out[c] = real ? in[c] : T(0);
      } else { // Nearest
        bool firstRowOfBlock = (phaseRow == 0);
        bool consume = firstRowOfBlock & (phaseCol == 0);
        wantInput = consume;
        if (consume) {
          for (int32_t c = 0; c < C; ++c) { out[c] = in[c]; row[icol * C + c] = in[c]; }
        } else {
          for (int32_t c = 0; c < C; ++c) out[c] = row[icol * C + c];
        }
      }

      if (++ocol == Wout) { ocol = 0; ++orow; }
    }
  };

}
