// test_opt_panel.cpp — GEVR #76 Phase 3–4: hub panel + ship rows.
//
// GETV_VR_OPT_PANEL unset/0 = OFF (this process unless ctest sets =1).
// Ship rows: TURN SPEED (TURN_SCALE), TURN STYLE (GETV_XR_TURN path), HEIGHT (FLOOR_M).
// Pose is cinema-right, not head-locked. Laser + trigger. No #74 knobs.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ge_vr/ge_vr.h"
#include "ge_vr/ge_vr_opt.h"

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

static bool panelOn() {
    const char* e = std::getenv("GETV_VR_OPT_PANEL");
    return e != nullptr && *e != '\0' && std::atoi(e) != 0;
}

static float g_slider = 60.0f;
static int g_toggle = 0;
static int g_enum = 0;

static float get_slider(void*) { return g_slider; }
static void set_slider(void*, float v) { g_slider = v; }
static int get_toggle(void*) { return g_toggle; }
static void set_toggle(void*, int v) { g_toggle = v; }
static int get_enum(void*) { return g_enum; }
static void set_enum(void*, int v) { g_enum = v; }

static const char* kEnumLabels[] = {"smooth", "snap"};

static void dirToward(const float from[3], const float to[3], float out[3]) {
    out[0] = to[0] - from[0];
    out[1] = to[1] - from[1];
    out[2] = to[2] - from[2];
    const float n = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (n > 1e-6f) {
        out[0] /= n;
        out[1] /= n;
        out[2] /= n;
    }
}

static int findRow(const char* id) {
    for (int i = 0; i < geVrOptCount(); ++i) {
        const char* s = geVrOptId(i);
        if (s && std::strcmp(s, id) == 0) return i;
    }
    return -1;
}

static void uvToWorld(const GeVrOptPanelQuad& q, float u, float v, float out[3]) {
    out[0] = q.center[0] + q.right[0] * (u - 0.5f) * q.width +
             q.up[0] * (v - 0.5f) * q.height;
    out[1] = q.center[1] + q.right[1] * (u - 0.5f) * q.width +
             q.up[1] * (v - 0.5f) * q.height;
    out[2] = q.center[2] + q.right[2] * (u - 0.5f) * q.width +
             q.up[2] * (v - 0.5f) * q.height;
}

