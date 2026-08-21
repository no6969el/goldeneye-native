#include "xr_math.h"

#include <algorithm>

namespace ge_vr {

Mtx4 identity() {
    Mtx4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) r.m[i][j] = (i == j) ? 1.0f : 0.0f;
    return r;
}

// --- perspNorm -------------------------------------------------------------
// Verbatim from src/libultra/gu/perspective.c. Reproduced rather than
// approximated: gSPPerspNormalize feeds the RSP W-divide and the game's near/far
// (10.0 / 300.0 in bondview2.c:8423) are tuned against this exact curve.
static uint16_t perspNormFor(float near_, float far_) {
    if (near_ + far_ <= 2.0f) return 0xFFFFu;
    uint16_t pn = static_cast<uint16_t>((2.0 * 65536.0) / (near_ + far_));
    if (pn == 0) pn = 1;
    return pn;
}

void guPerspectiveF_ref(float mf[4][4], uint16_t* perspNorm, float fovy_degrees,
                        float aspect, float near_, float far_, float scale) {
    Mtx4 id = identity();
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) mf[i][j] = id.m[i][j];

    const float fovy = fovy_degrees * 3.1415926f / 180.0f;
    const float cot = std::cos(fovy / 2.0f) / std::sin(fovy / 2.0f);

    mf[0][0] = cot / aspect;
    mf[1][1] = cot;
    mf[2][2] = (near_ + far_) / (near_ - far_);
    mf[2][3] = -1.0f;
    mf[3][2] = (2.0f * near_ * far_) / (near_ - far_);
    mf[3][3] = 0.0f;

    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) mf[i][j] *= scale;

    if (perspNorm) *perspNorm = perspNormFor(near_, far_);
}

void projectionFromFovF(float mf[4][4], uint16_t* perspNorm, const Fov& fov,
                        float near_, float far_, float scale) {
    // Frustum extents at the near plane. tan() of each half-angle; left/down are
    // negative angles, so l and b come out negative for a centred eye and
    // asymmetric — which is the whole point — for a real HMD.
    const float l = near_ * std::tan(fov.angleLeft);
    const float r = near_ * std::tan(fov.angleRight);
    const float b = near_ * std::tan(fov.angleDown);
    const float t = near_ * std::tan(fov.angleUp);

    const float rl = r - l;
    const float tb = t - b;

    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) mf[i][j] = 0.0f;

    mf[0][0] = (2.0f * near_) / rl;
    mf[1][1] = (2.0f * near_) / tb;

    // Row-vector convention: the frustum-centre shear lives in row 2, NOT in
    // column 2 as it would in the OpenGL/column-vector form. This is the single
    // most common transcription bug when porting a projection into libultra
    // layout — it produces an image that looks almost right and is subtly
    // sheared, which readers of the code will blame on tracking.
    mf[2][0] = (r + l) / rl;
    mf[2][1] = (t + b) / tb;
    mf[2][2] = (near_ + far_) / (near_ - far_);
    mf[2][3] = -1.0f;

    mf[3][2] = (2.0f * near_ * far_) / (near_ - far_);
    mf[3][3] = 0.0f;

    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) mf[i][j] *= scale;

    if (perspNorm) *perspNorm = perspNormFor(near_, far_);
}

Fov unionFov(const Fov& a, const Fov& b) {
    Fov u;
    u.angleLeft  = std::min(a.angleLeft,  b.angleLeft);
    u.angleRight = std::max(a.angleRight, b.angleRight);
    u.angleDown  = std::min(a.angleDown,  b.angleDown);
    u.angleUp    = std::max(a.angleUp,    b.angleUp);
    return u;
}

// --- quaternions -----------------------------------------------------------

