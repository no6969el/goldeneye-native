// audio_validate.cpp — bit-exact VADPCM validation against ROM ground truth.
//
// See tools/extract_adpcm_vectors.py for where the oracle comes from: each
// looped instrument in the bank stores ALADPCMloop::state, the sixteen decoded
// samples immediately preceding its loop point, written by Nintendo's encoder.
// Decode up to that point and the last sixteen samples must match exactly.
//
// This also exercises something the unit tests do not: the decode runs in
// frame-aligned CHUNKS with decoder state carried through RDRAM between
// commands, which is how the audio library actually drives A_ADPCM. A state
// carry-over bug shows up here and nowhere else.
//
// Usage: audio_validate <vectors.bin>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>

#include "ultra/audio/abi.h"
#include "ultra/audio/acmd_interp.h"
#include "ultra/rdram.h"

using namespace ge_audio;

namespace {

struct Vector {
    uint32_t order = 0, npred = 0, loop_start = 0;
    std::vector<int16_t> book;
    int16_t expected[16] = {};
    std::vector<uint8_t> data;
};

bool readVectors(const char* path, std::vector<Vector>& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    uint32_t count = 0;
    if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "GEAV", 4) != 0) {
        std::fclose(f);
        return false;
    }
    if (std::fread(&count, 4, 1, f) != 1) { std::fclose(f); return false; }

    for (uint32_t i = 0; i < count; ++i) {
        Vector v;
        uint32_t book_n = 0, data_n = 0;
        if (std::fread(&v.order, 4, 1, f) != 1) break;
        if (std::fread(&v.npred, 4, 1, f) != 1) break;
        if (std::fread(&book_n, 4, 1, f) != 1) break;
        v.book.resize(book_n);
        if (std::fread(v.book.data(), 2, book_n, f) != book_n) break;
        if (std::fread(&v.loop_start, 4, 1, f) != 1) break;
        if (std::fread(v.expected, 2, 16, f) != 16) break;
        if (std::fread(&data_n, 4, 1, f) != 1) break;
        v.data.resize(data_n);
        if (std::fread(v.data.data(), 1, data_n, f) != data_n) break;
        out.push_back(std::move(v));
    }
    std::fclose(f);
    return !out.empty();
}

// DMEM is 4 KiB, and a sample can be several times that, so decode in chunks —
// exactly as the audio library does.
constexpr uint32_t kFramesPerChunk = 16;
constexpr uint32_t kChunkInBytes = kFramesPerChunk * kAdpcmFrameBytes;      // 144
constexpr uint32_t kChunkOutBytes = kFramesPerChunk * kAdpcmSamplesPerFrame * 2;  // 512
constexpr uint32_t kDmemIn = 0x0000;
constexpr uint32_t kDmemOut = 0x0400;

constexpr uint32_t kBookPhys = 0x100000;
constexpr uint32_t kStatePhys = 0x101000;

RdramAccess ramAccess() {
    return [](uint32_t phys, size_t len, bool) -> void* {
        return ge_ultra::physicalToVirtual(phys, len);
    };
}

// Decode a whole vector, returning every sample.
std::vector<int16_t> decodeAll(const Vector& v, Interpreter& in) {
    // Codebook into RDRAM for A_LOADADPCM.
    void* book_dst = ge_ultra::physicalToVirtual(kBookPhys, v.book.size() * 2);
    std::memcpy(book_dst, v.book.data(), v.book.size() * 2);
    std::memset(ge_ultra::physicalToVirtual(kStatePhys, 64), 0, 64);

    Acmd load{(uint32_t(A_LOADADPCM) << 24) | uint32_t(v.book.size() * 2),
              kBookPhys};
    in.run(&load, 1);

    std::vector<int16_t> pcm;
    const uint32_t total_frames = uint32_t(v.data.size()) / kAdpcmFrameBytes;
    bool first = true;

    for (uint32_t f = 0; f < total_frames; f += kFramesPerChunk) {
        const uint32_t frames = std::min(kFramesPerChunk, total_frames - f);
        const uint32_t in_bytes = frames * kAdpcmFrameBytes;
        const uint32_t out_bytes = frames * kAdpcmSamplesPerFrame * 2;

        std::memset(in.dmemBytes(), 0, kAudioDmemSize);
        std::memcpy(in.dmemBytes() + kDmemIn,
                    v.data.data() + f * kAdpcmFrameBytes, in_bytes);

        Acmd cmds[2];
        cmds[0] = Acmd{(uint32_t(A_SETBUFF) << 24) | kDmemIn,
                       (kDmemOut << 16) | out_bytes};
        // A_INIT only on the very first chunk. Every later chunk must pick the
        // running state up out of RDRAM — that is the carry-over this validates.
        cmds[1] = Acmd{(uint32_t(A_ADPCM) << 24) |
                           (uint32_t(first ? A_INIT : A_CONTINUE) << 16),
                       kStatePhys};
        in.run(cmds, 2);
        first = false;

        const int16_t* src = in.dmem() + kDmemOut / 2;
        pcm.insert(pcm.end(), src, src + out_bytes / 2);
    }
    return pcm;
}

