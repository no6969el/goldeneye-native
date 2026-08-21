#include "gbi_interp.h"

#include <array>
#include <cstdio>
#include <cstring>

#include "rhi/vertex_pipeline.h"

namespace ge_gbi {

namespace {
// Runaway guard. A corrupt display list with no G_ENDDL would otherwise walk
// RDRAM forever; on hardware the RSP would eventually fault, here we would hang
// the render thread with no diagnostic.
constexpr uint32_t kMaxCommandsPerList = 1u << 20;
}  // namespace

Interpreter::Interpreter(RdramResolver resolver, IDrawSink* sink)
    : resolve_(std::move(resolver)), sink_(sink) {
    reset();
}

void Interpreter::reset() {
    std::memset(segment_, 0, sizeof(segment_));
    for (auto& v : vcache_) v = CachedVtx{};
    mtx_stack_.clear();
    geometry_mode_ = 0;
    othermode_h_ = othermode_l_ = 0;
    texture_ = TextureState{};
    persp_norm_ = 0xFFFF;
    num_lights_ = 0;
    dl_depth_ = 0;
    stats_ = Stats{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            proj_[i][j] = (i == j) ? 1.0f : 0.0f;
            modelview_[i][j] = (i == j) ? 1.0f : 0.0f;
        }
}

void Interpreter::setSegment(int seg, uint32_t base) {
    if (seg >= 0 && seg < kSegmentCount) segment_[seg] = base;
}

uint32_t Interpreter::segmentedToPhysical(uint32_t addr) const {
    // Microcode: mask 0x00FFFFFF, segment index is (addr >> 24) & 0xF.
    // Note only FOUR bits of segment are used — the ucode's `srl 22` + `andi
    // 0x3c` discards bits 28 and above. An address of 0x1E000000 therefore
    // resolves through segment 14, not out of range.
    const uint32_t seg = (addr >> 24) & 0xF;
    return segment_[seg] + (addr & 0x00FFFFFF);
}

bool Interpreter::run(uint32_t addr) {
    if (dl_depth_ >= kDlStackDepth) return false;
    ++dl_depth_;
    if (uint32_t(dl_depth_) > stats_.dl_depth_max) stats_.dl_depth_max = dl_depth_;

    const uint32_t start = segmentedToPhysical(addr);
    uint32_t phys = start;
    uint32_t executed = 0;
    bool ok = true;

    if (trace_) {
        std::fprintf(stderr, "%*s-> dl 0x%08X (phys 0x%08X)\n",
                     (dl_depth_ - 1) * 2, "", addr, start);
    }

    for (;;) {
        if (++executed > kMaxCommandsPerList) { ok = false; break; }

        const auto* cmd = static_cast<const Gfx*>(resolve_(phys, sizeof(Gfx)));
        if (!cmd) { ok = false; break; }

        bool end_dl = false;
        if (!execute(cmd, &end_dl)) { ok = false; break; }
        if (end_dl) break;

        phys += sizeof(Gfx);
    }

    if (trace_ && (!ok || executed > 4096)) {
        /* Only the abnormal ends are worth a line. A list that ran 400,000
           commands did not "run"; it fell into zeroed RDRAM and kept going
           until a stray 0xB8 stopped it. Reporting the ENTRY address is the
           point -- that is the caller that needs fixing. */
        std::fprintf(stderr, "%*s!! dl 0x%08X (phys 0x%08X) %s after %u cmds, "
                             "last phys 0x%08X\n",
                     (dl_depth_ - 1) * 2, "", addr, start,
                     ok ? "ran away" : "BAILED", executed, phys);
    }

    --dl_depth_;
    return ok;
}

bool Interpreter::execute(const Gfx* cmd, bool* end_dl) {
    const uint32_t w0 = cmd->w0;
    const uint32_t w1 = cmd->w1;
    const uint8_t op = uint8_t(w0 >> 24);
    ++stats_.commands;
    ++stats_.opcode_hist[op];

    switch (op) {
        case G_SPNOOP:
            return true;

        case G_MTX:
            cmdMtx(w0, w1);
            return true;

        case G_MOVEMEM:
            // Viewport, lights, lookat. The renderer gets the viewport from the
            // eye swapchain in VR, and lighting is reproduced on the GPU, so
            // this is deliberately a no-op at this layer.
            // TODO(phase2): forward viewport to the sink for the flat-screen path.
            return true;

        case G_VTX:
            cmdVtx(w0, w1);
            return true;

        case G_DL: {
            // Byte 1 of w0: 0 = push (call), non-zero = branch (jump, no return).
            const bool branch = ((w0 >> 16) & 0xFF) != 0;
            if (branch) {
                // gSPBranchList: replace the current list. Implemented as a
                // tail call: run the target, then end this list.
                run(w1);
                *end_dl = true;
                return true;
            }
            if (!run(w1)) return false;
            return true;
        }

        case G_TRI1: {
            const Tri t = decodeTri1(w0, w1);
            sink_->drawTriangle(t, vcache_);
            ++stats_.triangles;
            return true;
        }

        case G_TRI4: {
            Tri tris[4];
            const int n = decodeTri4(w0, w1, tris);
            for (int i = 0; i < n; ++i) {
                sink_->drawTriangle(tris[i], vcache_);
                ++stats_.triangles;
            }
            return true;
        }

        case G_ENDDL:
            *end_dl = true;
            return true;

        case G_SETGEOMETRYMODE:
            geometry_mode_ |= w1;
            sink_->setGeometryMode(geometry_mode_);
            return true;

        case G_CLEARGEOMETRYMODE:
            geometry_mode_ &= ~w1;
            sink_->setGeometryMode(geometry_mode_);
            return true;

        case G_SETOTHERMODE_H:
        case G_SETOTHERMODE_L:
            cmdSetOtherMode(op, w0, w1);
            return true;

        case G_TEXTURE:
            cmdTexture(w0, w1);
            return true;

        case G_MOVEWORD:
            cmdMoveWord(w0, w1);
            return true;

        case G_POPMTX:
            if (!mtx_stack_.empty()) {
                const auto& top = mtx_stack_.back();
                std::memcpy(modelview_, top.data(), sizeof(modelview_));
                mtx_stack_.pop_back();
                sink_->setModelview(modelview_);
            }
            // Underflow is silently ignored — matching the microcode, which does
            // not detect stack imbalance either. Do not "fix" this: game code
            // may rely on the forgiving behaviour.
            return true;

        case G_CULLDL:
            if (cmdCullDl(w0, w1)) *end_dl = true;
            return true;

        case G_RDPHALF_1:
        case G_RDPHALF_2:
        case G_RDPHALF_CONT:
        case G_LINE3D:
            // TODO(phase2): G_LINE3D is used by the debug/AI-path overlays only.
            return true;

        case G_NOOP:
            // 0xC0. This is G_SETTEX in the game's own naming, but it is a NOOP
            // to the RCP: the RSP forwards it and the RDP discards it. It is
            // expanded on the CPU by texLoadFromGdl() (src/game/tex.c:817)
            // BEFORE submission. If one reaches here, the list was not run
            // through the texture pre-pass — which is a bug in the port, not
            // something to emulate.
            ++stats_.settex_seen;
            return true;

        default:
            if (op >= 0xC1) {
                // RDP passthrough. Only SETTIMG/SETZIMG/SETCIMG carry an address
                // in w1 that needs segment resolution; everything else is
                // forwarded verbatim.
                uint32_t out_w1 = w1;
                if (op == G_SETTIMG || op == G_SETZIMG || op == G_SETCIMG)
                    out_w1 = segmentedToPhysical(w1);
                sink_->rdpCommand(w0, out_w1);
                return true;
            }
            ++stats_.unknown_opcodes;
            return true;
    }
}

void Interpreter::cmdMtx(uint32_t w0, uint32_t w1) {
    // F3D parameter encoding, in byte 1 of w0. Note this is NOT the F3DEX form,
    // where the flags are inverted — mixing them up loads the projection matrix
    // into the modelview slot, which looks like "the camera is stuck at origin".
    const uint8_t params = uint8_t((w0 >> 16) & 0xFF);
    const bool projection = (params & G_MTX_PROJECTION) != 0;
    const bool load       = (params & G_MTX_LOAD) != 0;
    const bool push       = (params & G_MTX_PUSH) != 0;

    const auto* m = static_cast<const Mtx*>(
        resolve_(segmentedToPhysical(w1), sizeof(Mtx)));
    if (!m) return;

    float f[4][4];
    mtxToFloat(*m, f);

    float (*target)[4] = projection ? proj_ : modelview_;

    if (push && !projection) {
        if (mtx_stack_.size() < kMatrixStackDepth) {
            std::array<float, 16> saved{};
            std::memcpy(saved.data(), modelview_, sizeof(modelview_));
            mtx_stack_.push_back(saved);
        }
        // Overflow silently does not push — matching the microcode, which
        // compares against its stack limit and skips. The load/multiply still
        // happens. This asymmetry is real hardware behaviour.
    }

    if (load) {
        std::memcpy(target, f, sizeof(f));
    } else {
        float r[4][4];
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k) s += f[i][k] * target[k][j];
                r[i][j] = s;
            }
        std::memcpy(target, r, sizeof(r));
    }

    if (projection) sink_->setProjection(proj_);
    else            sink_->setModelview(modelview_);
}

