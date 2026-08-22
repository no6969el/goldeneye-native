// ge_vr_bridge.cpp — implements the C ABI in include/ge_vr/ge_vr.h.
//
// This is the only place where OpenXR concepts and GoldenEye concepts meet.
// Game code sees plain floats; the XR layer sees poses. Everything in between —
// unit conversion, handedness, recentre offset, comfort clamping, tracking-loss
// fallback — lives here so there is exactly one place to be wrong.

#include "ge_vr/ge_vr.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

#include "xr_input.h"
#include "xr_math.h"
#include "xr_session.h"

namespace {

using namespace ge_vr;

struct BridgeState {
    std::mutex mu;

    bool active = false;
    int  current_eye = 0;

    FrameState frame;
    InputState input;

    // --- recentre ---
    // Applied as a yaw rotation + position offset rather than by recreating the
    // reference space: cheaper, instantaneous, and works on runtimes without
    // XR_EXT_local_floor.
    float recenter_yaw = 0.0f;
    Vec3  recenter_pos{};

    // --- calibration ---
    float standing_height_m = 1.7f;   // set by geVrRecenter()
    bool  height_calibrated = false;

    // --- comfort ---
    float positional_clamp_units = 45.0f;  // ~0.45 m of lean before greyout
    float comfort_fade = 0.0f;

    // --- tracking-loss latch ---
    // When a controller drops out mid-fight, aim must fall back to view-relative
    // rather than to whatever the last stale pose was. A stale pose pointing at
    // the floor while the player is being shot at is worse than no VR aiming.
    bool hand_tracked[kHandCount] = {false, false};
};

BridgeState& S() {
    static BridgeState s;
    return s;
}

// Head pose with recentre applied.
Pose recenteredHead(const BridgeState& s) {
    Pose p = s.frame.head;
    const Quat yaw = quatFromAxisAngle(Vec3{0, 1, 0}, -s.recenter_yaw);
    p.orientation = quatMul(yaw, p.orientation);
    Vec3 d{p.position.x - s.recenter_pos.x,
           p.position.y - s.recenter_pos.y,
           p.position.z - s.recenter_pos.z};
    p.position = quatRotate(yaw, d);
    return p;
}

}  // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

extern "C" int geVrInit(void) {
    // Session/backend construction is owned by the port's main(); this entry
    // point exists so game code can be linked and tested without one.
    // TODO(phase4): wire to Session::init once the RT64-backed IGraphicsBackend
    // exists. Until then the bridge runs inactive and every getter is neutral.
    return 1;
}

extern "C" void geVrShutdown(void) {
    std::lock_guard<std::mutex> lk(S().mu);
    S().active = false;
}

extern "C" int geVrIsActive(void) { return S().active ? 1 : 0; }

extern "C" GeVrEye geVrCurrentEye(void) {
    return static_cast<GeVrEye>(S().current_eye);
}

// Called by the frame loop, not by game code.
namespace ge_vr {

void bridgeBeginFrame(const FrameState& fs, const InputState& in) {
    auto& s = S();
    std::lock_guard<std::mutex> lk(s.mu);
    s.frame = fs;
    s.input = in;
    s.active = fs.eye[0].valid && fs.eye[1].valid;
    for (int h = 0; h < kHandCount; ++h) s.hand_tracked[h] = in.hand[h].tracked;

    // Comfort greyout ramps as the head approaches the lean limit rather than
    // snapping at it. A hard cut is more jarring than the lean it prevents.
    const Pose head = recenteredHead(s);
    const Vec3 hp = xrToGame(head.position, GE_VR_UNITS_PER_METRE);
    const float lateral = std::sqrt(hp.x * hp.x + hp.z * hp.z);
    const float t = s.positional_clamp_units > 0.0f
                        ? lateral / s.positional_clamp_units
                        : 0.0f;
    s.comfort_fade = std::clamp((t - 0.7f) / 0.3f, 0.0f, 1.0f);
}

void bridgeSetEye(int eye) { S().current_eye = eye; }

}  // namespace ge_vr

// ---------------------------------------------------------------------------
// Projection
// ---------------------------------------------------------------------------

extern "C" int geVrBuildProjectionF(float mf[4][4], unsigned short* perspNorm,
                                    float znear, float zfar, float scale) {
    auto& s = S();
    std::lock_guard<std::mutex> lk(s.mu);
    if (!s.active) return 0;

    const int eye = std::clamp(s.current_eye, 0, kEyeCount - 1);
    Fov fov = s.frame.eye[eye].fov;

    // GE_VR_EYE_UNION asks for the enclosing frustum: used by the culling pass so
    // geometry can't pop in one eye only (architecture doc §5.3).
    if (s.current_eye == GE_VR_EYE_UNION)
        fov = unionFov(s.frame.eye[0].fov, s.frame.eye[1].fov);

    // The game's world near plane is per-level data, not a constant: it is
    // Visibility.BlendMultiplier from fog_tables[] (src/game/bgfog.c:301),
    // reaching viSetZRange -> g_ViBackData->znear -> guPerspectiveF at
    // src/fr.c:709. Across the level table it runs 2..30 units, and 2 units is
    // 2 cm. Harmless on the N64's W-buffer; in stereo it throws away depth
    // precision and sits inside the headset's own comfort floor. Clamp it here
    // rather than in the host, so every host gets the same divergence.
    // zfar is passed through unchanged — it is per-level and legitimately so.
    if (znear < GE_VR_MIN_ZNEAR_UNITS) znear = GE_VR_MIN_ZNEAR_UNITS;

    uint16_t pn = 0;
    projectionFromFovF(mf, &pn, fov, znear, zfar, scale);
    if (perspNorm) *perspNorm = pn;
    return 1;
}