static void testRegistry() {
    std::printf("[ship rows + registry shell]\n");
    geVrOptReset();
    check(geVrOptCount() == GE_VR_OPT_SHIP_ROWS, "Reset registers 3 ship rows");
    check(findRow(GE_VR_OPT_ID_TURN_SCALE) == 0, "row 0 is turn_scale");
    check(findRow(GE_VR_OPT_ID_TURN_MODE) == 1, "row 1 is turn_mode");
    check(findRow(GE_VR_OPT_ID_FLOOR_M) == 2, "row 2 is floor_m");
    check(std::strcmp(geVrOptLabel(0), "TURN SPEED") == 0, "label TURN SPEED");
    check(std::strcmp(geVrOptLabel(1), "TURN STYLE") == 0, "label TURN STYLE");
    check(std::strcmp(geVrOptLabel(2), "HEIGHT") == 0, "label HEIGHT");
    check(geVrTurnScaleGet() == GE_VR_TURN_SCALE_DEFAULT, "TURN_SCALE default 60");
    check(geVrTurnArmed() == 1, "GETV_XR_TURN stays armed");
    check(geVrTurnModeGet() == GE_VR_TURN_MODE_SMOOTH, "default SMOOTH");
    checkNear(geVrFloorMGet(), GE_VR_FLOOR_M_DEFAULT, 1e-3f, "FLOOR_M default -0.200");

    check(geVrOptNudge(0, +1) == 1, "turn speed +");
    check(geVrTurnScaleGet() == 70, "cache 60+10");
    check(geVrOptNudge(0, -1) == 1, "turn speed -");
    check(geVrTurnScaleGet() == 60, "cache back to 60");
    geVrTurnScaleSet(GE_VR_TURN_SCALE_MIN);
    check(geVrOptNudge(0, -1) == 0, "turn speed clamp at min");
    check(geVrTurnScaleGet() == GE_VR_TURN_SCALE_MIN, "stays at min");

    /* U-04: setter wins after first getenv latch. */
    geVrTurnScaleSet(45);
    check(geVrTurnScaleGet() == 45, "setter unlatches cache");
    geVrTurnScaleSet(90);
    check(geVrTurnScaleGet() == 90, "second set still lands");

    check(geVrOptNudge(1, +1) == 1, "turn style → SNAP");
    check(geVrTurnModeGet() == GE_VR_TURN_MODE_SNAP, "mode SNAP on TURN path");
    check(geVrTurnArmed() == 1, "SNAP does not flip GETV_XR_TURN to 0");
    check(geVrOptNudge(1, +1) == 1, "turn style wrap");
    check(geVrTurnModeGet() == GE_VR_TURN_MODE_SMOOTH, "mode SMOOTH again");

    float floor0 = geVrFloorMGet();
    check(geVrOptNudge(2, +1) == 1, "height +");
    check(geVrFloorMGet() > floor0 + 1e-4f, "FLOOR_M rose");
    check(geVrOptNudge(2, -1) == 1, "height -");
    checkNear(geVrFloorMGet(), floor0, 1e-3f, "FLOOR_M back");

    bool saw_invented = false;
    bool saw_74 = false;
    for (int i = 0; i < geVrOptCount(); ++i) {
        const char* id = geVrOptId(i);
        if (!id) continue;
        if (std::strcmp(id, "GETV_XR_SNAP") == 0 ||
            std::strcmp(id, "GE_VR_SNAP_TURN") == 0 ||
            std::strcmp(id, "GETV_XR_TURNSNAP") == 0 ||
            std::strcmp(id, "GETV_XR_TURNSPEED") == 0 ||
            std::strcmp(id, "GETV_XR_HEIGHT") == 0) {
            saw_invented = true;
        }
        if (std::strstr(id, "HEAD_TRANSLATE") || std::strstr(id, "PLAYSPACE") ||
            std::strstr(id, "GUNREBASE") || std::strstr(id, "FLOOR_INJECT")) {
            saw_74 = true;
        }
    }
    check(!saw_invented, "no invented snap/height getenv ids");
    check(!saw_74, "no #74 / FLOOR_INJECT rows");

    geVrOptClear();
    check(geVrOptCount() == 0, "clear empties registry");
    check(geVrOptId(0) == nullptr, "empty id is null");
    check(geVrOptRegister(nullptr) == -1, "reject null desc");

    GeVrOptDesc bad = {};
    bad.id = "x";
    bad.label = "X";
    bad.kind = (GeVrOptKind)99;
    check(geVrOptRegister(&bad) == -1, "reject unknown kind");

    g_slider = 60.0f;
    g_toggle = 0;
    g_enum = 0;

    GeVrOptDesc slider = {};
    slider.id = "demo_slider";
    slider.label = "Demo slider";
    slider.kind = GE_VR_OPT_SLIDER;
    slider.slider_min = 10.0f;
    slider.slider_max = 90.0f;
    slider.slider_step = 10.0f;
    slider.get_f = get_slider;
    slider.set_f = set_slider;
    check(geVrOptRegister(&slider) == 0, "register slider");

    GeVrOptDesc tog = {};
    tog.id = "demo_toggle";
    tog.label = "Demo toggle";
    tog.kind = GE_VR_OPT_TOGGLE;
    tog.get_i = get_toggle;
    tog.set_i = set_toggle;
    check(geVrOptRegister(&tog) == 1, "register toggle");

    GeVrOptDesc en = {};
    en.id = "demo_enum";
    en.label = "Demo enum";
    en.kind = GE_VR_OPT_ENUM;
    en.enum_labels = kEnumLabels;
    en.enum_count = 2;
    en.get_i = get_enum;
    en.set_i = set_enum;
    check(geVrOptRegister(&en) == 2, "register enum");
    check(geVrOptCount() == 3, "three demo rows");
    check(std::strcmp(geVrOptEnumLabel(2, 0), "smooth") == 0, "enum label 0");
    check(std::strcmp(geVrOptEnumLabel(2, 1), "snap") == 0, "enum label 1");

    check(geVrOptNudge(0, +1) == 1, "slider +");
    checkNear(g_slider, 70.0f, 1e-4f, "slider 60+10");
    check(geVrOptNudge(0, -1) == 1, "slider -");
    checkNear(g_slider, 60.0f, 1e-4f, "slider back to 60");
    g_slider = 10.0f;
    check(geVrOptNudge(0, -1) == 0, "slider clamp at min");
    checkNear(g_slider, 10.0f, 1e-4f, "slider stays 10");

    check(geVrOptNudge(1, +1) == 1, "toggle on");
    check(g_toggle == 1, "toggle is 1");
    check(geVrOptNudge(1, +1) == 1, "toggle off");
    check(g_toggle == 0, "toggle is 0");

    check(geVrOptNudge(2, +1) == 1, "enum wrap +");
    check(g_enum == 1, "enum snap");
    check(geVrOptNudge(2, +1) == 1, "enum wrap around");
    check(g_enum == 0, "enum smooth again");

    bool saw_ship_in_demos = false;
    for (int i = 0; i < geVrOptCount(); ++i) {
        const char* id = geVrOptId(i);
        if (id && std::strcmp(id, GE_VR_OPT_ID_TURN_SCALE) == 0)
            saw_ship_in_demos = true;
    }
    check(!saw_ship_in_demos, "demo rows are not the ship TURN_SCALE row");

    geVrOptClear();
    check(geVrOptCount() == 0, "clear empties demo registry");
}