void Interpreter::cmdVtx(uint32_t w0, uint32_t w1) {
    // F3D packing (gbi.h:1865, via gDma1p):
    //   w0 = G_VTX<<24 | (((n)-1)<<4 | v0)<<16 | (16*n)
    // The microcode reads byte 1 and splits the nibbles: low = v0, high = n-1.
    const uint8_t b1 = uint8_t((w0 >> 16) & 0xFF);
    const int v0 = b1 & 0xF;
    const int n  = ((b1 >> 4) & 0xF) + 1;

    const auto* verts = static_cast<const Vtx*>(
        resolve_(segmentedToPhysical(w1), sizeof(Vtx) * n));
    if (!verts) return;

    for (int i = 0; i < n; ++i) {
        const int dst = v0 + i;
        // The cache is 16 entries and the hardware index is 4 bits, so a load
        // that would run past the end simply cannot address the overflow. Clamp
        // rather than wrap: wrapping would silently corrupt earlier vertices.
        if (dst >= kVertexCacheSize) break;
        vcache_[dst].raw = verts[i];
        vcache_[dst].valid = true;
        ++stats_.vertices_loaded;
    }
}

void Interpreter::cmdMoveWord(uint32_t w0, uint32_t w1) {
    // gImmp21: index in byte 3, offset in bytes 1-2.
    const uint8_t index  = uint8_t((w0 >> 0) & 0xFF);
    const uint16_t offset = uint16_t((w0 >> 8) & 0xFFFF);

    switch (index) {
        case G_MW_SEGMENT:
            setSegment(int(offset >> 2), w1);
            break;
        case G_MW_NUMLIGHT:
            // NUML(n) = (n+1)*32 | 0x80000000; the ucode masks to 12 bits.
            num_lights_ = ((w1 & 0xFFF) / 32);
            if (num_lights_ > 0) --num_lights_;
            break;
        case G_MW_PERSPNORM:
            persp_norm_ = uint16_t(w1);
            break;
        case G_MW_FOG:
        case G_MW_CLIP:
        case G_MW_LIGHTCOL:
        case G_MW_MATRIX:
        default:
            // TODO(phase2): fog params and light colours need forwarding once
            // the shader side exists.
            break;
    }
}

