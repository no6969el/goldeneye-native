// os_ai.cpp — the Audio Interface shim.
//
// On N64 the AI is a DMA engine that streams a buffer of interleaved 16-bit
// stereo to the DAC and raises an interrupt when it drains. The game's audio
// thread (src/audi.c) uses osAiGetLength() to decide HOW MANY SAMPLES to
// synthesise this frame:
//
//     info->frameSamples = (u16)(((g_FrameSize - (osAiGetLength() >> 2))
//                                 + 16 + EXTRA_SAMPLES) & ~0xf);   // audi.c:517
//
// That single line is why this file cannot be a stub. Return 0 forever and the
// game synthesises a full frame every time regardless of what has been consumed,
// the queue grows without bound, and audio drifts further behind the picture the
// longer you play. Return a constant and it under- or over-produces forever.
//
// So the shim models the drain honestly: the host frame loop reports how many
// samples were actually consumed, and osAiGetLength() reflects what is still
// outstanding.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "os.h"
#include "rdram.h"

namespace ge_ultra {
namespace {

struct AiState {
    // Ring of pending buffers. The N64 AI has a two-deep queue; the game relies
    // on being able to hand over the next buffer while the current one plays.
    struct Pending {
        uint32_t addr = 0;   // physical
        uint32_t bytes = 0;
        uint32_t consumed = 0;
    };
    Pending queue[2];
    int queued = 0;
    uint32_t frequency = 22050;
    uint64_t samples_played = 0;
    uint64_t samples_dropped = 0;
    bool enabled = false;
};

AiState g_ai;

}  // namespace

// ---------------------------------------------------------------------------
// Host surface
// ---------------------------------------------------------------------------

void aiReset() { g_ai = AiState{}; }

uint32_t aiFrequency() { return g_ai.frequency; }

// Drain up to `frames` stereo frames into `out` (interleaved s16 L,R).
// Returns the number of frames actually produced; the rest of `out` is silence.
//
// The port calls this from its audio callback. Producing silence on underrun
// rather than repeating the last buffer matters: a repeat is a very audible
// buzz, silence is a dropout you can measure.
uint32_t aiDrain(int16_t* out, uint32_t frames) {
    uint32_t written = 0;

    while (written < frames && g_ai.queued > 0) {
        AiState::Pending& p = g_ai.queue[0];
        const uint32_t remaining_bytes = p.bytes - p.consumed;
        const uint32_t remaining_frames = remaining_bytes / 4;  // 2ch * s16
        if (remaining_frames == 0) {
            std::memmove(&g_ai.queue[0], &g_ai.queue[1], sizeof(AiState::Pending));
            --g_ai.queued;
            continue;
        }

        const uint32_t n = std::min(frames - written, remaining_frames);
        if (const void* src = physicalToVirtual(p.addr + p.consumed, n * 4)) {
            std::memcpy(out + written * 2, src, n * 4);
        } else {
            std::memset(out + written * 2, 0, n * 4);
            g_ai.samples_dropped += n;
        }
        p.consumed += n * 4;
        written += n;
        g_ai.samples_played += n;
    }

    if (written < frames) {
        std::memset(out + written * 2, 0, (frames - written) * 4);
        g_ai.samples_dropped += frames - written;
    }
    return written;
}

uint64_t aiSamplesPlayed() { return g_ai.samples_played; }
uint64_t aiSamplesDropped() { return g_ai.samples_dropped; }
int aiQueueDepth() { return g_ai.queued; }

}  // namespace ge_ultra

using namespace ge_ultra;

// ---------------------------------------------------------------------------
// libultra
// ---------------------------------------------------------------------------

extern "C" s32 osAiSetFrequency(u32 frequency) {
    g_ai.frequency = frequency;
    g_ai.enabled = true;
    return s32(frequency);
}

extern "C" s32 osAiSetNextBuffer(void* buf, u32 size) {
    // Hardware refuses a third buffer while two are outstanding, and the game
    // checks the return. Silently accepting would let the queue grow and put
    // audio permanently behind the picture.
    if (g_ai.queued >= 2) return -1;

    AiState::Pending p;
    p.addr = virtualToPhysical(buf);
    p.bytes = size;
    p.consumed = 0;
    if (p.addr == 0 && buf != nullptr) return -1;  // not in RDRAM

    g_ai.queue[g_ai.queued++] = p;
    return 0;
}

extern "C" u32 osAiGetLength(void) {
    // Bytes still outstanding across the queue. audi.c:517 divides this by 4 to
    // get frames and subtracts from its target — see the file header.
    uint32_t bytes = 0;
    for (int i = 0; i < g_ai.queued; ++i)
        bytes += g_ai.queue[i].bytes - g_ai.queue[i].consumed;
    return bytes;
}

extern "C" u32 osAiGetStatus(void) {
    // Bit 31 set = full (two buffers queued); bit 30 = busy.
    uint32_t s = 0;
    if (g_ai.queued >= 2) s |= 0x80000000u;
    if (g_ai.queued >= 1) s |= 0x40000000u;
    return s;
}
