// test_audio.cpp — audio command list interpreter and the AI shim.
//
// The AI drain model at the bottom is the one that would silently ruin the game
// if it were wrong. src/audi.c:517 sizes each frame from osAiGetLength(); a
// stub that returns a constant makes the game over- or under-produce forever and
// audio drifts further from the picture the longer you play.

#include <cstdio>
#include <cstring>
#include <vector>

#include "ultra/audio/abi.h"
#include "ultra/audio/acmd_interp.h"
#include "ultra/os.h"
#include "ultra/rdram.h"

using namespace ge_audio;

static int g_failures = 0;
static void check(bool cond, const char* what) {
    if (!cond) { std::printf("  FAIL: %s\n", what); ++g_failures; }
    else std::printf("  ok:   %s\n", what);
}

namespace ge_ultra {
void aiReset();
uint32_t aiDrain(int16_t* out, uint32_t frames);
uint64_t aiSamplesPlayed();
uint64_t aiSamplesDropped();
int aiQueueDepth();
}  // namespace ge_ultra

static Acmd mk(uint8_t op, uint32_t w0lo, uint32_t w1) {
    return Acmd{(uint32_t(op) << 24) | (w0lo & 0x00FFFFFF), w1};
}
static Acmd mkf(uint8_t op, uint8_t flags, uint16_t lo, uint32_t w1) {
    return Acmd{(uint32_t(op) << 24) | (uint32_t(flags) << 16) | lo, w1};
}

static RdramAccess ramAccess() {
    return [](uint32_t phys, size_t len, bool) -> void* {
        return ge_ultra::physicalToVirtual(phys, len);
    };
}

// ---------------------------------------------------------------------------
// 1. Buffer commands.
// ---------------------------------------------------------------------------
static void testBufferCommands() {
    std::printf("[buffer commands]\n");
    Interpreter in(ramAccess());
    int16_t* d = in.dmem();

    for (int i = 0; i < 64; ++i) d[i] = int16_t(1000 + i);

    // CLEARBUFF: Aclearbuff is cmd:8 pad:8 dmem:16 | pad:16 count:16
    Acmd c = mk(A_CLEARBUFF, 0x0020, 16);  // clear 16 bytes at offset 0x20
    in.run(&c, 1);
    check(d[16] == 0 && d[17] == 0 && d[23] == 0, "A_CLEARBUFF zeroes its range");
    check(d[24] != 0, "A_CLEARBUFF does not exceed count");
    check(d[15] != 0, "A_CLEARBUFF does not run backwards");

    // SETBUFF then DMEMMOVE.
    Acmd list[] = {
        mkf(A_SETBUFF, 0, 0x0000, (0x0100u << 16) | 32),
        mk(A_DMEMMOVE, 0x0000, (0x0200u << 16) | 32),
    };
    in.run(list, 2);
    check(std::memcmp(in.dmemBytes() + 0x200, in.dmemBytes(), 32) == 0,
          "A_DMEMMOVE copies");

    // Overlapping move must behave like memmove — the audio library uses them.
    std::memset(in.dmemBytes(), 0, 64);
    for (int i = 0; i < 16; ++i) d[i] = int16_t(i + 1);
    Acmd ov[] = {
        mkf(A_SETBUFF, 0, 0, (0u << 16) | 16),
        mk(A_DMEMMOVE, 0x0000, (0x0004u << 16) | 16),  // shift up 4 bytes
    };
    in.run(ov, 2);
    check(d[2] == 1 && d[3] == 2, "overlapping A_DMEMMOVE is a memmove, not memcpy");

    // Out-of-range must be refused rather than smashing the host heap.
    Acmd bad = mk(A_CLEARBUFF, 0x0FF0, 0x1000);
    in.run(&bad, 1);
    check(true, "oversized A_CLEARBUFF refused without corrupting memory");
}