static void testWorldLockRight() {
    std::printf("[world-locked RIGHT of cinema]\n");
    geVrOptReset();
    geVrOptPanelResetCinemaFrame();

    GeVrOptPanelQuad q0{}, q1{};
    geVrOptPanelComputeQuad(&q0);
    geVrOptPanelComputeQuad(&q1);
    checkNear(q0.center[0], q1.center[0], 1e-5f, "pose stable (not head-glued)");
    checkNear(q0.center[2], q1.center[2], 1e-5f, "same Z both reads");

    const float cinema_x = 0.0f;
    const float cinema_z = -GE_VR_OPT_CINEMA_DIST_UNITS;
    check(q0.center[0] > cinema_x + 10.0f, "panel is to wearer-right (+X)");
    checkNear(q0.center[2], cinema_z + GE_VR_OPT_PANEL_FORWARD_UNITS, 1e-3f,
              "slightly toward player, still cinema-hub world");
    checkNear(q0.normal[2], 1.0f, 1e-4f, "faces the player (cinema family)");
    checkNear(q0.width, GE_VR_OPT_PANEL_WIDTH_UNITS, 1e-4f, "panel width");

    /* Cinema facing +X: wearer-right is -Z, not world +X. */
    const float c[3] = {0.0f, 160.0f, -250.0f};
    const float right[3] = {0.0f, 0.0f, -1.0f};
    const float up[3] = {0.0f, 1.0f, 0.0f};
    const float nrm[3] = {1.0f, 0.0f, 0.0f};
    geVrOptPanelSetCinemaFrame(c, right, up, nrm, 260.0f, 146.0f);
    GeVrOptPanelQuad qr{};
    geVrOptPanelComputeQuad(&qr);
    checkNear(qr.center[0], c[0] + GE_VR_OPT_PANEL_FORWARD_UNITS, 1.0f,
              "rotated: +X is toward-player, not wearer-right");
    check(qr.center[2] < c[2] - 10.0f, "rotated: offset along cinema right (-Z)");
}

