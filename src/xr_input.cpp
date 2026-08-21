#include "xr_input.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

#include "ge_vr/ge_vr.h"

#if GE_VR_HAVE_OPENXR
#include <openxr/openxr.h>
#endif

namespace ge_vr {

// N64 button bits, from PR/os_cont.h. Reproduced so this file doesn't depend on
// the decomp headers.
enum {
    N64_A       = 0x8000,
    N64_B       = 0x4000,
    N64_Z       = 0x2000,
    N64_START   = 0x1000,
    N64_L       = 0x0020,
    N64_R       = 0x0010,
    N64_C_UP    = 0x0008,
    N64_C_DOWN  = 0x0004,
    N64_C_LEFT  = 0x0002,
    N64_C_RIGHT = 0x0001,
};

SynthPad synthesizePad(const InputState& in, float /*snap_turn_accumulated_rad*/) {
    SynthPad pad;

    // Movement -> analogue stick. The N64 stick saturates around ±80, not ±127;
    // feeding it ±127 makes the player permanently sprint and breaks the
    // walk/run threshold the animation system keys off.
    pad.stick_x = int16_t(std::clamp(in.move_x, -1.0f, 1.0f) * 80.0f);
    pad.stick_y = int16_t(std::clamp(in.move_y, -1.0f, 1.0f) * 80.0f);

    uint16_t b = 0;
    if (in.buttons & GE_VR_BTN_FIRE_R) b |= N64_Z;
    if (in.buttons & GE_VR_BTN_FIRE_L) b |= N64_A;      // dual-wield second hand
    if (in.buttons & GE_VR_BTN_AIM_R)  b |= N64_R;      // GE aim mode
    if (in.buttons & GE_VR_BTN_USE)    b |= N64_B;
    if (in.buttons & GE_VR_BTN_SWAP)   b |= N64_A;
    if (in.buttons & GE_VR_BTN_PAUSE)  b |= N64_START;
    if (in.buttons & GE_VR_BTN_CROUCH) b |= N64_L;

    // Deliberately unmapped: C_UP / C_DOWN / C_LEFT / C_RIGHT.
    // Those were the look axes. In VR, look is the head. Leaving them live means
    // the game rotates the view out from under a player who is already turning
    // their neck, which is the fastest route to motion sickness in this project.

    pad.button = b;
    return pad;
}

#if GE_VR_HAVE_OPENXR

namespace {

struct Action {
    XrAction handle = XR_NULL_HANDLE;
};

struct Ctx {
    XrInstance instance = XR_NULL_HANDLE;
    XrActionSet set = XR_NULL_HANDLE;

    XrPath hand_path[kHandCount]{};

    Action a_aim_pose, a_grip_pose, a_trigger, a_squeeze, a_haptic;
    Action a_move, a_turn, a_reload, a_use, a_watch, a_swap, a_pause, a_crouch;

