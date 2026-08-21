// xr_session.h — OpenXR instance/session/swapchain lifetime and the frame loop.
//
// This owns frame pacing. That is the single biggest structural change the VR
// layer imposes on the port: on N64 the scheduler thread (src/sched.c) paced the
// game off VI retrace. Here, xrWaitFrame paces it. src/sched.c is not emulated,
// it is replaced.

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "xr_math.h"

namespace ge_vr {

// Per-eye state, refreshed once per frame by xrLocateViews at the predicted
// display time. Read by the bridge and, through it, by fr.c.
struct EyeView {
    Pose pose;
    Fov  fov;
    bool valid = false;
};

struct FrameState {
    int64_t predicted_display_time_ns = 0;
    // Seconds of predicted display time delta. Game simulation must consume THIS,
    // not wall-clock — otherwise head motion and world motion disagree by a frame,
    // which reads as nausea rather than as a frame-timing bug.
    float   predicted_delta_s = 0.0f;
    bool    should_render = false;
    EyeView eye[kEyeCount];
    Pose    head;           // in the stage/local reference space
    bool    head_valid = false;
};

// The renderer implements this. The scaffold ships a null implementation; the
// real one is backed by the RT64 fork (architecture doc §4.3).
class IGraphicsBackend {
public:
    virtual ~IGraphicsBackend() = default;

    // Graphics binding chained into XrSessionCreateInfo::next. Backend-specific
    // (XrGraphicsBindingD3D12KHR / XrGraphicsBindingVulkanKHR).
    virtual const void* sessionCreateInfoNext() = 0;

    // Which XR_EXT/KHR graphics extension this backend needs enabled.
    virtual const char* requiredExtension() = 0;

    virtual int64_t preferredSwapchainFormat(const std::vector<int64_t>& supported) = 0;

    // Called once per eye per frame with the acquired swapchain image handle.
    // Implementation binds it as the render target for that eye's pass.
    virtual void beginEyeTarget(int eye, uint64_t image_handle,
                                uint32_t width, uint32_t height) = 0;
    virtual void endEyeTarget(int eye) = 0;
};

// The per-frame callback the port installs. Called exactly once per rendered
// frame, between xrBeginFrame and xrEndFrame.
//
// Contract, and it matters: simulate ONCE, render TWICE. Running the game tick
// per eye doubles AI/physics cost and — worse — desynchronises the two eyes by
// one simulation step, which is the most reliably sickening bug in this whole
// project.
struct FrameCallbacks {
    std::function<void(const FrameState&)> simulate;         // once per frame
    std::function<void(const FrameState&, int eye)> render;  // twice per frame
};

class Session {
public:
    Session();
    ~Session();

    // Returns false if no runtime is present or the headset is unavailable. The
    // caller must fall back to the flat-screen port rather than aborting — an
    // unplugged headset should not be a crash.
    bool init(IGraphicsBackend* backend, const char* app_name);
    void shutdown();

    bool isRunning() const;
    bool isFocused() const;

    // Pumps XR events, waits, and drives one frame through the callbacks.
    // Returns false when the runtime has asked us to exit.
    bool pumpFrame(const FrameCallbacks& cb);

    const FrameState& lastFrame() const { return frame_; }

    void requestRecenter();

private:
    void pumpEvents();

    struct Impl;
    Impl* impl_;
    FrameState frame_;
};

}  // namespace ge_vr