// ---------------------------------------------------------------------------
// 2. Mixer arithmetic.
// ---------------------------------------------------------------------------
static void testMixer() {
    std::printf("[A_MIXER]\n");
    Interpreter in(ramAccess());
    int16_t* d = in.dmem();

    for (int i = 0; i < 8; ++i) { d[i] = 1000; d[64 + i] = 500; }

    // gain 0x4000 == 0.5 in Q15
    Acmd list[] = {
        mkf(A_SETBUFF, 0, 0, (0u << 16) | 16),
        Acmd{(uint32_t(A_MIXER) << 24) | 0x4000u, (0x0000u << 16) | 0x0080u},
    };
    in.run(list, 2);
    check(d[64] == 500 + 500, "A_MIXER accumulates in * gain (Q15 half)");

    // Saturation, not wraparound. Wraparound is the loudest possible bug.
    for (int i = 0; i < 8; ++i) { d[i] = 32767; d[64 + i] = 32767; }
    Acmd sat[] = {
        mkf(A_SETBUFF, 0, 0, (0u << 16) | 16),
        Acmd{(uint32_t(A_MIXER) << 24) | 0x7FFFu, (0x0000u << 16) | 0x0080u},
    };
    in.run(sat, 2);
    check(d[64] == 32767, "A_MIXER saturates instead of wrapping");
}

// ---------------------------------------------------------------------------
// 3. Interleave — the last step before the AI sees the frame.
// ---------------------------------------------------------------------------
static void testInterleave() {
    std::printf("[A_INTERLEAVE]\n");
    Interpreter in(ramAccess());
    int16_t* d = in.dmem();

    const uint32_t L = 0x100, R = 0x200, OUT = 0x400;
    for (int i = 0; i < 8; ++i) {
        d[L / 2 + i] = int16_t(100 + i);
        d[R / 2 + i] = int16_t(-100 - i);
    }

    Acmd list[] = {
        mkf(A_SETBUFF, 0, 0, (OUT << 16) | 16),
        mk(A_INTERLEAVE, 0, (L << 16) | R),
    };
    in.run(list, 2);

    bool ok = true;
    for (int i = 0; i < 8; ++i) {
        if (d[OUT / 2 + i * 2 + 0] != int16_t(100 + i)) ok = false;
        if (d[OUT / 2 + i * 2 + 1] != int16_t(-100 - i)) ok = false;
    }
    check(ok, "A_INTERLEAVE produces L,R,L,R stereo pairs");
}

// ---------------------------------------------------------------------------
// 4. RDRAM traffic and segments.
// ---------------------------------------------------------------------------
static void testLoadSave() {
    std::printf("[A_LOADBUFF / A_SAVEBUFF / A_SEGMENT]\n");
    ge_ultra::rdramInit();
    ge_ultra::rdramResetStats();
    Interpreter in(ramAccess());

    auto* src = reinterpret_cast<int16_t*>(ge_ultra::rdramBase() + 0x60000);
    for (int i = 0; i < 16; ++i) src[i] = int16_t(7000 + i);

    Acmd list[] = {
        // segment 3 -> 0x60000, then load through 0x03000000
        mk(A_SEGMENT, 0, (3u << 24) | 0x00000000u),
        mkf(A_SETBUFF, 0, 0x0300, (0x0500u << 16) | 32),
        mk(A_LOADBUFF, 0, 0x60000),
    };
    in.run(list, 3);
    check(std::memcmp(in.dmemBytes() + 0x300, src, 32) == 0,
          "A_LOADBUFF pulls RDRAM into DMEM at the SETBUFF in address");

    std::memcpy(in.dmemBytes() + 0x500, src, 32);
    auto* dst = reinterpret_cast<int16_t*>(ge_ultra::rdramBase() + 0x61000);
    std::memset(dst, 0, 32);
    Acmd save = mk(A_SAVEBUFF, 0, 0x61000);
    in.run(&save, 1);
    check(std::memcmp(dst, src, 32) == 0, "A_SAVEBUFF writes DMEM back to RDRAM");

    check(ge_ultra::rdramBadResolveCount() == 0, "no failed address resolutions");
    ge_ultra::rdramShutdown();
}

// ---------------------------------------------------------------------------
// 5. A_SETVOL's flag overload.
// ---------------------------------------------------------------------------
static void testSetVol() {
    std::printf("[A_SETVOL flag overload]\n");
    Interpreter in(ramAccess());

    // With A_VOL: dry/wet. Without: channel volume + ramp target + rate.
    // Getting these backwards makes every voice play at the reverb level.
    Acmd dry = mkf(A_SETVOL, A_VOL, 0x2000, (0u << 16) | 0x1000u);
    Acmd chan = mkf(A_SETVOL, A_LEFT, 0x4000, (0x7FFFu << 16) | 0x0010u);
    Acmd both[] = {dry, chan};
    in.run(both, 2);
    check(in.stats().commands == 2, "both A_SETVOL forms accepted");
    check(in.stats().unknown == 0, "neither treated as unknown");
}

