// gbi_interp.h — display-list walker for the gsp3D dialect.
//
// This replaces the RSP. It reads the real GBI stream the game builds into a
// real RDRAM-shaped buffer, and emits an API-agnostic command stream for the
// renderer backend.
//
// Design constraint that drives everything here: the game READS BACK AND
// REWRITES the display lists it just built. src/game/bg.c:2772 scans for G_VTX;
// src/game/lightfixture.c:170-192 walks backwards to the last gSPVertex to
// resolve a segment-14 address. Any port that replaces the gSP* macros with
// immediate-mode calls breaks those systems. So: the game keeps writing GBI
// into memory, and we interpret it. Do not "optimise" that away.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "gbi.h"

namespace ge_gbi {

// Resolves a physical (already segment-resolved) N64 address to a host pointer.
// The port backs this with its RDRAM allocation; tests back it with a flat
// array.
using RdramResolver = std::function<const void*(uint32_t phys_addr, size_t len)>;

struct TextureState {
    uint16_t scale_s = 0xFFFF;
    uint16_t scale_t = 0xFFFF;
    uint8_t  level   = 0;
    uint8_t  tile    = 0;
    bool     on      = false;
};

// The interpreter's view of a vertex after the RSP would have transformed it.
// The interpreter itself does NOT transform — it hands the renderer the raw
// vertex plus the current matrix state and lets the GPU do the work. Doing the
// transform on the CPU here would throw away the entire point of having a
// modern renderer.
struct CachedVtx {
    Vtx  raw{};
    bool valid = false;
};

// What the interpreter emits. Deliberately not a GPU API — the backend
// (RT64 fork) translates these.
class IDrawSink {
public:
    virtual ~IDrawSink() = default;

    // Triangle with three indices into the current vertex cache.
    virtual void drawTriangle(const Tri& tri, const CachedVtx cache[kVertexCacheSize]) = 0;

    virtual void setProjection(const float m[4][4]) = 0;
    virtual void setModelview(const float m[4][4]) = 0;
    virtual void setGeometryMode(uint32_t mode) = 0;
    virtual void setOtherModeH(uint32_t v) = 0;
    virtual void setOtherModeL(uint32_t v) = 0;
    virtual void setTexture(const TextureState& t) = 0;

    // Raw RDP command (0xC1..0xFF), already segment-fixed where applicable.
    // Combiner, tile setup, texture load, fill rect, scissor all arrive here.
    virtual void rdpCommand(uint32_t w0, uint32_t w1) = 0;
};

struct Stats {
    uint32_t commands = 0;
    uint32_t triangles = 0;
    uint32_t vertices_loaded = 0;
    uint32_t unknown_opcodes = 0;
    uint32_t dl_depth_max = 0;
    // 0xC0 seen. Expected and harmless in a RAW room list; a bug if it appears
    // in a list that has already been through texLoadFromGdl(). Counted
    // separately from unknown_opcodes so the distinction stays visible.
    uint32_t settex_seen = 0;
    uint32_t opcode_hist[256] = {};
};

class Interpreter {
public:
    Interpreter(RdramResolver resolver, IDrawSink* sink);

    void reset();

    // Walk a display list starting at a segmented or physical address.
    // Returns false if it bailed out (bad address, stack overflow, runaway).
    bool run(uint32_t addr);

    void setSegment(int seg, uint32_t base);
    uint32_t segmentedToPhysical(uint32_t addr) const;

    const Stats& stats() const { return stats_; }

    // Exposed for the VR layer: the projection matrix currently loaded. fr.c
    // swaps this per eye (see patches/DECOMP-PATCHES.md §1).
    const float (*projection() const)[4] { return proj_; }

    // Print every G_DL call/branch and every list that ends abnormally.
    //
    // Off by default and deliberately not a compile-time option: the failure it
    // diagnoses -- a list that walks into unmapped or zeroed RDRAM -- only
    // happens in the running game, where a rebuild to enable tracing is a slow
    // way to ask a fast question. Set GE_GBI_TRACE=1, or call this.
    void setTrace(bool on) { trace_ = on; }

private:
    bool execute(const Gfx* cmd, bool* end_dl);
    void cmdMtx(uint32_t w0, uint32_t w1);
    void cmdVtx(uint32_t w0, uint32_t w1);
    void cmdMoveWord(uint32_t w0, uint32_t w1);
    void cmdTexture(uint32_t w0, uint32_t w1);
    void cmdSetOtherMode(uint8_t op, uint32_t w0, uint32_t w1);
    bool cmdCullDl(uint32_t w0, uint32_t w1);

    RdramResolver resolve_;
    IDrawSink* sink_;

    uint32_t segment_[kSegmentCount]{};
    CachedVtx vcache_[kVertexCacheSize]{};

    float proj_[4][4]{};
    float modelview_[4][4]{};
    std::vector<std::array<float, 16>> mtx_stack_;

    uint32_t geometry_mode_ = 0;
    uint32_t othermode_h_ = 0;
    uint32_t othermode_l_ = 0;
    TextureState texture_{};
    uint16_t persp_norm_ = 0xFFFF;
    uint32_t num_lights_ = 0;

    int dl_depth_ = 0;
    bool trace_ = false;
    Stats stats_{};
};

}  // namespace ge_gbi
