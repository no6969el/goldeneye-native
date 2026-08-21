// vertex_pipeline.h — the RSP's vertex transform, on the host.
//
// gbi_interp deliberately does NOT transform vertices: it hands the renderer raw
// vertices plus the current matrices and lets the GPU do the work, because doing
// the transform on the CPU throws away the whole point of a modern renderer.
//
// But three things still need the RSP's exact arithmetic:
//
//   1. **Clip outcodes.** G_CULLDL culls a display list by testing the outcodes
//      of a vertex range (gmain.s:272-281, masks 0x7030 / 0x4343). Without them
//      the interpreter can only ever answer "don't cull", which is the safe
//      failure but costs real performance in a game built around portal culling.
//   2. **Screen-space projection read back by game code.** bondview and the HUD
//      project world points to screen for muzzle flashes, billboards and the
//      radar. Those call sites need the same answer the RSP would have given.
//   3. **VR.** The eye projection substituted in fr.c has to feed the same
//      pipeline, or the two eyes disagree with the game's own idea of where
//      things are.
//
// So this is a reference implementation of the transform, matching libultra's
// conventions, used for culling and read-back — not for feeding the GPU.

#pragma once

#include <cstdint>

#include "gbi/gbi.h"

namespace ge_rhi {

// Row-vector convention throughout, matching libultra: v * M, translation in
// row 3. See src/xr_math.h for why this matters and how it goes wrong.
struct Mat4 {
    float m[4][4];
};

Mat4 identity();
Mat4 multiply(const Mat4& a, const Mat4& b);
Mat4 translate(float x, float y, float z);

// The N64 viewport. vscale/vtrans are in 2.2 fixed point — the game writes them
// as `viewx * 2` and `viewx * 2 + viewleft * 4` (src/fr.c:698-701), so the
// factor-of-four is already baked into the values the RSP sees.
struct Viewport {
    int16_t vscale[4] = {640, 480, 511, 0};
    int16_t vtrans[4] = {640, 480, 511, 0};
};

// Clip outcode bits, as the microcode computes them. The low byte is the
// near/far and screen-edge set; the high byte mirrors it for the second of the
// two vertices the RSP processes per iteration.
enum : uint16_t {
    kClipNegX = 0x0001,
    kClipPosX = 0x0002,
    kClipNegY = 0x0004,
    kClipPosY = 0x0008,
    kClipFar  = 0x0010,
    kClipNear = 0x0020,
};

struct TransformedVtx {
    float clip[4];       // after MVP, before the perspective divide
    float screen[3];     // after divide and viewport
    float inv_w;
    uint16_t outcode;
    bool behind_near;    // w <= 0: the divide is meaningless, treat separately
};

// Transform one vertex. `mvp` is modelview * projection already combined, the
// way the RSP keeps it.
//
// persp_norm is the gSPPerspNormalize value (src/fr.c:709). On hardware it
// scales W to keep the reciprocal in range for the RSP's limited-precision
// divide. Reproduced here so screen positions agree with hardware rather than
// being merely close.
TransformedVtx transformVertex(const ge_gbi::Vtx& v, const Mat4& mvp,
                               const Viewport& vp, uint16_t persp_norm);

// Cull test for a run of vertices, matching G_CULLDL: if every vertex shares an
// outcode bit, the whole run is off-screen on that side and the display list is
// skipped.
//
// The microcode ANDs the outcodes together and tests against 0x7030
// (gmain.s:277, :790). Reproduced rather than approximated, because a cull test
// that is merely "close" removes geometry the player can see.
bool shouldCull(const TransformedVtx* verts, int count);

// Trivial-reject for a single triangle. Same rule, three vertices.
bool triangleRejected(const TransformedVtx& a, const TransformedVtx& b,
                      const TransformedVtx& c);

}  // namespace ge_rhi