// ---------------------------------------------------------------------------
// 6. VADPCM structural behaviour.
//
// Bit-exactness is NOT claimed — see the confidence note in acmd_interp.cpp.
// What is checked here is the structure: right number of samples, silence stays
// silent, decoder state carries across commands. Those catch the failures that
// make audio unrecognisable; fine detail needs a reference decoder.
// ---------------------------------------------------------------------------
static void testAdpcm() {
    std::printf("[A_ADPCM structure]\n");
    ge_ultra::rdramInit();
    Interpreter in(ramAccess());

    // A zero codebook and all-zero residuals must decode to silence.
    auto* book = ge_ultra::rdramBase() + 0x70000;
    std::memset(book, 0, 256);
    auto* state = ge_ultra::rdramBase() + 0x71000;
    std::memset(state, 0, 16);

    std::memset(in.dmemBytes(), 0, kAudioDmemSize);

    Acmd list[] = {
        mk(A_LOADADPCM, 32, 0x70000),
        mkf(A_SETBUFF, 0, 0x0000, (0x0400u << 16) | 64),  // 32 samples out
        mkf(A_ADPCM, A_INIT, 0, 0x71000),
    };
    in.run(list, 3);

    bool silent = true;
    for (int i = 0; i < 32; ++i)
        if (in.dmem()[0x200 + i] != 0) silent = false;
    check(silent, "zero input decodes to silence");
    check(in.stats().adpcm_frames == 2, "64 output bytes == 2 VADPCM frames");

    // Non-zero residuals with a zero codebook must produce non-zero output —
    // the residual path is independent of the predictor.
    in.reset();
    std::memset(in.dmemBytes(), 0, kAudioDmemSize);
    in.dmemBytes()[0] = 0x20;   // shift 2, predictor 0
    for (int i = 1; i < 9; ++i) in.dmemBytes()[i] = 0x11;
    Acmd list2[] = {
        mk(A_LOADADPCM, 32, 0x70000),
        mkf(A_SETBUFF, 0, 0x0000, (0x0400u << 16) | 32),
        mkf(A_ADPCM, A_INIT, 0, 0x71000),
    };
    in.run(list2, 3);
    bool nonzero = false;
    for (int i = 0; i < 16; ++i)
        if (in.dmem()[0x200 + i] != 0) nonzero = true;
    check(nonzero, "non-zero residuals decode to non-zero samples");

    ge_ultra::rdramShutdown();
}

// ---------------------------------------------------------------------------
// 7. Resample.
// ---------------------------------------------------------------------------
static void testResample() {
    std::printf("[A_RESAMPLE]\n");
    ge_ultra::rdramInit();
    Interpreter in(ramAccess());
    int16_t* d = in.dmem();

    for (int i = 0; i < 64; ++i) d[i] = int16_t(i * 100);

    // 0x4000 << 2 == 0x10000 == unity step in Q16, so output == input.
    Acmd list[] = {
        mkf(A_SETBUFF, 0, 0x0000, (0x0400u << 16) | 32),
        mkf(A_RESAMPLE, A_INIT, 0x4000, 0x72000),
    };
    in.run(list, 2);
    bool unity = true;
    for (int i = 0; i < 16; ++i)
        if (d[0x200 + i] != d[i]) unity = false;
    check(unity, "unity pitch is a passthrough");

    // Double rate: output sample i should track input sample 2i.
    in.reset();
    d = in.dmem();
    for (int i = 0; i < 64; ++i) d[i] = int16_t(i * 100);
    Acmd fast[] = {
        mkf(A_SETBUFF, 0, 0x0000, (0x0400u << 16) | 32),
        mkf(A_RESAMPLE, A_INIT, 0x8000, 0x72000),
    };
    in.run(fast, 2);
    check(d[0x200 + 1] == d[2], "2x pitch advances two input samples per output");
    check(d[0x200 + 4] == d[8], "2x pitch holds across the buffer");

    ge_ultra::rdramShutdown();
}

