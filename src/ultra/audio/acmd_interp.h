// acmd_interp.h — executes the audio command list on the CPU.
//
// This is to audio what gbi_interp.h is to graphics: the game keeps building the
// real command list in RDRAM, and this walks it instead of the RSP.
//
// The same reasoning applies for keeping the list rather than intercepting
// higher up: alAudioFrame() and the sequence players are real, already-decompiled
// C that we want to run unchanged. Replacing them would mean reimplementing
// GoldenEye's music and SFX behaviour, which is a far larger and much riskier
// job than executing sixteen documented commands.

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "abi.h"

namespace ge_audio {

// Resolves a physical address to a host pointer for `len` bytes, both for
// reading and writing. Backed by ge_ultra::rdram in the port.
using RdramAccess = std::function<void*(uint32_t phys, size_t len, bool write)>;

struct Stats {
    uint32_t commands = 0;
    uint32_t unknown = 0;
    uint32_t adpcm_frames = 0;
    uint32_t samples_resampled = 0;
    uint32_t opcode_hist[16] = {};
};

class Interpreter {
public:
    explicit Interpreter(RdramAccess access);

    void reset();

    // Execute a command list. `count` is the number of Acmd entries.
    // Returns false on a malformed list (bad address, DMEM overrun).
    bool run(const Acmd* cmds, size_t count);

    // Direct DMEM access, for tests and for the AI shim to read the final
    // interleaved output before A_SAVEBUFF writes it back.
    int16_t* dmem() { return reinterpret_cast<int16_t*>(dmem_.data()); }
    uint8_t* dmemBytes() { return dmem_.data(); }

    const Stats& stats() const { return stats_; }

private:
    // --- command handlers ---
    void cmdClearBuff(uint32_t w0, uint32_t w1);
    void cmdSetBuff(uint32_t w0, uint32_t w1);
    void cmdDmemMove(uint32_t w0, uint32_t w1);
    void cmdMixer(uint32_t w0, uint32_t w1);
    void cmdInterleave(uint32_t w0, uint32_t w1);
    void cmdSetVol(uint32_t w0, uint32_t w1);
    void cmdSegment(uint32_t w0, uint32_t w1);
    bool cmdLoadBuff(uint32_t w0, uint32_t w1);
    bool cmdSaveBuff(uint32_t w0, uint32_t w1);
    bool cmdLoadAdpcm(uint32_t w0, uint32_t w1);
    bool cmdAdpcm(uint32_t w0, uint32_t w1);
    bool cmdResample(uint32_t w0, uint32_t w1);
    bool cmdEnvMixer(uint32_t w0, uint32_t w1);
    bool cmdPoleF(uint32_t w0, uint32_t w1);
    void cmdSetLoop(uint32_t w0, uint32_t w1);

    uint32_t resolve(uint32_t addr) const;
    bool dmemRangeOk(uint32_t off, uint32_t bytes) const;

    RdramAccess access_;
    std::vector<uint8_t> dmem_;

    // --- microcode registers ---
    // A_SETBUFF loads these; ADPCM/RESAMPLE/ENVMIXER/POLEF then consume them.
    // They persist between commands, which is why a list that looks wrong in
    // isolation can be correct in sequence.
    uint16_t buff_in_ = 0;
    uint16_t buff_out_ = 0;
    uint16_t buff_count_ = 0;

    // A_SETVOL is overloaded by flags: it sets either the dry/wet levels, or a
    // channel's volume plus its ramp target and rate.
    int16_t vol_[2] = {0, 0};        // left, right
    int16_t vol_target_[2] = {0, 0};
    int32_t vol_rate_[2] = {0, 0};
    int16_t vol_dry_ = 0;
    int16_t vol_wet_ = 0;

    uint32_t segment_[16] = {};
    uint32_t adpcm_loop_addr_ = 0;

    // VADPCM codebook: predictors x order x 8.
    int16_t codebook_[kAdpcmMaxPredictors * kAdpcmOrder * 8] = {};
    int     codebook_predictors_ = 0;

    Stats stats_{};
};

}  // namespace ge_audio
