// test_math.cpp — parity and sanity tests for the stereo projection math.
//
// The point of these tests is that projection bugs in VR do not look like bugs.
// A sheared frustum or a mismatched perspNorm produces an image that renders
// fine on a monitor and gives people headaches in a headset. That failure mode
// cannot be caught by looking at it, so it has to be caught here.

#include <cmath>
#include <cstdint>
#include <cstdio>

#include "ge_vr/ge_vr.h"
#include "xr_math.h"

using namespace ge_vr;

static int g_failures = 0;

static void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("  FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("  ok:   %s\n", what);
    }
}

static void checkNear(float a, float b, float eps, const char* what) {
    if (std::fabs(a - b) > eps) {
        std::printf("  FAIL: %s (%.6f vs %.6f, eps %.6f)\n", what, a, b, eps);
        ++g_failures;
    } else {
        std::printf("  ok:   %s\n", what);
    }
}

// ---------------------------------------------------------------------------
// 1. A symmetric FOV must reproduce guPerspectiveF exactly.
//
// This is the load-bearing test. If projectionFromFovF and guPerspectiveF
// disagree for a symmetric frustum, then every VR frame is subtly wrong
// relative to the flat-screen build and there is no way to A/B them.
// ---------------------------------------------------------------------------
static void testSymmetricParity() {
    std::printf("[symmetric parity vs guPerspectiveF]\n");

    // GoldenEye's actual near/far, from src/game/bondview2.c:8423.
    const float znear = 10.0f, zfar = 300.0f;
    const float fovy_deg = 60.0f;
    const float aspect = 1.4005603f;  // the NTSC value used in bondview2.c

    float ref[4][4];
    uint16_t ref_pn = 0;
    guPerspectiveF_ref(ref, &ref_pn, fovy_deg, aspect, znear, zfar, 1.0f);

    // Equivalent symmetric FOV in half-angles.
    const float half_v = fovy_deg * 3.1415926f / 180.0f * 0.5f;
    const float half_h = std::atan(std::tan(half_v) * aspect);
    Fov fov;
    fov.angleUp    =  half_v;
    fov.angleDown  = -half_v;
    fov.angleRight =  half_h;
    fov.angleLeft  = -half_h;

    float got[4][4];
    uint16_t got_pn = 0;
    projectionFromFovF(got, &got_pn, fov, znear, zfar, 1.0f);

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "m[%d][%d]", i, j);
            checkNear(got[i][j], ref[i][j], 1e-4f, buf);
        }
    }
    check(got_pn == ref_pn, "perspNorm matches guPerspectiveF");
}

// ---------------------------------------------------------------------------
// 2. Asymmetry must land in ROW 2, not column 2.
//
// The row-vs-column transcription error is the single most common bug when
// porting a projection into libultra's row-vector layout, and it produces a
// sheared image that reads as a tracking problem rather than a math problem.
// ---------------------------------------------------------------------------
static void testAsymmetryPlacement() {
    std::printf("[asymmetric frustum placement]\n");

    Fov fov;
    fov.angleLeft  = -0.9f;   // wider on the nasal side, as a real HMD is
    fov.angleRight =  0.7f;
    fov.angleUp    =  0.8f;
    fov.angleDown  = -0.85f;

    float m[4][4];
    projectionFromFovF(m, nullptr, fov, 10.0f, 300.0f, 1.0f);

    check(std::fabs(m[2][0]) > 1e-3f, "horizontal shear present in m[2][0]");
    check(std::fabs(m[2][1]) > 1e-3f, "vertical shear present in m[2][1]");
    checkNear(m[0][2], 0.0f, 1e-6f, "m[0][2] is zero (shear NOT in column 2)");
    checkNear(m[1][2], 0.0f, 1e-6f, "m[1][2] is zero (shear NOT in column 2)");
    checkNear(m[2][3], -1.0f, 1e-6f, "m[2][3] == -1 (libultra convention)");
    checkNear(m[3][3],  0.0f, 1e-6f, "m[3][3] == 0  (libultra convention)");
    check(m[3][2] != 0.0f, "depth term lives in m[3][2], not m[2][3]");

    // A symmetric FOV must produce zero shear — otherwise the "asymmetry" is
    // actually a constant offset bug.
    Fov sym;
    float ms[4][4];
    projectionFromFovF(ms, nullptr, sym, 10.0f, 300.0f, 1.0f);
    checkNear(ms[2][0], 0.0f, 1e-6f, "symmetric FOV has zero horizontal shear");
    checkNear(ms[2][1], 0.0f, 1e-6f, "symmetric FOV has zero vertical shear");
}

