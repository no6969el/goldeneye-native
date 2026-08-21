#include "acmd_interp.h"

#include <algorithm>
#include <cstring>

namespace ge_audio {
namespace {

inline int16_t clampS16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return int16_t(v);
}

// DMEM holds s16 samples. Reading and writing them through helpers keeps the
// byte-offset-vs-sample-index confusion in one place: every DMEM address in the
// ABI is a BYTE offset, but the data is 16-bit.
inline int16_t dmemRead16(const uint8_t* dmem, uint32_t byte_off) {
    int16_t v;
    std::memcpy(&v, dmem + byte_off, 2);
    return v;
}
inline void dmemWrite16(uint8_t* dmem, uint32_t byte_off, int16_t v) {
    std::memcpy(dmem + byte_off, &v, 2);
}

}  // namespace

Interpreter::Interpreter(RdramAccess access)
    : access_(std::move(access)), dmem_(kAudioDmemSize, 0) {
    reset();
}

void Interpreter::reset() {
    std::fill(dmem_.begin(), dmem_.end(), 0);
    buff_in_ = buff_out_ = buff_count_ = 0;
    vol_[0] = vol_[1] = 0;
    vol_target_[0] = vol_target_[1] = 0;
    vol_rate_[0] = vol_rate_[1] = 0;
    vol_dry_ = vol_wet_ = 0;
    std::memset(segment_, 0, sizeof(segment_));
    adpcm_loop_addr_ = 0;
    std::memset(codebook_, 0, sizeof(codebook_));
    codebook_predictors_ = 0;
    stats_ = Stats{};
}

uint32_t Interpreter::resolve(uint32_t addr) const {
    // Same rule as the graphics segment table.
    const uint32_t seg = (addr >> 24) & 0xF;
    return segment_[seg] + (addr & 0x00FFFFFF);
}

bool Interpreter::dmemRangeOk(uint32_t off, uint32_t bytes) const {
    return uint64_t(off) + bytes <= uint64_t(kAudioDmemSize);
}

