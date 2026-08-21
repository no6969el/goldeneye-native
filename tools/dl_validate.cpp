// dl_validate.cpp — run the interpreter over REAL GoldenEye display lists.
//
// The unit tests prove the decoder agrees with the game's own decoder on
// synthetic input. This proves the interpreter survives the actual corpus: every
// room display list in every level, extracted from a real ROM.
//
// What it is checking for, in order of how badly each would hurt:
//
//   1. Unknown opcodes. If the game emits something the interpreter does not
//      handle, that geometry silently disappears at runtime. The opcode
//      histogram below is the evidence that the map in gbi.h is complete.
//   2. Lists that do not terminate at G_ENDDL. A runaway walk reads whatever
//      follows in RDRAM as commands.
//   3. Vertex indices outside the 16-entry cache. Would mean the F3D
//      determination is wrong.
//   4. Failed address resolutions.
//
// Usage:  dl_validate <corpus-dir>
// where corpus-dir holds .bin files produced by tools/extract_display_lists.py.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "gbi/gbi.h"
#include "gbi/gbi_interp.h"
#include "ultra/os_io.h"
#include "ultra/rdram.h"

namespace fs = std::filesystem;
using namespace ge_gbi;

namespace {

struct Sink : IDrawSink {
    uint64_t tris = 0;
    uint64_t bad_index = 0;   // index into a cache slot never loaded
    uint64_t degenerate = 0;  // two or more indices equal
    int max_index = -1;

    void drawTriangle(const Tri& t, const CachedVtx cache[kVertexCacheSize]) override {
        ++tris;
        for (int i = 0; i < 3; ++i) {
            max_index = std::max(max_index, int(t.v[i]));
            if (!cache[t.v[i]].valid) ++bad_index;
        }
        if (t.v[0] == t.v[1] || t.v[1] == t.v[2] || t.v[0] == t.v[2]) ++degenerate;
    }
    void setProjection(const float[4][4]) override {}
    void setModelview(const float[4][4]) override {}
    void setGeometryMode(uint32_t) override {}
    void setOtherModeH(uint32_t) override {}
    void setOtherModeL(uint32_t) override {}
    void setTexture(const TextureState&) override {}
    void rdpCommand(uint32_t, uint32_t) override {}
};

const char* opcodeName(uint8_t op) {
    switch (op) {
        case 0x00: return "G_SPNOOP";
        case 0x01: return "G_MTX";
        case 0x03: return "G_MOVEMEM";
        case 0x04: return "G_VTX";
        case 0x06: return "G_DL";
        case 0xB1: return "G_TRI4        <- GoldenEye extension";
        case 0xB6: return "G_CLEARGEOMETRYMODE";
        case 0xB7: return "G_SETGEOMETRYMODE";
        case 0xB8: return "G_ENDDL";
        case 0xB9: return "G_SETOTHERMODE_L";
        case 0xBA: return "G_SETOTHERMODE_H";
        case 0xBB: return "G_TEXTURE";
        case 0xBC: return "G_MOVEWORD";
        case 0xBD: return "G_POPMTX";
        case 0xBE: return "G_CULLDL";
        case 0xBF: return "G_TRI1";
        case 0xC0: return "G_SETTEX      <- CPU-expanded by tex.c";
        case 0xE7: return "G_RDPPIPESYNC";
        case 0xE6: return "G_RDPLOADSYNC";
        case 0xE8: return "G_RDPTILESYNC";
        case 0xF0: return "G_LOADTLUT";
        case 0xF2: return "G_SETTILESIZE";
        case 0xF3: return "G_LOADBLOCK";
        case 0xF5: return "G_SETTILE";
        case 0xF8: return "G_SETFOGCOLOR";
        case 0xFB: return "G_SETENVCOLOR";
        case 0xFC: return "G_SETCOMBINE";
        case 0xFD: return "G_SETTIMG";
        case 0xFF: return "G_SETCIMG";
        default:   return "";
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <corpus-dir>\n", argv[0]);
        return 2;
    }

    if (!ge_ultra::rdramInit()) {
        std::fprintf(stderr, "RDRAM allocation failed\n");
        return 2;
    }
    ge_ultra::rdramResetStats();

    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(argv[1]))
        if (e.is_regular_file() && e.path().extension() == ".bin")
            files.push_back(e.path());
    std::sort(files.begin(), files.end());

    if (files.empty()) {
        std::fprintf(stderr, "no .bin files in %s\n", argv[1]);
        return 2;
    }

    // Layout: display list at 0x10000, a scratch vertex pool at 0x400000.
    // Every segment points at the vertex pool so that G_VTX loads resolve —
    // room lists reference vertices through segment 14 (SPSEGMENT_BG_VTX), and
    // we care that the ADDRESSING works, not what the vertices contain.
    constexpr uint32_t kDlBase = 0x10000;
    constexpr uint32_t kVtxPool = 0x400000;

    uint64_t total_cmds = 0, total_tris = 0, total_verts = 0;
    uint64_t total_unknown = 0, total_settex = 0, total_bad_index = 0;
    uint64_t total_degenerate = 0;
    uint64_t hist[256] = {};
    int max_vtx_index = -1;
    size_t largest = 0;
    std::vector<std::string> failures;

