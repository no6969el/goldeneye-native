// test_gunrebase.cpp — GETV_VR_GUNREBASE (#74) host ABI.
//
// Default OFF = raw stage grip/aim (tonight). ON = same recenter yaw+XZ as
// the head, then head-relative XZ, so a room sidestep does not walk the fist.
// Drive with ctest ENVIRONMENT; the getenv is cached for the process.

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "ge_vr/ge_vr.h"
#include "xr_input.h"
#include "xr_math.h"
#include "xr_session.h"

using namespace ge_vr;

namespace ge_vr {
void bridgeBeginFrame(const FrameState& fs, const InputState& in);
}

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

static bool gunrebaseOn() {
    const char* e = std::getenv("GETV_VR_GUNREBASE");
    return e != nullptr && *e != '\0' && std::atoi(e) != 0;
}

static Pose poseAt(float x, float y, float z) {
    Pose p;
    p.orientation = Quat{0, 0, 0, 1};
    p.position = Vec3{x, y, z};
    return p;
}

static void fillFrame(FrameState* fs, InputState* in, float sidestep_m) {
    *fs = FrameState{};
    *in = InputState{};
    fs->head_valid = true;
    fs->head = poseAt(sidestep_m, 1.6f, 0.0f);
    for (int e = 0; e < kEyeCount; ++e) {
        fs->eye[e].valid = true;
        fs->eye[e].pose = fs->head;
        fs->eye[e].pose.position.x += (e == 0 ? -0.032f : 0.032f);
    }
    for (int h = 0; h < kHandCount; ++h) {
        const float hand_x = sidestep_m + (h == 0 ? -0.25f : 0.25f);
        in->hand[h].tracked = true;
        in->hand[h].grip = poseAt(hand_x, 1.2f, -0.30f);
        in->hand[h].aim = poseAt(hand_x, 1.25f, -0.40f);
    }
}

static void translationOf(const float mf[4][4], float out[3]) {
    out[0] = mf[3][0];
    out[1] = mf[3][1];
    out[2] = mf[3][2];
}

int main() {
    const bool on = gunrebaseOn();
    std::printf("[GETV_VR_GUNREBASE %s]\n", on ? "ON" : "OFF (default)");

    FrameState fs;
    InputState in;
    fillFrame(&fs, &in, 0.0f);
    bridgeBeginFrame(fs, in);
    geVrRecenter();

    check(geVrHandIsTracked(GE_VR_HAND_RIGHT) != 0, "right hand tracked");
    check(geVrHandIsTracked(GE_VR_HAND_LEFT) != 0, "left hand tracked");

    float before_r[4][4]{}, before_l[4][4]{};
    check(geVrGetWeaponModelMatrixF(GE_VR_HAND_RIGHT, before_r) != 0, "right grip mtx");
    check(geVrGetWeaponModelMatrixF(GE_VR_HAND_LEFT, before_l) != 0, "left grip mtx");
    float br[3], bl[3];
    translationOf(before_r, br);
    translationOf(before_l, bl);

    float aim0[3], dir0[3];
    geVrGetAimRay(GE_VR_HAND_RIGHT, aim0, dir0);

    // Physical room sidestep, no stick: head + both fists translate together.
    fillFrame(&fs, &in, 0.80f);
    bridgeBeginFrame(fs, in);

    float after_r[4][4]{}, after_l[4][4]{};
    geVrGetWeaponModelMatrixF(GE_VR_HAND_RIGHT, after_r);
    geVrGetWeaponModelMatrixF(GE_VR_HAND_LEFT, after_l);
    float ar[3], al[3];
    translationOf(after_r, ar);
    translationOf(after_l, al);

    float aim1[3], dir1[3];
    geVrGetAimRay(GE_VR_HAND_RIGHT, aim1, dir1);

    const float step_units = 0.80f * GE_VR_UNITS_PER_METRE;

    if (on) {
        checkNear(ar[0], br[0], 1e-3f, "right grip X locked after sidestep");
        checkNear(ar[2], br[2], 1e-3f, "right grip Z locked after sidestep");
        checkNear(al[0], bl[0], 1e-3f, "left cube X locked after sidestep");
        checkNear(al[2], bl[2], 1e-3f, "left cube Z locked after sidestep");
        checkNear(aim1[0], aim0[0], 1e-3f, "aim origin X locked (same yaw as grip)");
        checkNear(aim1[2], aim0[2], 1e-3f, "aim origin Z locked");
        checkNear(ar[1], br[1], 1e-3f, "right grip Y kept");
        checkNear(dir1[0], dir0[0], 1e-3f, "aim dir X unchanged (identity yaw)");
        checkNear(dir1[2], dir0[2], 1e-3f, "aim dir Z unchanged");
    } else {
        checkNear(ar[0], br[0] + step_units, 1e-3f, "OFF: right grip walks with stage X");
        checkNear(al[0], bl[0] + step_units, 1e-3f, "OFF: left cube walks with stage X");
        checkNear(aim1[0], aim0[0] + step_units, 1e-3f, "OFF: aim origin walks with stage X");
        checkNear(ar[1], br[1], 1e-3f, "OFF: grip Y unchanged");
    }

    // HEADYAW / camera path still recenters the head (not flipped by this knob).
    float hp[3];
    geVrGetHeadPosition(hp);
    if (on) {
        checkNear(hp[0], step_units, 1e-3f, "head X still follows room (HT term live)");
    } else {
        checkNear(hp[0], step_units, 1e-3f, "head X follows room when OFF too");
    }

    if (g_failures) {
        std::printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    std::printf("\nall tests passed\n");
    return 0;
}