// ---------------------------------------------------------------------------
// 3. The union frustum must strictly enclose both eyes (culling correctness).
// ---------------------------------------------------------------------------
static void testUnionFov() {
    std::printf("[union frustum for culling]\n");
    Fov l; l.angleLeft = -1.0f; l.angleRight = 0.7f; l.angleUp = 0.8f;  l.angleDown = -0.9f;
    Fov r; r.angleLeft = -0.7f; r.angleRight = 1.0f; r.angleUp = 0.85f; r.angleDown = -0.8f;
    Fov u = unionFov(l, r);
    check(u.angleLeft  <= l.angleLeft  && u.angleLeft  <= r.angleLeft,  "encloses left");
    check(u.angleRight >= l.angleRight && u.angleRight >= r.angleRight, "encloses right");
    check(u.angleUp    >= l.angleUp    && u.angleUp    >= r.angleUp,    "encloses up");
    check(u.angleDown  <= l.angleDown  && u.angleDown  <= r.angleDown,  "encloses down");
}

// ---------------------------------------------------------------------------
// 4. Angle decomposition round-trips.
// ---------------------------------------------------------------------------
static void testAngles() {
    std::printf("[pose -> game angles]\n");

    // Identity: looking down -Z, level.
    float th = 9.0f, ve = 9.0f, ro = 9.0f;
    poseToGameAngles(Quat{}, &th, &ve, &ro);
    checkNear(th, 0.0f, 1e-5f, "identity yaw is zero");
    checkNear(ve, 0.0f, 1e-5f, "identity pitch is zero");

    // Yaw 90 degrees about +Y.
    const Quat yaw90 = quatFromAxisAngle(Vec3{0, 1, 0}, 1.5707963f);
    poseToGameAngles(yaw90, &th, &ve, nullptr);
    checkNear(std::fabs(th), 1.5707963f, 1e-4f, "90 degree yaw recovered");
    checkNear(ve, 0.0f, 1e-4f, "yaw does not leak into pitch");

    // Pitch 30 degrees up about +X.
    const Quat pitch30 = quatFromAxisAngle(Vec3{1, 0, 0}, 0.5235988f);
    poseToGameAngles(pitch30, &th, &ve, nullptr);
    checkNear(ve, 0.5235988f, 1e-4f, "30 degree pitch recovered");
    checkNear(th, 0.0f, 1e-4f, "pitch does not leak into yaw");
}

// ---------------------------------------------------------------------------
// 5. View matrix is the inverse of the model matrix.
// ---------------------------------------------------------------------------
static void testViewInverse() {
    std::printf("[view matrix is inverse of model matrix]\n");
    Pose p;
    p.orientation = quatFromAxisAngle(Vec3{0.3f, 1.0f, 0.2f}, 0.7f);
    p.position = Vec3{0.4f, 1.6f, -0.9f};

    const Mtx4 model = modelMatrixFromPose(p, GE_VR_UNITS_PER_METRE);
    const Mtx4 view  = viewMatrixFromPose(p, GE_VR_UNITS_PER_METRE);
    const Mtx4 prod  = mtxMul(model, view);

    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "model*view identity [%d][%d]", i, j);
            checkNear(prod.m[i][j], i == j ? 1.0f : 0.0f, 1e-3f, buf);
        }
}

// ---------------------------------------------------------------------------
// 6. perspNorm edge cases match libultra exactly.
// ---------------------------------------------------------------------------
static void testPerspNorm() {
    std::printf("[perspNorm parity]\n");
    struct { float n, f; } cases[] = {
        {10.0f, 300.0f},   // GoldenEye's actual values
        {0.5f,  1.0f},     // near+far <= 2 -> 0xFFFF
        {1.0f,  10000.0f}, // very large far
    };
    for (auto& c : cases) {
        uint16_t a = 0, b = 0;
        float m1[4][4], m2[4][4];
        guPerspectiveF_ref(m1, &a, 60.0f, 1.4f, c.n, c.f, 1.0f);
        Fov fov;
        projectionFromFovF(m2, &b, fov, c.n, c.f, 1.0f);
        char buf[96];
        std::snprintf(buf, sizeof(buf), "perspNorm near=%.1f far=%.1f (%u)", c.n, c.f, a);
        check(a == b, buf);
    }
}

int main() {
    testSymmetricParity();
    testAsymmetryPlacement();
    testUnionFov();
    testAngles();
    testViewInverse();
    testPerspNorm();

    if (g_failures) {
        std::printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    std::printf("\nall tests passed\n");
    return 0;
}