extern "C" int geVrGetEyeViewOffsetF(float mf[4][4]) {
    auto& s = S();
    std::lock_guard<std::mutex> lk(s.mu);
    if (!s.active) return 0;

    const int eye = std::clamp(s.current_eye, 0, kEyeCount - 1);

    // The eye pose relative to the head. The game's own guLookAt already put us
    // at the player's eye height looking down vv_theta/vv_verta; what it cannot
    // know is IPD, head roll, and the positional lean. That difference is
    // exactly head^-1 * eye, and it is what goes here.
    const Pose head = recenteredHead(s);
    const Pose eye_pose = s.frame.eye[eye].pose;

    const Quat inv_head = quatConjugate(head.orientation);
    Pose rel;
    rel.orientation = quatMul(inv_head, eye_pose.orientation);
    rel.position = quatRotate(inv_head, Vec3{eye_pose.position.x - head.position.x,
                                             eye_pose.position.y - head.position.y,
                                             eye_pose.position.z - head.position.z});

    // Positional lean, clamped. NOTE: this moves the camera only. It must never
    // reach the collision capsule — see architecture doc §6.2.
    Vec3 lean = xrToGame(head.position, GE_VR_UNITS_PER_METRE);
    const float lateral = std::sqrt(lean.x * lean.x + lean.z * lean.z);
    if (lateral > s.positional_clamp_units && lateral > 0.0f) {
        const float k = s.positional_clamp_units / lateral;
        lean.x *= k;
        lean.z *= k;
    }
    rel.position.x += lean.x / GE_VR_UNITS_PER_METRE;
    rel.position.z += lean.z / GE_VR_UNITS_PER_METRE;

    const Mtx4 m = viewMatrixFromPose(rel, GE_VR_UNITS_PER_METRE);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) mf[i][j] = m.m[i][j];
    return 1;
}

// ---------------------------------------------------------------------------
// Head
// ---------------------------------------------------------------------------

extern "C" void geVrGetHeadAngles(float* theta, float* verta, float* roll) {
    auto& s = S();
    std::lock_guard<std::mutex> lk(s.mu);
    if (theta) *theta = 0.0f;
    if (verta) *verta = 0.0f;
    if (roll)  *roll  = 0.0f;
    if (!s.active || !s.frame.head_valid) return;
    poseToGameAngles(recenteredHead(s).orientation, theta, verta, roll);
}

extern "C" void geVrGetHeadPosition(float out[3]) {
    auto& s = S();
    std::lock_guard<std::mutex> lk(s.mu);
    out[0] = out[1] = out[2] = 0.0f;
    if (!s.active || !s.frame.head_valid) return;
    const Vec3 p = xrToGame(recenteredHead(s).position, GE_VR_UNITS_PER_METRE);
    out[0] = p.x; out[1] = p.y; out[2] = p.z;
}

extern "C" float geVrGetPositionalClamp(void) { return S().positional_clamp_units; }
extern "C" float geVrGetComfortFade(void)     { return S().comfort_fade; }

// ---------------------------------------------------------------------------
// Hands
// ---------------------------------------------------------------------------

extern "C" int geVrHandIsTracked(GeVrHand hand) {
    const int h = std::clamp(int(hand), 0, kHandCount - 1);
    return S().hand_tracked[h] ? 1 : 0;
}

extern "C" void geVrGetAimRay(GeVrHand hand, float origin[3], float dir[3]) {
    auto& s = S();
    std::lock_guard<std::mutex> lk(s.mu);
    const int h = std::clamp(int(hand), 0, kHandCount - 1);

    // Fallback: view ray. Tracking loss must degrade to the ORIGINAL behaviour.
    origin[0] = origin[1] = origin[2] = 0.0f;
    dir[0] = 0.0f; dir[1] = 0.0f; dir[2] = -1.0f;
    if (!s.active || !s.hand_tracked[h]) return;

    const Pose aim = s.input.hand[h].aim;
    const Vec3 o = xrToGame(aim.position, GE_VR_UNITS_PER_METRE);
    const Vec3 d = poseForward(aim.orientation);
    origin[0] = o.x; origin[1] = o.y; origin[2] = o.z;
    dir[0] = d.x; dir[1] = d.y; dir[2] = d.z;
}