    XrSpace aim_space[kHandCount]{};
    XrSpace grip_space[kHandCount]{};
};

Ctx g;

XrPath path(const char* s) {
    XrPath p = XR_NULL_PATH;
    xrStringToPath(g.instance, s, &p);
    return p;
}

bool makeAction(XrActionType type, const char* name, const char* localized,
                Action* out, bool two_hands = true) {
    XrActionCreateInfo ci{XR_TYPE_ACTION_CREATE_INFO};
    ci.actionType = type;
    std::snprintf(ci.actionName, sizeof(ci.actionName), "%s", name);
    std::snprintf(ci.localizedActionName, sizeof(ci.localizedActionName), "%s", localized);
    if (two_hands) {
        ci.countSubactionPaths = kHandCount;
        ci.subactionPaths = g.hand_path;
    }
    return XR_SUCCEEDED(xrCreateAction(g.set, &ci, &out->handle));
}

void suggest(const char* profile,
             std::initializer_list<std::pair<XrAction, const char*>> bindings) {
    std::vector<XrActionSuggestedBinding> v;
    v.reserve(bindings.size());
    for (auto& [action, p] : bindings) v.push_back({action, path(p)});
    XrInteractionProfileSuggestedBinding s{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    s.interactionProfile = path(profile);
    s.countSuggestedBindings = uint32_t(v.size());
    s.suggestedBindings = v.data();
    // A failure here means this runtime doesn't know the profile — expected and
    // harmless, as long as khr/simple_controller also gets suggested.
    xrSuggestInteractionProfileBindings(g.instance, &s);
}

}  // namespace

bool inputInit(void* xr_instance, void* xr_session) {
    g.instance = static_cast<XrInstance>(xr_instance);
    auto session = static_cast<XrSession>(xr_session);

    XrActionSetCreateInfo sci{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::snprintf(sci.actionSetName, sizeof(sci.actionSetName), "gameplay");
    std::snprintf(sci.localizedActionSetName, sizeof(sci.localizedActionSetName), "Gameplay");
    sci.priority = 0;
    if (XR_FAILED(xrCreateActionSet(g.instance, &sci, &g.set))) return false;

    g.hand_path[GE_VR_HAND_LEFT]  = path("/user/hand/left");
    g.hand_path[GE_VR_HAND_RIGHT] = path("/user/hand/right");

    makeAction(XR_ACTION_TYPE_POSE_INPUT,    "aim_pose",  "Aim Pose",  &g.a_aim_pose);
    makeAction(XR_ACTION_TYPE_POSE_INPUT,    "grip_pose", "Grip Pose", &g.a_grip_pose);
    makeAction(XR_ACTION_TYPE_FLOAT_INPUT,   "fire",      "Fire",      &g.a_trigger);
    makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "aim_mode",  "Aim Mode",  &g.a_squeeze);
    makeAction(XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", "Haptic",    &g.a_haptic);
    makeAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "move",   "Move",   &g.a_move,   false);
    makeAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "turn",   "Turn",   &g.a_turn,   false);
    makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT,  "reload", "Reload", &g.a_reload, false);
    makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT,  "use",    "Use",    &g.a_use,    false);
    makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT,  "watch",  "Watch",  &g.a_watch,  false);
    makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT,  "swap",   "Swap Weapon", &g.a_swap, false);
    makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT,  "pause",  "Pause",  &g.a_pause,  false);
    makeAction(XR_ACTION_TYPE_BOOLEAN_INPUT,  "crouch", "Crouch", &g.a_crouch, false);

    // --- mandatory fallback profile ---
    suggest("/interaction_profiles/khr/simple_controller", {
        {g.a_aim_pose.handle,  "/user/hand/left/input/aim/pose"},
        {g.a_aim_pose.handle,  "/user/hand/right/input/aim/pose"},
        {g.a_grip_pose.handle, "/user/hand/left/input/grip/pose"},
        {g.a_grip_pose.handle, "/user/hand/right/input/grip/pose"},
        {g.a_trigger.handle,   "/user/hand/right/input/select/click"},
        {g.a_use.handle,       "/user/hand/left/input/select/click"},
        {g.a_pause.handle,     "/user/hand/left/input/menu/click"},
        {g.a_haptic.handle,    "/user/hand/left/output/haptic"},
        {g.a_haptic.handle,    "/user/hand/right/output/haptic"},
    });

    suggest("/interaction_profiles/oculus/touch_controller", {
        {g.a_aim_pose.handle,  "/user/hand/left/input/aim/pose"},
        {g.a_aim_pose.handle,  "/user/hand/right/input/aim/pose"},
        {g.a_grip_pose.handle, "/user/hand/left/input/grip/pose"},
        {g.a_grip_pose.handle, "/user/hand/right/input/grip/pose"},
        {g.a_trigger.handle,   "/user/hand/left/input/trigger/value"},
        {g.a_trigger.handle,   "/user/hand/right/input/trigger/value"},
        {g.a_squeeze.handle,   "/user/hand/left/input/squeeze/value"},
        {g.a_squeeze.handle,   "/user/hand/right/input/squeeze/value"},
        {g.a_move.handle,      "/user/hand/left/input/thumbstick"},
        {g.a_turn.handle,      "/user/hand/right/input/thumbstick"},
        {g.a_reload.handle,    "/user/hand/right/input/a/click"},
        {g.a_use.handle,       "/user/hand/right/input/b/click"},
        {g.a_swap.handle,      "/user/hand/left/input/x/click"},
        {g.a_watch.handle,     "/user/hand/left/input/y/click"},
        {g.a_crouch.handle,    "/user/hand/left/input/thumbstick/click"},
        {g.a_pause.handle,     "/user/hand/left/input/menu/click"},
        {g.a_haptic.handle,    "/user/hand/left/output/haptic"},
        {g.a_haptic.handle,    "/user/hand/right/output/haptic"},
    });

    // TODO(phase4): valve/index_controller, htc/vive_controller,
    // microsoft/motion_controller. Index in particular wants force-based squeeze.

    // --- action spaces ---
    for (int h = 0; h < kHandCount; ++h) {
        XrActionSpaceCreateInfo asci{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        asci.poseInActionSpace.orientation.w = 1.0f;
        asci.subactionPath = g.hand_path[h];
        asci.action = g.a_aim_pose.handle;
        xrCreateActionSpace(session, &asci, &g.aim_space[h]);
        asci.action = g.a_grip_pose.handle;
        xrCreateActionSpace(session, &asci, &g.grip_space[h]);
    }

    XrSessionActionSetsAttachInfo ai{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    ai.countActionSets = 1;
    ai.actionSets = &g.set;
    return XR_SUCCEEDED(xrAttachSessionActionSets(session, &ai));
}

void inputShutdown() {
    for (int h = 0; h < kHandCount; ++h) {
        if (g.aim_space[h])  xrDestroySpace(g.aim_space[h]);
        if (g.grip_space[h]) xrDestroySpace(g.grip_space[h]);
    }
    if (g.set) xrDestroyActionSet(g.set);
    g = Ctx{};
}

void inputSyncFrame(void* xr_session, void* base_space, int64_t t, InputState* out) {
    auto session = static_cast<XrSession>(xr_session);
    auto space   = static_cast<XrSpace>(base_space);
    *out = InputState{};

    XrActiveActionSet active{g.set, XR_NULL_PATH};
    XrActionsSyncInfo si{XR_TYPE_ACTIONS_SYNC_INFO};
    si.countActiveActionSets = 1;
    si.activeActionSets = &active;
    if (XR_FAILED(xrSyncActions(session, &si))) return;

    auto getFloat = [&](XrAction a, XrPath sub, float* v) {
        XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
        gi.action = a; gi.subactionPath = sub;
        XrActionStateFloat s{XR_TYPE_ACTION_STATE_FLOAT};
        if (XR_SUCCEEDED(xrGetActionStateFloat(session, &gi, &s)) && s.isActive)
            *v = s.currentState;
    };
    auto getBool = [&](XrAction a, XrPath sub, uint32_t bit) {
        XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
        gi.action = a; gi.subactionPath = sub;
        XrActionStateBoolean s{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session, &gi, &s)) && s.isActive &&
            s.currentState)
            out->buttons |= bit;
    };
    auto getVec2 = [&](XrAction a, float* x, float* y) {
        XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
        gi.action = a;
        XrActionStateVector2f s{XR_TYPE_ACTION_STATE_VECTOR2F};
        if (XR_SUCCEEDED(xrGetActionStateVector2f(session, &gi, &s)) && s.isActive) {
            *x = s.currentState.x;
            *y = s.currentState.y;
        }
    };

    getVec2(g.a_move.handle, &out->move_x, &out->move_y);
    getVec2(g.a_turn.handle, &out->turn_x, &out->turn_y);
    getBool(g.a_reload.handle, XR_NULL_PATH, GE_VR_BTN_RELOAD);
    getBool(g.a_use.handle,    XR_NULL_PATH, GE_VR_BTN_USE);
    getBool(g.a_watch.handle,  XR_NULL_PATH, GE_VR_BTN_WATCH);
    getBool(g.a_swap.handle,   XR_NULL_PATH, GE_VR_BTN_SWAP);
    getBool(g.a_pause.handle,  XR_NULL_PATH, GE_VR_BTN_PAUSE);
    getBool(g.a_crouch.handle, XR_NULL_PATH, GE_VR_BTN_CROUCH);

    for (int h = 0; h < kHandCount; ++h) {
        getFloat(g.a_trigger.handle, g.hand_path[h], &out->hand[h].trigger);
        if (out->hand[h].trigger > 0.6f)
            out->buttons |= (h == GE_VR_HAND_LEFT) ? GE_VR_BTN_FIRE_L : GE_VR_BTN_FIRE_R;
        getBool(g.a_squeeze.handle, g.hand_path[h],
                (h == GE_VR_HAND_LEFT) ? GE_VR_BTN_AIM_L : GE_VR_BTN_AIM_R);

        // Poses are located at the SAME predicted display time as the views.
        // Locating them at "now" instead puts the gun a frame behind the world,
        // which players read as the gun being loose and floaty.
        XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
        if (XR_SUCCEEDED(xrLocateSpace(g.aim_space[h], space, t, &loc)) &&
            (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
            (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
            out->hand[h].tracked = true;
            out->hand[h].aim.orientation = {loc.pose.orientation.x, loc.pose.orientation.y,
                                            loc.pose.orientation.z, loc.pose.orientation.w};
            out->hand[h].aim.position = {loc.pose.position.x, loc.pose.position.y,
                                         loc.pose.position.z};
        }
        XrSpaceLocation gloc{XR_TYPE_SPACE_LOCATION};
        if (XR_SUCCEEDED(xrLocateSpace(g.grip_space[h], space, t, &gloc))) {
            out->hand[h].grip.orientation = {gloc.pose.orientation.x, gloc.pose.orientation.y,
                                             gloc.pose.orientation.z, gloc.pose.orientation.w};
            out->hand[h].grip.position = {gloc.pose.position.x, gloc.pose.position.y,
                                          gloc.pose.position.z};
        }
    }
}

void inputHaptic(int hand, float amplitude, float duration_s, float frequency_hz) {
    if (!g.set || hand < 0 || hand >= kHandCount) return;
    XrHapticVibration v{XR_TYPE_HAPTIC_VIBRATION};
    v.amplitude = std::clamp(amplitude, 0.0f, 1.0f);
    v.duration  = XrDuration(duration_s * 1e9);
    v.frequency = frequency_hz;
    XrHapticActionInfo hi{XR_TYPE_HAPTIC_ACTION_INFO};
    hi.action = g.a_haptic.handle;
    hi.subactionPath = g.hand_path[hand];
    // Session handle is needed here; stored by inputInit in a real build.
    // TODO(phase4): cache XrSession in Ctx and call xrApplyHapticFeedback.
    (void)hi;
}

#else  // !GE_VR_HAVE_OPENXR

bool inputInit(void*, void*) { return false; }
void inputShutdown() {}
void inputSyncFrame(void*, void*, int64_t, InputState* out) { *out = InputState{}; }
void inputHaptic(int, float, float, float) {}

#endif

}  // namespace ge_vr