static void testRayVsCinema() {
    std::printf("[aim-ray: panel hits, cinema misses]\n");
    geVrOptReset();
    geVrOptPanelResetCinemaFrame();

    GeVrOptPanelQuad q{};
    geVrOptPanelComputeQuad(&q);

    const float eye[3] = {0.0f, GE_VR_OPT_CINEMA_CENTER_Y, 0.0f};
    const float at_cinema[3] = {0.0f, 0.0f, -1.0f};
    float t = 0.0f, uv[2] = {0, 0};
    check(geVrOptPanelRayHit(eye, at_cinema, &t, uv) == 0,
          "looking at cinema does not hit options");

    float at_panel[3];
    dirToward(eye, q.center, at_panel);
    check(geVrOptPanelRayHit(eye, at_panel, &t, uv) == 1, "aim at panel hits");
    check(uv[0] > 0.2f && uv[0] < 0.8f, "hit near panel center U");
    check(uv[1] > 0.2f && uv[1] < 0.8f, "hit near panel center V");
    check(geVrOptPanelRowAtUv(uv) >= 0, "ship rows: center hits a row");

    const float title_uv[2] = {0.5f, 0.95f};
    check(geVrOptPanelRowAtUv(title_uv) == -1, "header band is not a row");
    const float close_uv[2] = {0.95f, 0.95f};
    check(geVrOptPanelCloseAtUv(close_uv) == 1, "header-right is close");
    check(geVrOptPanelCloseAtUv(title_uv) == 0, "header-left is not close");
}

static void testHubGateAndEvaluate() {
    std::printf("[hub gate + first-eye evaluate]\n");
    geVrOptReset();
    geVrOptPanelResetCinemaFrame();
    geVrOptPanelSetHubActive(0);
    check(geVrOptPanelHubActive() == 0, "hub starts off");
    check(geVrOptPanelVisible() == 0, "not visible until hub + knob");
    check(geVrOptPanelGetQuad(nullptr) == 0, "GetQuad rejects without visible");

    GeVrOptPanelQuad q{};
    const int on = panelOn() ? 1 : 0;
    check(geVrOptPanelEnabled() == on, "enabled matches GETV_VR_OPT_PANEL");

    geVrOptPanelSetHubActive(1);
    if (on) {
        check(geVrOptPanelVisible() == 1, "ON + hub = visible (intro)");
        check(geVrOptPanelGetQuad(&q) == 1, "GetQuad ok in hub");
    } else {
        check(geVrOptPanelVisible() == 0, "default OFF: hidden even in hub");
        check(geVrOptPanelGetQuad(&q) == 0, "GetQuad hidden when OFF");
    }

    geVrOptPanelSetHubActive(0);
    check(geVrOptPanelVisible() == 0, "mission / gameplay: gone");

    geVrOptPanelSetHubActive(1);
    GeVrOptPanelQuad pose{};
    geVrOptPanelComputeQuad(&pose);
    const float eye[3] = {0.0f, GE_VR_OPT_CINEMA_CENTER_Y, 0.0f};
    GeVrOptPointer ptrs[GE_VR_HAND_COUNT]{};
    ptrs[GE_VR_HAND_RIGHT].tracked = 1;
    ptrs[GE_VR_HAND_RIGHT].origin[0] = eye[0];
    ptrs[GE_VR_HAND_RIGHT].origin[1] = eye[1];
    ptrs[GE_VR_HAND_RIGHT].origin[2] = eye[2];
    dirToward(eye, pose.center, ptrs[GE_VR_HAND_RIGHT].dir);
    ptrs[GE_VR_HAND_RIGHT].trigger = 0.0f;

    const int ev = geVrOptPanelEvaluate(1, ptrs);
    if (on) {
        check(ev == 1, "evaluate runs in hub when ON");
        check(geVrOptPanelHovered() == 1, "panel hovered by aim-ray");
        check(geVrOptPanelHoverIndex() >= 0, "ship row hover under aim-ray");
        check(geVrOptPanelEvaluate(0, ptrs) == 1, "second eye keeps state");
        check(geVrOptPanelHovered() == 1, "no per-eye chatter");
    } else {
        check(ev == 0, "evaluate no-ops when OFF");
        check(geVrOptPanelHovered() == 0, "OFF: no hover");
    }

    /* Face A must not be the confirm — Evaluate has no button field. */
    check(geVrOptCount() == GE_VR_OPT_SHIP_ROWS, "trigger path does not add extra rows");

    if (on) {
        GeVrOptLaser laser{};
        check(geVrOptPanelGetLaser(GE_VR_HAND_RIGHT, &laser) == 1,
              "laser from aim-ray while panel up");
        check(laser.on_panel == 1, "laser hits the glass");
        check(laser.tracked == 1, "laser tracked");
        checkNear(laser.hit[0], pose.center[0], 8.0f, "beam ends on panel");
    }
}