void Interpreter::cmdTexture(uint32_t w0, uint32_t w1) {
    texture_.level = uint8_t((w0 >> 11) & 0x07);
    texture_.tile  = uint8_t((w0 >> 8) & 0x07);
    texture_.on    = ((w0 >> 0) & 0xFF) != 0;
    texture_.scale_s = uint16_t((w1 >> 16) & 0xFFFF);
    texture_.scale_t = uint16_t((w1 >> 0) & 0xFFFF);
    sink_->setTexture(texture_);
}

void Interpreter::cmdSetOtherMode(uint8_t op, uint32_t w0, uint32_t w1) {
    // F3D: length in byte 3, shift in byte 2. The mask is built from those and
    // the incoming bits replace that field only.
    const uint32_t len   = ((w0 >> 0) & 0xFF) + 1;
    const uint32_t shift = (w0 >> 8) & 0xFF;
    const uint32_t mask  = (len >= 32) ? 0xFFFFFFFFu
                                       : (((1u << len) - 1u) << shift);

    if (op == G_SETOTHERMODE_H) {
        othermode_h_ = (othermode_h_ & ~mask) | (w1 & mask);
        sink_->setOtherModeH(othermode_h_);
    } else {
        othermode_l_ = (othermode_l_ & ~mask) | (w1 & mask);
        sink_->setOtherModeL(othermode_l_);
    }
}

bool Interpreter::cmdCullDl(uint32_t w0, uint32_t w1) {
    // gSPCullDisplayList(vstart, vend): w0 low bits = (0x0f & vstart)*40,
    // w1 = vend*40. The *40 and the 0x0f mask are further confirmation of the
    // 16-entry / 40-byte-stride cache.
    //
    // This computes real clip outcodes for the named vertex range and applies
    // the hardware's rule: if every vertex in the range is outside the SAME
    // frustum plane, the whole display list is skipped.
    //
    // Only the outcodes are computed on the CPU, not the full transform — the
    // GPU still does the drawing transform. A cull test is a handful of vertices
    // per display list; doing it here costs almost nothing and restores the
    // portal culling the game is built around.
    //
    // Validated on real data by tools/geom_validate.cpp: across all 1,296 rooms
    // in the game, rooms the camera is standing in survive and rooms displaced
    // off-screen are culled.
    const uint32_t vstart = (w0 & 0x0FFF) / 40;
    const uint32_t vend = (w1 & 0x0FFF) / 40;
    if (vstart > vend || vend >= kVertexCacheSize) return false;

    const ge_rhi::Mat4 mvp = ge_rhi::multiply(
        *reinterpret_cast<const ge_rhi::Mat4*>(modelview_),
        *reinterpret_cast<const ge_rhi::Mat4*>(proj_));

    ge_rhi::Viewport vp{};
    uint16_t common = 0xFFFF;
    int considered = 0;

    for (uint32_t i = vstart; i <= vend; ++i) {
        if (!vcache_[i].valid) continue;
        const ge_rhi::TransformedVtx t =
            ge_rhi::transformVertex(vcache_[i].raw, mvp, vp, persp_norm_);
        common &= t.outcode;
        ++considered;
    }

    // No usable vertices means no basis for a decision. Never-cull remains the
    // safe failure: drawing geometry hardware would have skipped costs
    // performance, whereas culling wrongly makes world geometry vanish.
    if (considered == 0) return false;
    return common != 0;
}

}  // namespace ge_gbi