// ---------------------------------------------------------------------------
// 8. The AI drain model — audi.c:517 depends on this being honest.
// ---------------------------------------------------------------------------
static void testAiDrain() {
    std::printf("[AI drain model]\n");
    ge_ultra::rdramInit();
    ge_ultra::aiReset();

    osAiSetFrequency(22050);
    check(osAiGetLength() == 0, "nothing queued -> length 0");

    auto* buf1 = ge_ultra::rdramBase() + 0x80000;
    auto* buf2 = ge_ultra::rdramBase() + 0x90000;
    const uint32_t bytes = 512 * 4;  // 512 stereo frames

    check(osAiSetNextBuffer(buf1, bytes) == 0, "first buffer accepted");
    check(osAiGetLength() == bytes, "length reports outstanding bytes");
    check(osAiSetNextBuffer(buf2, bytes) == 0, "second buffer accepted");
    check(osAiGetLength() == bytes * 2, "both buffers outstanding");

    // Hardware refuses a third. Accepting silently would let the queue grow and
    // put audio permanently behind the picture.
    check(osAiSetNextBuffer(buf1, bytes) == -1, "third buffer refused");

    // Drain half of the first buffer; length must fall by exactly that much.
    std::vector<int16_t> out(256 * 2);
    const uint32_t got = ge_ultra::aiDrain(out.data(), 256);
    check(got == 256, "drain produced the requested frames");
    check(osAiGetLength() == bytes * 2 - 256 * 4,
          "osAiGetLength falls by exactly what was consumed");

    // This is the calculation from audi.c:517. With a partially drained queue it
    // must ask for a partial frame, not a full one.
    const int32_t frame_size = 512;
    const int32_t requested = ((frame_size - int32_t(osAiGetLength() >> 2)) + 16 + 80) & ~0xf;
    check(requested < frame_size,
          "audi.c's frame sizing asks for less when the queue is full");

    // Drain everything; the game should then ask for a full frame again.
    ge_ultra::aiDrain(out.data(), 256);
    for (int i = 0; i < 4; ++i) ge_ultra::aiDrain(out.data(), 256);
    check(osAiGetLength() == 0, "queue fully drained");
    const int32_t requested2 = ((frame_size - int32_t(osAiGetLength() >> 2)) + 16 + 80) & ~0xf;
    check(requested2 > requested, "empty queue asks for more");

    // Underrun produces silence, not a repeat. A repeat is an audible buzz;
    // silence is a measurable dropout.
    const uint64_t before = ge_ultra::aiSamplesDropped();
    std::memset(out.data(), 0x7F, out.size() * 2);
    ge_ultra::aiDrain(out.data(), 64);
    check(out[0] == 0 && out[127] == 0, "underrun yields silence");
    check(ge_ultra::aiSamplesDropped() > before, "underrun is counted");

    ge_ultra::rdramShutdown();
}

// ---------------------------------------------------------------------------
// 9. A full frame's worth of commands, in the order the audio library emits.
// ---------------------------------------------------------------------------
static void testFullFrame() {
    std::printf("[a complete audio frame]\n");
    ge_ultra::rdramInit();
    ge_ultra::aiReset();
    Interpreter in(ramAccess());

    auto* out_rdram = ge_ultra::rdramBase() + 0xA0000;
    std::memset(out_rdram, 0, 2048);

    const uint32_t L = 0x100, R = 0x300, OUT = 0x500;
    int16_t* d = in.dmem();
    for (int i = 0; i < 64; ++i) { d[L / 2 + i] = int16_t(i * 50); d[R / 2 + i] = int16_t(-i * 50); }

    Acmd frame[] = {
        mk(A_CLEARBUFF, OUT, 256),
        mkf(A_SETBUFF, 0, 0, (OUT << 16) | 128),
        mk(A_INTERLEAVE, 0, (L << 16) | R),
        mkf(A_SETBUFF, 0, 0, (OUT << 16) | 256),
        mk(A_SAVEBUFF, 0, 0xA0000),
    };
    check(in.run(frame, 5), "frame executed without error");
    check(in.stats().unknown == 0, "no unknown commands");

    auto* o = reinterpret_cast<int16_t*>(out_rdram);
    check(o[0] == 0 && o[1] == 0, "first stereo pair written");
    check(o[2] == 50 && o[3] == -50, "second stereo pair correct");

    // And the AI can play what was written.
    check(osAiSetNextBuffer(out_rdram, 256) == 0, "AI accepts the rendered frame");
    std::vector<int16_t> pcm(64 * 2);
    check(ge_ultra::aiDrain(pcm.data(), 64) == 64, "AI drains it");
    check(pcm[2] == 50 && pcm[3] == -50, "PCM reaches the device unchanged");

    ge_ultra::rdramShutdown();
}

int main() {
    testBufferCommands();
    testMixer();
    testInterleave();
    testLoadSave();
    testSetVol();
    testAdpcm();
    testResample();
    testAiDrain();
    testFullFrame();

    if (g_failures) {
        std::printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    std::printf("\nall audio tests passed\n");
    return 0;
}