static void testChromeAndDropdown() {
    std::printf("[glass chrome + dropdown sibling]\n");
    geVrOptReset();
    check(std::strcmp(GE_VR_OPT_PANEL_TITLE, "VR SETTINGS") == 0,
          "header title is VR SETTINGS");

    GeVrOptChrome ch{};
    geVrOptPanelGetChrome(&ch);
    check(ch.glass_rgba[3] > 0.4f && ch.glass_rgba[3] < 1.0f, "translucent glass");
    checkNear(ch.text_rgb[0], 1.0f, 1e-4f, "bright white text");
    check(ch.pill_text_rgb[0] < 0.2f, "dark text on white pill");

    check(geVrOptPanelGetRowRect(0, nullptr) == 0, "row rect needs out");
    GeVrOptRowRect rr{};
    check(geVrOptPanelGetRowRect(0, &rr) == 1, "tall row rect");
    check(rr.uv1[1] - rr.uv0[1] >= GE_VR_OPT_ROW_MIN_H - 1e-4f, "generous hitbox");
    check(rr.uv1[0] - rr.uv0[0] > 0.9f, "full-width row");

    check(geVrOptPanelDropdownOpen() == 0, "dropdown starts closed");
    geVrOptPanelOpenDropdown(findRow(GE_VR_OPT_ID_TURN_MODE));
    check(geVrOptPanelDropdownOpen() == 1, "TURN STYLE opens dropdown");

    GeVrOptPanelQuad mainq{}, dropq{};
    geVrOptPanelComputeQuad(&mainq);
    check(geVrOptPanelGetDropdownQuad(&dropq) == 1, "dropdown pose");
    /* Sibling to the RIGHT: centers separated by more than half-widths + gap. */
    float dx = dropq.center[0] - mainq.center[0];
    float dz = dropq.center[2] - mainq.center[2];
    float sep = std::sqrt(dx * dx + dz * dz);
    float min_sep = mainq.width * 0.5f + dropq.width * 0.5f +
                    GE_VR_OPT_DROPDOWN_GAP_UNITS * 0.5f;
    check(sep >= min_sep - 1.0f, "dropdown does not overlap main glass");

    bool saw_demo_dump = false;
    for (int i = 0; i < geVrOptCount(); ++i) {
        const char* id = geVrOptId(i);
        if (id && (std::strstr(id, "hand") || std::strstr(id, "haptic") ||
                   std::strstr(id, "6dof") || std::strstr(id, "button_sens"))) {
            saw_demo_dump = true;
        }
    }
    check(!saw_demo_dump, "no Hand/6DoF/Haptic demo dump");
    geVrOptPanelCloseDropdown();
}