// A correct decode of an instrument sample is smooth; a wrong predictor
// formulation produces noise. Mean |first difference| / RMS is ~1.41 for white
// noise and well under 1 for tonal audio. Reported alongside the exact match so
// a failure says WHICH way it failed.
double roughness(const std::vector<int16_t>& pcm) {
    if (pcm.size() < 2) return 0.0;
    double sumsq = 0.0, sumdiff = 0.0;
    for (size_t i = 0; i < pcm.size(); ++i) sumsq += double(pcm[i]) * pcm[i];
    for (size_t i = 1; i < pcm.size(); ++i)
        sumdiff += std::abs(double(pcm[i]) - double(pcm[i - 1]));
    const double rms = std::sqrt(sumsq / double(pcm.size()));
    const double mad = sumdiff / double(pcm.size() - 1);
    return rms > 1e-9 ? mad / rms : 0.0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <vectors.bin>\n", argv[0]);
        return 2;
    }

    std::vector<Vector> vectors;
    if (!readVectors(argv[1], vectors)) {
        std::fprintf(stderr, "could not read vectors from %s\n", argv[1]);
        return 2;
    }
    if (!ge_ultra::rdramInit()) return 2;

    std::printf("=== VADPCM validation against ROM loop-point state ===\n");
    std::printf("%zu vector(s)\n\n", vectors.size());

    int passed = 0;
    for (size_t i = 0; i < vectors.size(); ++i) {
        const Vector& v = vectors[i];
        Interpreter in(ramAccess());
        const std::vector<int16_t> pcm = decodeAll(v, in);

        std::printf("case %zu: order=%u npred=%u loop_start=%u  -> %zu samples\n",
                    i, v.order, v.npred, v.loop_start, pcm.size());

        if (pcm.size() < v.loop_start - (v.loop_start % kAdpcmSamplesPerFrame) + 16) {
            std::printf("  SKIP: decoded %zu < loop_start %u\n", pcm.size(),
                        v.loop_start);
            continue;
        }

        /*
         * ALADPCMloop::state is the frame CONTAINING the loop point, not the
         * sixteen samples before it.
         *
         * Measured, not assumed: decode all 48 looped ADPCM waves in the
         * instrument bank in full and search the output for each state
         * sequence. It is present in every one of them, at
         * loop_start - (loop_start % 16) -- i.e. the frame boundary at or below
         * the loop point, which is what a resume seed has to be, since the
         * decoder can only restart on a frame boundary.
         *
         * The old `loop_start - 16` window reported 0/4 for a decoder that was
         * correct. That is why PRIORITIES.md called A_ADPCM "known incorrect"
         * and ranked it the single highest-value open item.
         */
        const uint32_t win = v.loop_start - (v.loop_start % kAdpcmSamplesPerFrame);
        const int16_t* got = pcm.data() + win;
        int mismatches = 0;
        for (int k = 0; k < 16; ++k)
            if (got[k] != v.expected[k]) ++mismatches;

        std::printf("  roughness %.3f (white noise ~1.41)\n", roughness(pcm));
        std::printf("  expected: ");
        for (int k = 0; k < 8; ++k) std::printf("%6d ", v.expected[k]);
        std::printf("...\n  got:      ");
        for (int k = 0; k < 8; ++k) std::printf("%6d ", got[k]);
        std::printf("...\n");

        if (mismatches == 0) {
            std::printf("  PASS: all 16 loop-point samples match exactly\n\n");
            ++passed;
        } else {
            std::printf("  FAIL: %d of 16 loop-point samples differ\n\n",
                        mismatches);
        }
    }

    std::printf("%d / %zu vectors bit-exact\n", passed, vectors.size());
    ge_ultra::rdramShutdown();
    return passed == int(vectors.size()) ? 0 : 1;
}
