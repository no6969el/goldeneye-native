#include "vertex_pipeline.h"

#include <cmath>
#include <cstring>

namespace ge_rhi {

Mat4 identity() {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) r.m[i][j] = (i == j) ? 1.0f : 0.0f;
    return r;
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a.m[i][k] * b.m[k][j];
            r.m[i][j] = s;
        }
    return r;
}

Mat4 translate(float x, float y, float z) {
    Mat4 r = identity();
    // Row-vector convention: translation lives in ROW 3, not column 3. Putting
    // it in the column transposes the whole transform and every room ends up in
    // the wrong place in a way that still looks like geometry.
    r.m[3][0] = x;
    r.m[3][1] = y;
    r.m[3][2] = z;
    return r;
}

TransformedVtx transformVertex(const ge_gbi::Vtx& v, const Mat4& mvp,
                               const Viewport& vp, uint16_t persp_norm) {
    TransformedVtx out{};

    const float pos[4] = {float(v.x), float(v.y), float(v.z), 1.0f};
    for (int j = 0; j < 4; ++j) {
        float s = 0.0f;
        for (int k = 0; k < 4; ++k) s += pos[k] * mvp.m[k][j];
        out.clip[j] = s;
    }

    const float w = out.clip[3];

    // Outcodes are computed in CLIP space, before the divide — that is what
    // makes them valid for vertices behind the eye, where the divide would flip
    // the sign and put an off-screen vertex apparently on-screen.
    uint16_t code = 0;
    if (out.clip[0] < -w) code |= kClipNegX;
    if (out.clip[0] >  w) code |= kClipPosX;
    if (out.clip[1] < -w) code |= kClipNegY;
    if (out.clip[1] >  w) code |= kClipPosY;
    if (out.clip[2] >  w) code |= kClipFar;
    if (out.clip[2] < -w) code |= kClipNear;
    out.outcode = code;

    out.behind_near = (w <= 0.0f);
    if (out.behind_near) {
        out.inv_w = 0.0f;
        out.screen[0] = out.screen[1] = out.screen[2] = 0.0f;
        return out;
    }

    // gSPPerspNormalize scales W so the RSP's reciprocal stays in range. On
    // hardware this is a precision device, not a geometric one — it must not
    // change where anything lands, only how accurately. Applying it to W and
    // then dividing reproduces the hardware's rounding behaviour without
    // altering the result.
    const float pn = persp_norm ? (float(persp_norm) / 65536.0f) : 1.0f;
    const float wn = w * pn;
    out.inv_w = (wn != 0.0f) ? (1.0f / wn) : 0.0f;

    const float ndc[3] = {out.clip[0] / w, out.clip[1] / w, out.clip[2] / w};

    // Viewport: scale and translate are in 2.2 fixed point, so divide by 4.
    // src/fr.c:698-701 writes them already multiplied, which is why the game's
    // values look four times too large if you expect plain pixels.
    for (int i = 0; i < 3; ++i)
        out.screen[i] = ndc[i] * (float(vp.vscale[i]) / 4.0f) +
                        (float(vp.vtrans[i]) / 4.0f);

    return out;
}

bool shouldCull(const TransformedVtx* verts, int count) {
    if (!verts || count <= 0) return false;
    uint16_t common = 0xFFFF;
    for (int i = 0; i < count; ++i) common &= verts[i].outcode;
    // Every vertex outside the same plane -> the whole run is off-screen.
    return common != 0;
}

bool triangleRejected(const TransformedVtx& a, const TransformedVtx& b,
                      const TransformedVtx& c) {
    return (a.outcode & b.outcode & c.outcode) != 0;
}

}  // namespace ge_rhi
