// xr_math.h — pose/matrix math bridging OpenXR conventions to libultra conventions.
//
// Two coordinate systems are in play and conflating them is the classic way to
// spend a week on "why is the world mirrored":
//
//   OpenXR : right-handed, +X right, +Y up, -Z forward. Metres.
//   N64/gu : row-vector convention (v * M), translation lives in ROW 3, and
//            guPerspectiveF writes mf[2][3] = -1 with the depth term in mf[3][2].
//            Game units (see GE_VR_UNITS_PER_METRE).
//
// Everything in this file produces libultra-layout matrices so the output can be
// handed straight to guMtxF2L() and gSPMatrix().

#pragma once

#include <cmath>
#include <cstdint>

namespace ge_vr {

constexpr int kEyeCount = 2;
constexpr int kHandCount = 2;

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Quat {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
};

struct Pose {
    Quat orientation;
    Vec3 position;
};

// Per-eye field of view, four independent half-angles in radians.
// Mirrors XrFovf exactly. angleLeft and angleDown are normally negative.
struct Fov {
    float angleLeft   = -0.7853982f;
    float angleRight  =  0.7853982f;
    float angleUp     =  0.7853982f;
    float angleDown   = -0.7853982f;
};

// libultra-layout 4x4. Indexed [row][col].
struct Mtx4 {
    float m[4][4];
};

Mtx4 identity();

// ---------------------------------------------------------------------------
// Projection
// ---------------------------------------------------------------------------

// Reference reimplementation of libultra guPerspectiveF, used as the parity
// oracle in tests and as the fallback when VR is inactive.
//   src/libultra/gu/perspective.c in n64decomp/007.
void guPerspectiveF_ref(float mf[4][4], uint16_t* perspNorm, float fovy_degrees,
                        float aspect, float near_, float far_, float scale);

// Asymmetric-frustum projection in libultra layout.
//
// This is the core of stereo. guPerspective builds a symmetric frustum from one
// fovy; an HMD needs four independent half-angles, different per eye. Getting
// this wrong does not look "slightly off" — mismatched frusta between eyes are
// the thing that gives people headaches within a minute.
//
// perspNorm is computed by the exact same rule as guPerspectiveF, because it
// feeds the RSP's W-divide precision and the game's Z range assumptions are
// tuned around it.
void projectionFromFovF(float mf[4][4], uint16_t* perspNorm, const Fov& fov,
                        float near_, float far_, float scale);

// A symmetric frustum that strictly encloses both eye frusta. Used for culling
// so that geometry never pops in one eye only (architecture doc §5.3).
Fov unionFov(const Fov& left, const Fov& right);

// ---------------------------------------------------------------------------
// Poses
// ---------------------------------------------------------------------------

Quat  quatMul(const Quat& a, const Quat& b);
Quat  quatConjugate(const Quat& q);
Vec3  quatRotate(const Quat& q, const Vec3& v);
Quat  quatFromAxisAngle(const Vec3& axis, float radians);

// Decompose an orientation into the game's angle convention.
//   theta = yaw   (bondview.h vv_theta)
//   verta = pitch (bondview.h vv_verta)
//   roll  = roll  — the N64 camera has no roll term, so this is applied in the
//                   eye view matrix by the bridge rather than written to the
//                   player struct. Discarding it entirely is very uncomfortable.
void poseToGameAngles(const Quat& q, float* theta, float* verta, float* roll);

// OpenXR pose -> libultra-layout VIEW matrix (i.e. the inverse of the pose),
// with position converted from metres to game units.
Mtx4 viewMatrixFromPose(const Pose& pose, float units_per_metre);

// OpenXR pose -> libultra-layout MODEL matrix, for drawing a weapon at a
// controller in world space.
Mtx4 modelMatrixFromPose(const Pose& pose, float units_per_metre);

// Forward direction (-Z in OpenXR) rotated into world space. Used to build the
// hitscan ray in gunfire.c.
Vec3 poseForward(const Quat& q);

Mtx4 mtxMul(const Mtx4& a, const Mtx4& b);

// Metres -> game units, with the OpenXR -> game axis convention applied in one
// place so no caller has to remember it.
Vec3 xrToGame(const Vec3& v, float units_per_metre);

}  // namespace ge_vr