    for (const auto& p : files) {
        FILE* f = std::fopen(p.string().c_str(), "rb");
        if (!f) continue;
        std::fseek(f, 0, SEEK_END);
        const long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (sz <= 0 || uint32_t(sz) > 0x100000) { std::fclose(f); continue; }
        std::vector<uint8_t> data(static_cast<size_t>(sz), 0u);
        if (std::fread(data.data(), 1, data.size(), f) != data.size()) {
            std::fclose(f);
            continue;
        }
        std::fclose(f);
        largest = std::max(largest, data.size());

        // The corpus is big-endian (straight out of the ROM). The interpreter
        // reads native-order Gfx structs, so swap the words on the way in. This
        // is exactly the loader-side swap that os_io.h argues for: here the
        // structure IS known — a display list is an array of two u32 — so a
        // 32-bit swap is unambiguously correct.
        // Clear first. Without this, a list that fails to terminate walks into
        // the previous list's bytes and reports plausible-looking garbage
        // instead of an obvious failure — which is exactly what happened the
        // first time this was run.
        std::memset(ge_ultra::rdramBase() + kDlBase, 0, 0x100000);
        std::memcpy(ge_ultra::rdramBase() + kDlBase, data.data(), data.size());
        ge_ultra::swap32InPlace(ge_ultra::rdramBase() + kDlBase, data.size());

        Sink sink;
        Interpreter interp(ge_ultra::rdramResolver(), &sink);
        // Segment 0 must stay 0. A physical address is itself resolved through
        // the segment table (segment = addr >> 24), so pointing segment 0 at the
        // vertex pool relocates the display list's own base address and the
        // walker reads zeroes — which look exactly like a list full of
        // G_SPNOOP. Only the segments the room data actually uses get remapped.
        for (int s = 1; s < kSegmentCount; ++s) interp.setSegment(s, kVtxPool);

        const bool ok = interp.run(kDlBase);
        const Stats& st = interp.stats();

        if (!ok) failures.push_back(p.filename().string() + " (did not terminate)");
        if (st.unknown_opcodes) {
            failures.push_back(p.filename().string() + " (" +
                               std::to_string(st.unknown_opcodes) + " unknown opcodes)");
        }
        if (sink.bad_index) {
            failures.push_back(p.filename().string() + " (" +
                               std::to_string(sink.bad_index) + " unloaded vertex refs)");
        }

        total_cmds += st.commands;
        total_tris += st.triangles;
        total_verts += st.vertices_loaded;
        total_unknown += st.unknown_opcodes;
        total_settex += st.settex_seen;
        total_bad_index += sink.bad_index;
        total_degenerate += sink.degenerate;
        max_vtx_index = std::max(max_vtx_index, sink.max_index);
        for (int i = 0; i < 256; ++i) hist[i] += st.opcode_hist[i];
    }

    std::printf("=== GoldenEye display-list corpus ===\n");
    std::printf("lists:        %zu   (largest %zu bytes)\n", files.size(), largest);
    std::printf("commands:     %llu\n", (unsigned long long)total_cmds);
    std::printf("triangles:    %llu\n", (unsigned long long)total_tris);
    std::printf("vertices:     %llu\n", (unsigned long long)total_verts);
    std::printf("\nopcode histogram:\n");
    int distinct = 0;
    for (int i = 0; i < 256; ++i) {
        if (!hist[i]) continue;
        ++distinct;
        const char* n = opcodeName(uint8_t(i));
        std::printf("  0x%02X  %10llu  %s%s\n", i, (unsigned long long)hist[i],
                    *n ? n : "*** UNKNOWN ***", "");
    }
    std::printf("distinct opcodes: %d\n", distinct);

    std::printf("\nchecks:\n");
    std::printf("  unknown opcodes:        %llu\n", (unsigned long long)total_unknown);
    std::printf("  G_SETTEX (expected):    %llu\n", (unsigned long long)total_settex);
    std::printf("  max vertex index:       %d  (cache is %d)\n", max_vtx_index,
                kVertexCacheSize);
    std::printf("  refs to unloaded slots: %llu\n", (unsigned long long)total_bad_index);
    std::printf("  degenerate triangles:   %llu\n", (unsigned long long)total_degenerate);
    std::printf("  failed resolves:        %llu\n",
                (unsigned long long)ge_ultra::rdramBadResolveCount());

    bool pass = true;
    if (total_unknown) { std::printf("\nFAIL: unknown opcodes present\n"); pass = false; }
    if (max_vtx_index >= kVertexCacheSize) {
        std::printf("\nFAIL: vertex index %d exceeds the %d-entry cache — the F3D\n"
                    "      determination would be wrong\n", max_vtx_index, kVertexCacheSize);
        pass = false;
    }
    if (total_bad_index) {
        std::printf("\nFAIL: %llu references to never-loaded cache slots\n",
                    (unsigned long long)total_bad_index);
        pass = false;
    }
    if (!failures.empty()) {
        std::printf("\n%zu list(s) with problems:\n", failures.size());
        for (size_t i = 0; i < failures.size() && i < 20; ++i)
            std::printf("  %s\n", failures[i].c_str());
        pass = false;
    }

    ge_ultra::rdramShutdown();
    std::printf("\n%s\n", pass ? "CORPUS VALIDATION PASSED" : "CORPUS VALIDATION FAILED");
    return pass ? 0 : 1;
}
