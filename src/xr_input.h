// xr_input.h — OpenXR action sets and the OSContPad synthesis path.
//
// Design rule: every unmodified game input path keeps working. The shim
// synthesises a normal OSContPad from sticks and buttons and hands it to
// osContGetReadData(), so src/joy.c and everything above it are untouched.
// ONLY aim direction and the hitscan ray bypass the pad, because those are the
// two things a 1997 gamepad genuinely cannot express.

#pragma once

#include <cstdint>

#include "xr_math.h"

namespace ge_vr {

struct HandState {
    Pose  aim;              // /input/aim/pose  — barrel line, used for hitscan
    Pose  grip;             // /input/grip/pose — fist, used for weapon model
    float trigger = 0.0f;   // 0..1
    bool  tracked = false;
};

struct InputState {
    HandState hand[kHandCount];
    float move_x = 0.0f, move_y = 0.0f;
    float turn_x = 0.0f, turn_y = 0.0f;
    uint32_t buttons = 0;   // GE_VR_BTN_* from ge_vr.h
};

// Creates the action set, actions, and suggested bindings for the interaction
// profiles below, then attaches to the session.
//
// Profiles to support at minimum:
//   /interaction_profiles/khr/simple_controller      (mandatory fallback)
//   /interaction_profiles/oculus/touch_controller
//   /interaction_profiles/valve/index_controller
//   /interaction_profiles/htc/vive_controller
//   /interaction_profiles/microsoft/motion_controller
//
// The simple_controller fallback is not optional: a runtime that offers only
// that profile will silently deliver no input at all if it is omitted, which
// presents as "the game ignores my controller" with no error anywhere.
bool inputInit(void* xr_instance, void* xr_session);
void inputShutdown();

// Sync actions and locate hand poses at the predicted display time. Poses must
// be located at the SAME time as the views, or the gun and the world disagree.
void inputSyncFrame(void* xr_session, void* base_space, int64_t predicted_time_ns,
                    InputState* out);

void inputHaptic(int hand, float amplitude, float duration_s, float frequency_hz);

// ---------------------------------------------------------------------------
// OSContPad synthesis
// ---------------------------------------------------------------------------

// Mirrors libultra's OSContPad without including ultra64.h here.
struct SynthPad {
    int16_t  stick_x = 0;   // -80..80, the N64 range src/joy.c expects
    int16_t  stick_y = 0;
    uint16_t button = 0;    // N64 button bits
};

// Map VR input onto the pad the game already understands.
//
// Note what is deliberately NOT mapped: the C-buttons that originally drove
// look. In VR, look comes from the head and turn comes from the right stick, so
// the C-button look axes must be zeroed or the view will fight the player.
SynthPad synthesizePad(const InputState& in, float snap_turn_accumulated_rad);

}  // namespace ge_vr
