// test_opt_panel.cpp — GEVR #76 Phase 2: empty hub panel + option registry.
//
// GETV_VR_OPT_PANEL unset/0 = OFF (this process unless ctest sets =1).
// No TURN_SCALE row is registered. Pose is cinema-right, not head-locked.

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

static void testRegistry() {
    std::printf("[registry shell — slider / toggle / enum]\n");
    geVrOptReset();
    check(geVrOptCount() == 0, "Phase 2 starts with zero rows");
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

    bool saw_turn = false;
    for (int i = 0; i < geVrOptCount(); ++i) {
        const char* id = geVrOptId(i);
        if (id && (std::strcmp(id, "turn_scale") == 0 ||
                   std::strcmp(id, "GETV_XR_TURN_SCALE") == 0)) {
            saw_turn = true;
        }
    }
    check(!saw_turn, "no TURN_SCALE row in Phase 2");

    geVrOptClear();
    check(geVrOptCount() == 0, "clear empties registry");
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
    check(geVrOptPanelRowAtUv(uv) == -1, "empty registry: no row under uv");

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
    ptrs[GE_VR_HAND_RIGHT].trigger = 0.9f;

    const int ev = geVrOptPanelEvaluate(1, ptrs);
    if (on) {
        check(ev == 1, "evaluate runs in hub when ON");
        check(geVrOptPanelHovered() == 1, "panel hovered by aim-ray");
        check(geVrOptPanelHoverIndex() == -1, "empty shell: hover is not a row");
        check(geVrOptPanelEvaluate(0, ptrs) == 1, "second eye keeps state");
        check(geVrOptPanelHovered() == 1, "no per-eye chatter");
    } else {
        check(ev == 0, "evaluate no-ops when OFF");
        check(geVrOptPanelHovered() == 0, "OFF: no hover");
    }

    /* Face A must not be the confirm — Evaluate has no button field. */
    ptrs[GE_VR_HAND_RIGHT].trigger = 0.0f;
    geVrOptPanelEvaluate(1, ptrs);
    ptrs[GE_VR_HAND_RIGHT].trigger = 0.9f;
    geVrOptPanelEvaluate(1, ptrs); /* rising edge on empty panel */
    check(geVrOptCount() == 0, "trigger on empty panel registers nothing");

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

    GeVrOptDesc en = {};
    en.id = "demo_enum";
    en.label = "Demo enum";
    en.kind = GE_VR_OPT_ENUM;
    en.enum_labels = kEnumLabels;
    en.enum_count = 2;
    en.get_i = get_enum;
    en.set_i = set_enum;
    check(geVrOptRegister(&en) == 0, "test-only enum (not a ship row)");
    check(geVrOptPanelGetRowRect(0, nullptr) == 0, "row rect needs out");
    GeVrOptRowRect rr{};
    check(geVrOptPanelGetRowRect(0, &rr) == 1, "tall row rect");
    check(rr.uv1[1] - rr.uv0[1] >= GE_VR_OPT_ROW_MIN_H - 1e-4f, "generous hitbox");
    check(rr.uv1[0] - rr.uv0[0] > 0.9f, "full-width row");

    check(geVrOptPanelDropdownOpen() == 0, "dropdown starts closed");
    geVrOptPanelOpenDropdown(0);
    check(geVrOptPanelDropdownOpen() == 1, "enum opens dropdown");

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
    geVrOptClear();
    geVrOptPanelCloseDropdown();
}

int main() {
    std::printf("[GETV_VR_OPT_PANEL %s]\n", panelOn() ? "ON" : "OFF (default)");

    testRegistry();
    testWorldLockRight();
    testRayVsCinema();
    testHubGateAndEvaluate();
    testChromeAndDropdown();

    if (g_failures) {
        std::printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    std::printf("\nall tests passed\n");
    return 0;
}