extern "C" void geVrGetWeaponDisplacement(GeVrHand hand, float* dtheta, float* dverta) {
    auto& s = S();
    std::lock_guard<std::mutex> lk(s.mu);
    if (dtheta) *dtheta = 0.0f;
    if (dverta) *dverta = 0.0f;
    const int h = std::clamp(int(hand), 0, kHandCount - 1);
    if (!s.active || !s.hand_tracked[h] || !s.frame.head_valid) return;

    float head_theta = 0.0f, head_verta = 0.0f;
    poseToGameAngles(recenteredHead(s).orientation, &head_theta, &head_verta, nullptr);

    float aim_theta = 0.0f, aim_verta = 0.0f;
    poseToGameAngles(s.input.hand[h].aim.orientation, &aim_theta, &aim_verta, nullptr);

    // Wrap into (-pi, pi]. Without this the weapon snaps 360 degrees whenever the
    // player aims across the yaw seam — visually spectacular, entirely wrong.
    float d = aim_theta - head_theta;
    while (d >  3.14159265f) d -= 6.28318531f;
    while (d < -3.14159265f) d += 6.28318531f;

    if (dtheta) *dtheta = d;
    if (dverta) *dverta = aim_verta - head_verta;
}

extern "C" int geVrGetWeaponModelMatrixF(GeVrHand hand, float mf[4][4]) {
    auto& s = S();
    std::lock_guard<std::mutex> lk(s.mu);
    const int h = std::clamp(int(hand), 0, kHandCount - 1);
    if (!s.active || !s.hand_tracked[h]) return 0;

    // Grip pose, not aim pose: the model should sit in the fist. The aim pose is
    // the barrel line and is used only for the hitscan ray.
    const Mtx4 m = modelMatrixFromPose(s.input.hand[h].grip, GE_VR_UNITS_PER_METRE);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) mf[i][j] = m.m[i][j];
    return 1;
}

extern "C" void geVrHaptic(GeVrHand hand, float amplitude, float duration_s,
                           float frequency_hz) {
    ge_vr::inputHaptic(int(hand), amplitude, duration_s, frequency_hz);
}

// ---------------------------------------------------------------------------
// Pad
// ---------------------------------------------------------------------------

extern "C" void geVrGetPadState(GeVrPadState* out) {
    if (!out) return;
    auto& s = S();
    std::lock_guard<std::mutex> lk(s.mu);
    *out = GeVrPadState{};
    if (!s.active) return;
    out->move_x = s.input.move_x;
    out->move_y = s.input.move_y;
    out->turn_x = s.input.turn_x;
    out->turn_y = s.input.turn_y;
    out->trigger[0] = s.input.hand[0].trigger;
    out->trigger[1] = s.input.hand[1].trigger;
    out->buttons = s.input.buttons;
}

extern "C" int geVrWatchGestureActive(void) {
    auto& s = S();
    std::lock_guard<std::mutex> lk(s.mu);
    if (!s.active || !s.hand_tracked[GE_VR_HAND_LEFT] || !s.frame.head_valid) return 0;

    // "Raise the left wrist toward the face." Two conditions, both needed:
    // the wrist is close to the head AND it is turned so the watch face points
    // at the eyes. Distance alone triggers constantly when reloading.
    const Pose head = s.frame.head;
    const Pose wrist = s.input.hand[GE_VR_HAND_LEFT].grip;
    const Vec3 d{wrist.position.x - head.position.x,
                 wrist.position.y - head.position.y,
                 wrist.position.z - head.position.z};
    const float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (dist > 0.40f) return 0;

    // Watch face is +Y in grip space for a wrist-worn object.
    const Vec3 face = quatRotate(wrist.orientation, Vec3{0, 1, 0});
    const float inv = dist > 1e-4f ? 1.0f / dist : 0.0f;
    const float toward = -(face.x * d.x + face.y * d.y + face.z * d.z) * inv;
    return toward > 0.5f ? 1 : 0;
}

extern "C" float geVrPhysicalCrouch(void) {
    auto& s = S();
    std::lock_guard<std::mutex> lk(s.mu);
    if (!s.active || !s.height_calibrated || !s.frame.head_valid) return 0.0f;
    const float h = s.frame.head.position.y;
    // Full crouch at 55% of standing height; linear between.
    const float t = (s.standing_height_m - h) / (s.standing_height_m * 0.45f);
    return std::clamp(t, 0.0f, 1.0f);
}

extern "C" void geVrRecenter(void) {
    auto& s = S();
    std::lock_guard<std::mutex> lk(s.mu);
    if (!s.frame.head_valid) return;
    float theta = 0.0f;
    poseToGameAngles(s.frame.head.orientation, &theta, nullptr, nullptr);
    s.recenter_yaw = theta;
    s.recenter_pos = s.frame.head.position;
    s.recenter_pos.y = 0.0f;  // never recentre height away; that's calibration
    s.standing_height_m = s.frame.head.position.y;
    s.height_calibrated = true;
}
