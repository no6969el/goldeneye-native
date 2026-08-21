/*
 * ge_gfx_probe.cpp -- what is actually IN the game's display lists.
 *
 * The port now submits real graphics tasks (PRIORITIES.md P3m), and until this
 * file existed the handler counted them and threw them away. "3 graphics tasks"
 * says the plumbing works; it says nothing about whether the lists are walkable,
 * whether they address real vertices, or whether the geometry is sane.
 *
 * This walks each task's display list with the port's own interpreter -- the
 * same one validated over all 1,937 room lists in the decomp -- and reports what
 * it found. It draws nothing. That is deliberate: the first question is whether
 * the data is right, and a renderer would answer it with a black screen either
 * way.
 *
 * The interpreter is the honest instrument here because it is independent of
 * everything the boot path does. It was written and validated against display
 * lists extracted from the decomp's own C source, long before the game could
 * run; if it walks a list the RUNNING game built, in RDRAM, and finds the same
 * opcode vocabulary, that is real corroboration rather than a tautology.
 */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gbi/gbi_interp.h"
#include "ultra/os.h"
#include "ultra/rdram.h"

extern "C" {
#include "ultra/os_sp.h"
}

namespace {

using namespace ge_gbi;

/*
 * Counts only. Vertices are recorded so the report can say whether the list
 * addressed geometry that actually resolved, which is the difference between
 * "walked fine" and "walked fine over nothing".
 */
struct ProbeSink : IDrawSink {
    uint32_t tris = 0;
    uint32_t tris_with_invalid_vtx = 0;
    float min_x = 1e30f, max_x = -1e30f;
    float min_y = 1e30f, max_y = -1e30f;
    uint32_t rdp_cmds = 0;

    void drawTriangle(const Tri& t, const CachedVtx cache[kVertexCacheSize]) override {
        ++tris;
        for (int i = 0; i < 3; ++i) {
            const CachedVtx& c = cache[t.v[i]];
            if (!c.valid) { ++tris_with_invalid_vtx; return; }
        }
        for (int i = 0; i < 3; ++i) {
            const Vtx& v = cache[t.v[i]].raw;
            const float x = float(v.x);
            const float y = float(v.y);
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }
    void setProjection(const float[4][4]) override {}
    void setModelview(const float[4][4]) override {}
    void setGeometryMode(uint32_t) override {}
    void setOtherModeH(uint32_t) override {}
    void setOtherModeL(uint32_t) override {}
    void setTexture(const TextureState&) override {}
    void rdpCommand(uint32_t, uint32_t) override { ++rdp_cmds; }
};

unsigned g_reported = 0;

}  // namespace

extern "C" void geGfxProbeHandler(struct OSTask_s* task, void* user)
{
    (void)user;
    if (task == nullptr) {
        return;
    }

    /*
     * Report the first few and then go quiet. A per-frame dump would bury the
     * one thing worth reading -- whether the FIRST list was sane.
     */
    const bool verbose = (g_reported < 2 || g_reported == 60 || g_reported == 200);
    ++g_reported;

    unsigned int dl = 0, len = 0;
    geSpTaskDisplayList(task, &dl, &len);

    /*
     * Raw dump of the head of the list, BEFORE interpreting. The interpreter
     * follows G_DL calls, so its opcode histogram mixes the master list with
     * everything it jumps into; when a walk goes wrong the histogram cannot say
     * where. These 32 words can.
     */
    if (verbose) {
        /* GE_DUMP=<hex phys addr> dumps somewhere else instead: the trace names
           the physical address of a list that went wrong, and being able to look
           at it without a rebuild is the whole point of having named it. */
        uint32_t base = dl & 0x00FFFFFFu;
        if (const char* d = std::getenv("GE_DUMP")) {
            base = uint32_t(std::strtoul(d, nullptr, 0));
        }
        std::printf("[gfx] raw head of 0x%08X (phys 0x%08X):\n", dl, base);
        for (unsigned i = 0; i < 24; ++i) {
            const uint32_t phys = base + i * 8u;
            const void* p = ge_ultra::physicalToVirtual(phys, 8);
            if (p == nullptr) {
                std::printf("      +%03u  <unresolvable>\n", i * 8u);
                break;
            }
            uint32_t w0, w1;
            std::memcpy(&w0, p, 4);
            std::memcpy(&w1, (const char*)p + 4, 4);
            std::printf("      +%03u  %08X %08X   op=%02X\n",
                        i * 8u, w0, w1, unsigned(w0 >> 24));
        }
        std::fflush(stdout);
    }

    ProbeSink sink;
    Interpreter interp(ge_ultra::rdramResolver(), &sink);
    if (verbose && std::getenv("GE_GBI_TRACE") != nullptr) {
        interp.setTrace(true);
    }
    const bool ok = interp.run(dl);
    const Stats& st = interp.stats();

    if (!verbose) {
        return;
    }

    std::printf("[gfx] task %u: dl=0x%08X len=%u -> %s\n",
                g_reported, dl, len, ok ? "walked to G_ENDDL" : "BAILED OUT");
    std::printf("      %u commands, %u triangles, %u vertices loaded, "
                "%u unknown opcodes\n",
                st.commands, st.triangles, st.vertices_loaded,
                st.unknown_opcodes);
    std::printf("      %u RDP commands, %u triangles with an unresolved vertex\n",
                sink.rdp_cmds, sink.tris_with_invalid_vtx);
    if (sink.tris > sink.tris_with_invalid_vtx) {
        std::printf("      model-space extent x[%.0f..%.0f] y[%.0f..%.0f]\n",
                    double(sink.min_x), double(sink.max_x),
                    double(sink.min_y), double(sink.max_y));
    }

    /* The opcode vocabulary, which is the part that corroborates. */
    std::printf("      opcodes:");
    for (int i = 0; i < 256; ++i) {
        if (st.opcode_hist[i]) {
            std::printf(" %02X:%u", i, st.opcode_hist[i]);
        }
    }
    std::printf("\n");
    std::fflush(stdout);
}