Quat quatMul(const Quat& a, const Quat& b) {
    return Quat{
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

Quat quatConjugate(const Quat& q) { return Quat{-q.x, -q.y, -q.z, q.w}; }

Vec3 quatRotate(const Quat& q, const Vec3& v) {
    // v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)
    const float tx = 2.0f * (q.y * v.z - q.z * v.y);
    const float ty = 2.0f * (q.z * v.x - q.x * v.z);
    const float tz = 2.0f * (q.x * v.y - q.y * v.x);
    return Vec3{
        v.x + q.w * tx + (q.y * tz - q.z * ty),
        v.y + q.w * ty + (q.z * tx - q.x * tz),
        v.z + q.w * tz + (q.x * ty - q.y * tx),
    };
}

Quat quatFromAxisAngle(const Vec3& axis, float radians) {
    const float len = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (len < 1e-8f) return Quat{};
    const float s = std::sin(radians * 0.5f) / len;
    return Quat{axis.x * s, axis.y * s, axis.z * s, std::cos(radians * 0.5f)};
}

Vec3 poseForward(const Quat& q) {
    return quatRotate(q, Vec3{0.0f, 0.0f, -1.0f});  // OpenXR forward is -Z
}

void poseToGameAngles(const Quat& q, float* theta, float* verta, float* roll) {
    const Vec3 fwd = poseForward(q);
    const Vec3 up  = quatRotate(q, Vec3{0.0f, 1.0f, 0.0f});

    // Yaw about the world up axis. atan2 over the horizontal projection of
    // forward. Note the argument order: the game's theta measures from +Z, not
    // from +X, matching bondview's sin/cos usage (bondview.h theta_transform:
    // "f[0]: forward component (sin theta) ... f[2]: sideways (cos theta)").
    if (theta) *theta = std::atan2(fwd.x, -fwd.z);

    // Pitch: elevation of forward above the horizontal plane.
    const float horiz = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z);
    if (verta) *verta = std::atan2(fwd.y, horiz);

    // Roll: rotation of the up vector about the forward axis. The N64 camera has
    // no roll concept, so this never reaches the player struct — the bridge folds
    // it into the eye view matrix instead. Dropping it makes head tilt feel like
    // the world is fighting you.
    if (roll) {
        const Vec3 right = quatRotate(q, Vec3{1.0f, 0.0f, 0.0f});
        // Reference "up with zero roll" = up-axis component orthogonal to fwd.
        *roll = std::atan2(right.y, up.y);
    }
}

Vec3 xrToGame(const Vec3& v, float units_per_metre) {
    // Axis mapping is identity here and scale-only; if profiling against the
    // real game reveals a handedness flip (it may — GE's world axes are not
    // documented in the decomp headers), invert it in THIS function and nowhere
    // else. TODO(phase5): confirm against a known level geometry landmark.
    return Vec3{v.x * units_per_metre, v.y * units_per_metre, v.z * units_per_metre};
}

static Mtx4 rotationMatrix(const Quat& q) {
    Mtx4 r = identity();
    const Vec3 x = quatRotate(q, Vec3{1, 0, 0});
    const Vec3 y = quatRotate(q, Vec3{0, 1, 0});
    const Vec3 z = quatRotate(q, Vec3{0, 0, 1});
    // Row-vector convention: basis vectors go in rows.
    r.m[0][0] = x.x; r.m[0][1] = x.y; r.m[0][2] = x.z;
    r.m[1][0] = y.x; r.m[1][1] = y.y; r.m[1][2] = y.z;
    r.m[2][0] = z.x; r.m[2][1] = z.y; r.m[2][2] = z.z;
    return r;
}

Mtx4 mtxMul(const Mtx4& a, const Mtx4& b) {
    Mtx4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a.m[i][k] * b.m[k][j];
            r.m[i][j] = s;
        }
    return r;
}

Mtx4 modelMatrixFromPose(const Pose& pose, float units_per_metre) {
    Mtx4 r = rotationMatrix(pose.orientation);
    const Vec3 p = xrToGame(pose.position, units_per_metre);
    r.m[3][0] = p.x;
    r.m[3][1] = p.y;
    r.m[3][2] = p.z;
    return r;
}

Mtx4 viewMatrixFromPose(const Pose& pose, float units_per_metre) {
    // View = inverse(model). For a rigid transform in row-vector layout that is
    // transpose(R) with translation = -p * transpose(R).
    const Quat inv = quatConjugate(pose.orientation);
    Mtx4 r = rotationMatrix(inv);
    const Vec3 p = xrToGame(pose.position, units_per_metre);
    const Vec3 t = quatRotate(inv, Vec3{-p.x, -p.y, -p.z});
    r.m[3][0] = t.x;
    r.m[3][1] = t.y;
    r.m[3][2] = t.z;
    return r;
}

}  // namespace ge_vr