static void testPersistSidecar() {
    std::printf("[TURN_SCALE persist sidecar]\n");
    geVrOptReset();
    const char* path = "/tmp/gevr-opt-prefs-cfa7.cmd";
    std::remove(path);
    setenv("GETV_VR_OPT_PREFS", path, 1);
    geVrTurnScaleSet(40);
    geVrFloorMSet(-0.150f);

    FILE* f = std::fopen(path, "r");
    check(f != nullptr, "sidecar written");
    char buf[1024] = {};
    if (f) {
        const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        std::fclose(f);
    }
    check(std::strstr(buf, "GETV_XR_TURN_SCALE=40") != nullptr,
          "sidecar has TURN_SCALE=40");
    check(std::strstr(buf, "GETV_XR_FLOOR_M=") != nullptr, "sidecar has FLOOR_M");
    check(std::strstr(buf, "GETV_XR_TURN=1") != nullptr, "sidecar keeps TURN armed");
    check(std::strstr(buf, "GETV_XR_SNAP") == nullptr, "sidecar has no GETV_XR_SNAP");
    check(std::strstr(buf, "HEAD_TRANSLATE") == nullptr, "sidecar has no HT");
    std::remove(path);
}

static void testTriggerTurnSpeed() {
    std::printf("[laser + trigger nudges TURN_SCALE]\n");
    if (!panelOn()) {
        std::printf("  skip: GETV_VR_OPT_PANEL off\n");
        return;
    }
    geVrOptReset();
    geVrOptPanelResetCinemaFrame();
    geVrOptPanelSetHubActive(1);
    geVrTurnScaleSet(60);

    GeVrOptPanelQuad q{};
    geVrOptPanelComputeQuad(&q);
    GeVrOptRowRect rr{};
    check(geVrOptPanelGetRowRect(0, &rr) == 1, "turn speed row rect");
    const float u = 0.80f; /* right half = + */
    const float v = (rr.uv0[1] + rr.uv1[1]) * 0.5f;
    float target[3];
    uvToWorld(q, u, v, target);
    const float eye[3] = {0.0f, GE_VR_OPT_CINEMA_CENTER_Y, 0.0f};
    GeVrOptPointer ptrs[GE_VR_HAND_COUNT]{};
    ptrs[GE_VR_HAND_RIGHT].tracked = 1;
    ptrs[GE_VR_HAND_RIGHT].origin[0] = eye[0];
    ptrs[GE_VR_HAND_RIGHT].origin[1] = eye[1];
    ptrs[GE_VR_HAND_RIGHT].origin[2] = eye[2];
    dirToward(eye, target, ptrs[GE_VR_HAND_RIGHT].dir);
    ptrs[GE_VR_HAND_RIGHT].trigger = 0.0f;
    geVrOptPanelEvaluate(1, ptrs);
    check(geVrOptPanelHoverIndex() == 0, "laser hovers TURN SPEED");
    GeVrOptLaser laser{};
    check(geVrOptPanelGetLaser(GE_VR_HAND_RIGHT, &laser) == 1, "laser visible");
    check(laser.on_panel == 1, "laser on glass");

    ptrs[GE_VR_HAND_RIGHT].trigger = 0.9f;
    geVrOptPanelEvaluate(1, ptrs);
    check(geVrTurnScaleGet() == 70, "trigger + raises TURN_SCALE this process");
}

int main() {
    setenv("GETV_VR_OPT_PREFS", "/tmp/gevr-opt-prefs-cfa7.cmd", 1);
    std::printf("[GETV_VR_OPT_PANEL %s]\n", panelOn() ? "ON" : "OFF (default)");

    testRegistry();
    testWorldLockRight();
    testRayVsCinema();
    testHubGateAndEvaluate();
    testChromeAndDropdown();
    testPersistSidecar();
    testTriggerTurnSpeed();

    if (g_failures) {
        std::printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    std::printf("\nall tests passed\n");
    return 0;
}