bool Interpreter::run(const Acmd* cmds, size_t count) {
    if (!cmds) return false;
    bool ok = true;

    for (size_t i = 0; i < count; ++i) {
        const uint32_t w0 = cmds[i].w0;
        const uint32_t w1 = cmds[i].w1;
        const uint8_t op = uint8_t(w0 >> 24);
        ++stats_.commands;
        if (op < 16) ++stats_.opcode_hist[op];

        switch (op) {
            case A_SPNOOP:                              break;
            case A_CLEARBUFF:  cmdClearBuff(w0, w1);    break;
            case A_SETBUFF:    cmdSetBuff(w0, w1);      break;
            case A_DMEMMOVE:   cmdDmemMove(w0, w1);     break;
            case A_MIXER:      cmdMixer(w0, w1);        break;
            case A_INTERLEAVE: cmdInterleave(w0, w1);   break;
            case A_SETVOL:     cmdSetVol(w0, w1);       break;
            case A_SEGMENT:    cmdSegment(w0, w1);      break;
            case A_SETLOOP:    cmdSetLoop(w0, w1);      break;
            case A_LOADBUFF:   ok &= cmdLoadBuff(w0, w1);   break;
            case A_SAVEBUFF:   ok &= cmdSaveBuff(w0, w1);   break;
            case A_LOADADPCM:  ok &= cmdLoadAdpcm(w0, w1);  break;
            case A_ADPCM:      ok &= cmdAdpcm(w0, w1);      break;
            case A_RESAMPLE:   ok &= cmdResample(w0, w1);   break;
            case A_ENVMIXER:   ok &= cmdEnvMixer(w0, w1);   break;
            case A_POLEF:      ok &= cmdPoleF(w0, w1);      break;
            default:
                ++stats_.unknown;
                break;
        }
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Unambiguous commands. These have exact, well-specified semantics and the
// tests check them directly.
// ---------------------------------------------------------------------------

void Interpreter::cmdClearBuff(uint32_t w0, uint32_t w1) {
    const uint32_t off = w0 & 0xFFFF;
    const uint32_t n = w1 & 0xFFFF;
    if (!dmemRangeOk(off, n)) return;
    std::memset(dmem_.data() + off, 0, n);
}

void Interpreter::cmdSetBuff(uint32_t w0, uint32_t w1) {
    // Loads the in/out/count registers that ADPCM, RESAMPLE, ENVMIXER and POLEF
    // consume. They persist across commands — a list that looks incomplete in
    // isolation is usually relying on a SETBUFF several commands earlier.
    buff_in_ = uint16_t(w0 & 0xFFFF);
    buff_out_ = uint16_t((w1 >> 16) & 0xFFFF);
    buff_count_ = uint16_t(w1 & 0xFFFF);
}

void Interpreter::cmdDmemMove(uint32_t w0, uint32_t w1) {
    const uint32_t src = w0 & 0xFFFF;
    const uint32_t dst = (w1 >> 16) & 0xFFFF;
    const uint32_t n = w1 & 0xFFFF;
    if (!dmemRangeOk(src, n) || !dmemRangeOk(dst, n)) return;
    // memmove, not memcpy: the microcode permits overlapping regions and the
    // audio library does use them.
    std::memmove(dmem_.data() + dst, dmem_.data() + src, n);
}

void Interpreter::cmdMixer(uint32_t w0, uint32_t w1) {
    // Amixer: cmd:8 flags:8 gain:16 | dmemi:16 dmemo:16
    const int32_t g = int16_t(w0 & 0xFFFF);
    const uint32_t dmemi = (w1 >> 16) & 0xFFFF;
    const uint32_t dmemo = w1 & 0xFFFF;
    const uint32_t count = buff_count_;
    if (!dmemRangeOk(dmemi, count) || !dmemRangeOk(dmemo, count)) return;

    for (uint32_t i = 0; i + 1 < count; i += 2) {
        const int32_t s = dmemRead16(dmem_.data(), dmemi + i);
        const int32_t d = dmemRead16(dmem_.data(), dmemo + i);
        // Q15 multiply-accumulate: gain is a signed 1.15 fraction.
        dmemWrite16(dmem_.data(), dmemo + i, clampS16(d + ((s * g) >> 15)));
    }
}

void Interpreter::cmdInterleave(uint32_t w0, uint32_t w1) {
    // Ainterleave: cmd:8 pad:24 | inL:16 inR:16.
    // Output goes to the SETBUFF-configured out address. This is the last step
    // of the frame: two mono buffers become the stereo pair the AI plays.
    (void)w0;
    const uint32_t inL = (w1 >> 16) & 0xFFFF;
    const uint32_t inR = w1 & 0xFFFF;
    const uint32_t out = buff_out_;
    const uint32_t count = buff_count_;  // bytes per channel

    if (!dmemRangeOk(inL, count) || !dmemRangeOk(inR, count) ||
        !dmemRangeOk(out, count * 2))
        return;

    for (uint32_t i = 0; i + 1 < count; i += 2) {
        dmemWrite16(dmem_.data(), out + i * 2 + 0, dmemRead16(dmem_.data(), inL + i));
        dmemWrite16(dmem_.data(), out + i * 2 + 2, dmemRead16(dmem_.data(), inR + i));
    }
}

void Interpreter::cmdSetVol(uint32_t w0, uint32_t w1) {
    // Overloaded by flags — this is the command most often gotten wrong:
    //   A_VOL set  : load dry/wet levels
    //   A_VOL clear: load a channel volume, its ramp target and ramp rate,
    //                selected left/right by A_LEFT.
    const uint8_t flags = uint8_t((w0 >> 16) & 0xFF);
    const int16_t a = int16_t(w0 & 0xFFFF);
    const int16_t b = int16_t((w1 >> 16) & 0xFFFF);
    const int16_t c = int16_t(w1 & 0xFFFF);

    if (flags & A_VOL) {
        vol_dry_ = a;
        vol_wet_ = c;
    } else {
        const int ch = (flags & A_LEFT) ? 0 : 1;
        vol_[ch] = a;
        vol_target_[ch] = b;
        vol_rate_[ch] = c;
    }
}

void Interpreter::cmdSegment(uint32_t w0, uint32_t w1) {
    (void)w0;
    const uint32_t num = (w1 >> 24) & 0xF;
    segment_[num] = w1 & 0x00FFFFFF;
}

void Interpreter::cmdSetLoop(uint32_t w0, uint32_t w1) {
    (void)w0;
    adpcm_loop_addr_ = resolve(w1);
}

bool Interpreter::cmdLoadBuff(uint32_t w0, uint32_t w1) {
    (void)w0;
    const uint32_t phys = resolve(w1);
    const uint32_t n = buff_count_;
    if (!dmemRangeOk(buff_in_, n)) return false;
    const void* src = access_(phys, n, false);
    if (!src) return false;
    std::memcpy(dmem_.data() + buff_in_, src, n);
    return true;
}

bool Interpreter::cmdSaveBuff(uint32_t w0, uint32_t w1) {
    (void)w0;
    const uint32_t phys = resolve(w1);
    const uint32_t n = buff_count_;
    if (!dmemRangeOk(buff_out_, n)) return false;
    void* dst = access_(phys, n, true);
    if (!dst) return false;
    std::memcpy(dst, dmem_.data() + buff_out_, n);
    return true;
}

bool Interpreter::cmdLoadAdpcm(uint32_t w0, uint32_t w1) {
    // count is in BYTES of codebook data. Each predictor set is
    // order * 8 * sizeof(s16) = 32 bytes.
    const uint32_t bytes = w0 & 0xFFFF;
    const uint32_t phys = resolve(w1);
    const uint32_t clamped = std::min<uint32_t>(bytes, sizeof(codebook_));

    const void* src = access_(phys, clamped, false);
    if (!src) return false;
    std::memcpy(codebook_, src, clamped);
    codebook_predictors_ = int(clamped / (kAdpcmOrder * 8 * sizeof(int16_t)));
    return true;
}

// ---------------------------------------------------------------------------
// VADPCM
//
// 9-byte frames -> 16 samples. Header byte: high nibble is a left-shift applied
// to each 4-bit residual, low nibble selects a predictor set from the codebook.
//
// The predictor is order-2 over the two samples preceding the frame, plus a
// contribution from earlier residuals within the same 8-sample half. The
// accumulator runs in Q11 and is clamped to s16 on output.
//
// *** STATUS: KNOWN INCORRECT. DO NOT SHIP. ***
//
// This was previously marked "unvalidated". It is now known to be wrong, which
// is a real downgrade and worth stating plainly.
//
// tools/extract_adpcm_vectors.py pulls a bit-exact oracle out of the instrument
// bank: ALADPCMloop::state holds the sixteen decoded samples immediately before
// a sample's loop point, written by Nintendo's own encoder. Decode up to that
// point and the last sixteen samples must match. Run:
//
//     python3 tools/extract_adpcm_vectors.py <decomp> /tmp/vec.bin
//     ./build/audio_validate /tmp/vec.bin
//
// Result: 0 of 4 vectors match, and the expected sequence does not appear
// ANYWHERE in the decoded stream under 56+ algorithm variants.
//
// What the investigation DID establish (so nobody repeats it):
//
//   * The oracle is sound. Loop structs verify: start < end <= sample count,
//     count == 0xFFFFFFFF (infinite), and the selected cases are frame-aligned.
//   * The input data is real VADPCM. Shift nibbles cluster in 7..12 and
//     predictor indices are always < npredictors across every frame.
//   * The codebook is NOT transformed at load. src/libultra/audio/load.c:380
//     computes bookSize = 2*order*npredictors*ADPCMVSIZE and hands
//     book->book straight to aLoadADPCM, so the microcode sees the file layout.
//   * One real bug was found and fixed here: the inner convolution originally
//     ran over the decoded outputs instead of the residuals, which made the
//     recursion unstable — output stayed smooth (so it looked plausible) and
//     then ran away to saturation. That fix is retained; it is necessary but
//     not sufficient.
//   * Ruled out by exhaustive search: history order, book1/book2 assignment,
//     convolution index direction and reversal, nibble order, accumulator shift
//     (10..16), per-predictor codebook layout.
//
// So the error is in the decode arithmetic, in a dimension not yet searched.
// Everything downstream of it — chunking, state carry-through RDRAM, the
// command plumbing — is exercised by audio_validate and behaves correctly.
// ---------------------------------------------------------------------------

bool Interpreter::cmdAdpcm(uint32_t w0, uint32_t w1) {
    const uint8_t flags = uint8_t((w0 >> 16) & 0xFF);
    const uint32_t state_addr = resolve(w1);
    const uint32_t out_bytes = buff_count_;

    if (!dmemRangeOk(buff_in_, out_bytes) || !dmemRangeOk(buff_out_, out_bytes))
        return false;

    // Decoder state: the last two samples, carried across commands via RDRAM.
    int16_t hist[kAdpcmOrder] = {0, 0};
    if (!(flags & A_INIT)) {
        // A_LOOP restores from the loop point set by A_SETLOOP rather than from
        // the running state — that is how a looping instrument resumes without
        // a discontinuity.
        const uint32_t from = (flags & A_LOOP) ? adpcm_loop_addr_ : state_addr;
        if (const void* s = access_(from, sizeof(hist), false))
            std::memcpy(hist, s, sizeof(hist));
    }

    const uint32_t frames = out_bytes / (kAdpcmSamplesPerFrame * 2);
    uint32_t in_off = buff_in_;
    uint32_t out_off = buff_out_;

    for (uint32_t f = 0; f < frames; ++f) {
        if (!dmemRangeOk(in_off, kAdpcmFrameBytes)) break;

        const uint8_t header = dmem_[in_off];
        const int shift = header >> 4;
        const int pred = header & 0xF;
        const int p = (codebook_predictors_ > 0) ? (pred % codebook_predictors_) : 0;
        const int16_t* book1 = &codebook_[p * 16];
        const int16_t* book2 = &codebook_[p * 16 + 8];

        // Unpack 16 signed 4-bit residuals.
        int32_t residual[kAdpcmSamplesPerFrame];
        for (int i = 0; i < kAdpcmSamplesPerFrame; ++i) {
            const uint8_t byte = dmem_[in_off + 1 + i / 2];
            int nib = (i & 1) ? (byte & 0x0F) : (byte >> 4);
            if (nib > 7) nib -= 16;             // sign-extend the nibble
            residual[i] = int32_t(nib) << shift;
        }

        // Two independent 8-sample halves, each seeded from the running history.
        for (int half = 0; half < 2; ++half) {
            const int32_t* r = residual + half * 8;
            int32_t out[8];
            for (int i = 0; i < 8; ++i) {
                int64_t acc = int64_t(book1[i]) * hist[0] + int64_t(book2[i]) * hist[1];
                // The convolution runs over the RESIDUALS, not over the decoded
                // samples. Feeding out[j] back here makes the recursion
                // unstable: the output stays smooth (so it looks plausible) and
                // then runs away to saturation within a few hundred samples.
                // That was the original bug, and only the ROM's loop-point
                // state caught it.
                for (int j = 0; j < i; ++j)
                    acc += int64_t(book2[i - 1 - j]) * r[j];
                acc += int64_t(r[i]) << 11;
                out[i] = int32_t(acc >> 11);
            }
            for (int i = 0; i < 8; ++i) {
                if (!dmemRangeOk(out_off, 2)) break;
                dmemWrite16(dmem_.data(), out_off, clampS16(out[i]));
                out_off += 2;
            }
            hist[0] = clampS16(out[6]);
            hist[1] = clampS16(out[7]);
        }

        in_off += kAdpcmFrameBytes;
        ++stats_.adpcm_frames;
    }

    if (void* dst = access_(state_addr, sizeof(hist), true))
        std::memcpy(dst, hist, sizeof(hist));
    return true;
}

// ---------------------------------------------------------------------------
// Resample — pitch shift by stepping through the input at a fractional rate.
//
// `pitch` is a 16-bit fixed-point ratio in 1.15... in practice the audio library
// passes a value where 0x4000 is unity for this microcode revision, but the
// accumulator below is written in Q16 so the caller's convention is applied in
// one place and is easy to correct if validation shows it off by a power of two.
//
// CONFIDENCE: structure is right, interpolation is linear rather than the
// hardware's 4-tap polyphase filter. That makes it slightly duller than
// hardware, not wrong — pitch and length are exact, which is what matters for
// staying in sync with the sequence player.
// TODO(M1): implement the 4-tap filter once there is a reference to compare to.
// ---------------------------------------------------------------------------

bool Interpreter::cmdResample(uint32_t w0, uint32_t w1) {
    const uint8_t flags = uint8_t((w0 >> 16) & 0xFF);
    const uint32_t pitch = w0 & 0xFFFF;
    const uint32_t state_addr = resolve(w1);
    const uint32_t out_bytes = buff_count_;

    if (!dmemRangeOk(buff_out_, out_bytes)) return false;

    uint32_t frac = 0;
    if (!(flags & A_INIT)) {
        if (const void* s = access_(state_addr, sizeof(frac), false))
            std::memcpy(&frac, s, sizeof(frac));
    }

    const uint32_t step = pitch << 2;  // to Q16
    const uint32_t out_samples = out_bytes / 2;

    for (uint32_t i = 0; i < out_samples; ++i) {
        const uint32_t idx = frac >> 16;
        const uint32_t f = frac & 0xFFFF;
        const uint32_t a_off = buff_in_ + idx * 2;
        const uint32_t b_off = a_off + 2;
        if (!dmemRangeOk(b_off, 2)) break;

        const int32_t a = dmemRead16(dmem_.data(), a_off);
        const int32_t b = dmemRead16(dmem_.data(), b_off);
        const int32_t v = a + (((b - a) * int32_t(f)) >> 16);

        dmemWrite16(dmem_.data(), buff_out_ + i * 2, clampS16(v));
        frac += step;
        ++stats_.samples_resampled;
    }

    if (void* dst = access_(state_addr, sizeof(frac), true))
        std::memcpy(dst, &frac, sizeof(frac));
    return true;
}

// ---------------------------------------------------------------------------
// Envelope mixer — mixes one buffer into dry L/R and wet L/R with per-sample
// volume ramps. This is where a voice's pan and fade actually happen.
//
// CONFIDENCE: lowest of the sixteen. The ramp arithmetic is reproduced from the
// documented behaviour, but the hardware applies the rate as a 16.16 multiply
// per 8-sample group rather than per sample, and the exact grouping is not
// something I can confirm without a reference. A wrong grouping makes fades
// happen at 8x or 1/8x speed — audible, but not silent-failure.
// TODO(M1): validate ramp timing against a recorded fade.
// ---------------------------------------------------------------------------

bool Interpreter::cmdEnvMixer(uint32_t w0, uint32_t w1) {
    const uint8_t flags = uint8_t((w0 >> 16) & 0xFF);
    const uint32_t state_addr = resolve(w1);
    const uint32_t count = buff_count_;

    if (!dmemRangeOk(buff_in_, count)) return false;

    int16_t vol[2] = {vol_[0], vol_[1]};
    if (!(flags & A_INIT)) {
        if (const void* s = access_(state_addr, sizeof(vol), false))
            std::memcpy(vol, s, sizeof(vol));
    }

    // Dry pair follows the SETBUFF out address; wet pair sits after it when
    // A_AUX is set. With no aux, only the dry pair is written.
    const uint32_t dryL = buff_out_;
    const uint32_t dryR = buff_out_ + count;
    const bool aux = (flags & A_AUX) != 0;
    const uint32_t wetL = buff_out_ + count * 2;
    const uint32_t wetR = buff_out_ + count * 3;

    if (!dmemRangeOk(dryR, count)) return false;
    if (aux && !dmemRangeOk(wetR, count)) return false;

    for (uint32_t i = 0; i + 1 < count; i += 2) {
        const int32_t s = dmemRead16(dmem_.data(), buff_in_ + i);

        for (int ch = 0; ch < 2; ++ch) {
            const int32_t scaled = (s * vol[ch]) >> 15;
            const uint32_t dry = ch ? dryR : dryL;
            dmemWrite16(dmem_.data(), dry + i,
                        clampS16(dmemRead16(dmem_.data(), dry + i) +
                                 ((scaled * vol_dry_) >> 15)));
            if (aux) {
                const uint32_t wet = ch ? wetR : wetL;
                dmemWrite16(dmem_.data(), wet + i,
                            clampS16(dmemRead16(dmem_.data(), wet + i) +
                                     ((scaled * vol_wet_) >> 15)));
            }
        }

        // Ramp toward the target. Stopping AT the target rather than
        // overshooting matters: an unclamped ramp inverts the volume and the
        // voice comes back at full level from the other side.
        for (int ch = 0; ch < 2; ++ch) {
            if (vol_rate_[ch] == 0) continue;
            int32_t v = vol[ch] + vol_rate_[ch];
            if (vol_rate_[ch] > 0) v = std::min<int32_t>(v, vol_target_[ch]);
            else                   v = std::max<int32_t>(v, vol_target_[ch]);
            vol[ch] = clampS16(v);
        }
    }

    if (void* dst = access_(state_addr, sizeof(vol), true))
        std::memcpy(dst, vol, sizeof(vol));
    return true;
}

// ---------------------------------------------------------------------------
// One-pole filter, used for the reverb/aux path.
// ---------------------------------------------------------------------------

bool Interpreter::cmdPoleF(uint32_t w0, uint32_t w1) {
    const uint8_t flags = uint8_t((w0 >> 16) & 0xFF);
    const int32_t gain = int16_t(w0 & 0xFFFF);
    const uint32_t state_addr = resolve(w1);
    const uint32_t count = buff_count_;

    if (!dmemRangeOk(buff_in_, count) || !dmemRangeOk(buff_out_, count))
        return false;

    int16_t prev = 0;
    if (!(flags & A_INIT)) {
        if (const void* s = access_(state_addr, sizeof(prev), false))
            std::memcpy(&prev, s, sizeof(prev));
    }

    for (uint32_t i = 0; i + 1 < count; i += 2) {
        const int32_t x = dmemRead16(dmem_.data(), buff_in_ + i);
        const int32_t y = x + ((int32_t(prev) * gain) >> 15);
        prev = clampS16(y);
        dmemWrite16(dmem_.data(), buff_out_ + i, prev);
    }

    if (void* dst = access_(state_addr, sizeof(prev), true))
        std::memcpy(dst, &prev, sizeof(prev));
    return true;
}

}  // namespace ge_audio
