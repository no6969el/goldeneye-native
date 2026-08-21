#include "xr_session.h"

#include <cstdio>
#include <cstring>

#if GE_VR_HAVE_OPENXR
#include <openxr/openxr.h>
#endif

namespace ge_vr {

#if GE_VR_HAVE_OPENXR

#define XR_CHECK(expr)                                                     \
    do {                                                                   \
        XrResult _r = (expr);                                              \
        if (XR_FAILED(_r)) {                                               \
            std::fprintf(stderr, "[ge-xr] %s failed (%d) at %s:%d\n",      \
                         #expr, int(_r), __FILE__, __LINE__);              \
            return false;                                                  \
        }                                                                  \
    } while (0)

namespace {

Pose toPose(const XrPosef& p) {
    Pose out;
    out.orientation = Quat{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w};
    out.position    = Vec3{p.position.x, p.position.y, p.position.z};
    return out;
}

Fov toFov(const XrFovf& f) {
    Fov out;
    out.angleLeft  = f.angleLeft;
    out.angleRight = f.angleRight;
    out.angleUp    = f.angleUp;
    out.angleDown  = f.angleDown;
    return out;
}

struct Swapchain {
    XrSwapchain handle = XR_NULL_HANDLE;
    uint32_t width = 0, height = 0;
    std::vector<uint64_t> images;  // backend-opaque native handles
};

}  // namespace

struct Session::Impl {
    IGraphicsBackend* backend = nullptr;

    XrInstance   instance   = XR_NULL_HANDLE;
    XrSystemId   system     = XR_NULL_SYSTEM_ID;
    XrSession    session    = XR_NULL_HANDLE;

    // Two spaces, deliberately:
    //  - stage: the physical play area. 6DoF head position is measured here.
    //  - view:  the head. Used to derive head-relative quantities.
    // A seated user with no stage bounds gets LOCAL instead; the bridge treats
    // that as "positional tracking present but play area unknown" and tightens
    // the comfort clamp rather than disabling 6DoF.
    XrSpace      stage_space = XR_NULL_HANDLE;
    XrSpace      view_space  = XR_NULL_HANDLE;
    bool         stage_is_local = false;

    XrSessionState state = XR_SESSION_STATE_UNKNOWN;
    bool session_running = false;
    bool exit_requested  = false;

    XrViewConfigurationView view_config[kEyeCount]{};
    Swapchain swapchain[kEyeCount];
    XrFrameState frame_state{XR_TYPE_FRAME_STATE};
    int64_t last_display_time = 0;

    // Recentre is applied as a yaw+position offset in the bridge rather than by
    // recreating the reference space, so it survives runtimes that don't support
    // XR_EXT_local_floor and doesn't stall the frame loop.
    bool recenter_pending = false;
};

Session::Session() : impl_(new Impl) {}
Session::~Session() { shutdown(); delete impl_; }

bool Session::init(IGraphicsBackend* backend, const char* app_name) {
    impl_->backend = backend;

    const char* extensions[] = { backend->requiredExtension() };

    XrInstanceCreateInfo ici{XR_TYPE_INSTANCE_CREATE_INFO};
    std::snprintf(ici.applicationInfo.applicationName,
                  sizeof(ici.applicationInfo.applicationName), "%s", app_name);
    ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    ici.enabledExtensionCount = 1;
    ici.enabledExtensionNames = extensions;

    if (XR_FAILED(xrCreateInstance(&ici, &impl_->instance))) {
        // No runtime installed, or headset not connected. This is an expected
        // outcome, not an error: fall through to the flat-screen port.
        std::fprintf(stderr, "[ge-xr] no OpenXR runtime available; running flat.\n");
        return false;
    }

    XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XR_CHECK(xrGetSystem(impl_->instance, &sgi, &impl_->system));

    uint32_t view_count = 0;
    XR_CHECK(xrEnumerateViewConfigurationViews(
        impl_->instance, impl_->system,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &view_count, nullptr));
    if (view_count != kEyeCount) {
        std::fprintf(stderr, "[ge-xr] expected 2 views, runtime reports %u\n", view_count);
        return false;
    }
    for (int i = 0; i < kEyeCount; ++i)
        impl_->view_config[i] = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
    XR_CHECK(xrEnumerateViewConfigurationViews(
        impl_->instance, impl_->system,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, view_count, &view_count,
        impl_->view_config));

    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next     = backend->sessionCreateInfoNext();
    sci.systemId = impl_->system;
    XR_CHECK(xrCreateSession(impl_->instance, &sci, &impl_->session));

    // --- reference spaces ---
    XrReferenceSpaceCreateInfo rsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rsci.poseInReferenceSpace.orientation.w = 1.0f;

    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    if (XR_FAILED(xrCreateReferenceSpace(impl_->session, &rsci, &impl_->stage_space))) {
        rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        impl_->stage_is_local = true;
        XR_CHECK(xrCreateReferenceSpace(impl_->session, &rsci, &impl_->stage_space));
    }
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    XR_CHECK(xrCreateReferenceSpace(impl_->session, &rsci, &impl_->view_space));

    // --- swapchains ---
    uint32_t fmt_count = 0;
    XR_CHECK(xrEnumerateSwapchainFormats(impl_->session, 0, &fmt_count, nullptr));
    std::vector<int64_t> formats(fmt_count);
    XR_CHECK(xrEnumerateSwapchainFormats(impl_->session, fmt_count, &fmt_count, formats.data()));
    const int64_t chosen = backend->preferredSwapchainFormat(formats);

    for (int eye = 0; eye < kEyeCount; ++eye) {
        const auto& vc = impl_->view_config[eye];
        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        ci.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                         XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        ci.format      = chosen;
        ci.sampleCount = 1;
        ci.width       = vc.recommendedImageRectWidth;
        ci.height      = vc.recommendedImageRectHeight;
        ci.faceCount   = 1;
        ci.arraySize   = 1;
        ci.mipCount    = 1;
        XR_CHECK(xrCreateSwapchain(impl_->session, &ci, &impl_->swapchain[eye].handle));
        impl_->swapchain[eye].width  = ci.width;
        impl_->swapchain[eye].height = ci.height;

        // TODO(phase4): enumerate images with the backend-specific
        // XrSwapchainImage*KHR struct and store native handles. Left to the
        // backend because the struct type is graphics-API-dependent.
    }

    return true;
}

void Session::shutdown() {
    if (!impl_ || !impl_->instance) return;
    for (auto& sc : impl_->swapchain)
        if (sc.handle) xrDestroySwapchain(sc.handle);
    if (impl_->view_space)  xrDestroySpace(impl_->view_space);
    if (impl_->stage_space) xrDestroySpace(impl_->stage_space);
    if (impl_->session)     xrDestroySession(impl_->session);
    if (impl_->instance)    xrDestroyInstance(impl_->instance);
    *impl_ = Impl{};
}

bool Session::isRunning() const { return impl_ && impl_->session_running; }
bool Session::isFocused() const {
    return impl_ && impl_->state == XR_SESSION_STATE_FOCUSED;
}
void Session::requestRecenter() { if (impl_) impl_->recenter_pending = true; }

void Session::pumpEvents() {
    Impl* impl = impl_;
    XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(impl->instance, &ev) == XR_SUCCESS) {
        switch (ev.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                auto* e = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
                impl->state = e->state;
                if (e->state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
                    bi.primaryViewConfigurationType =
                        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    xrBeginSession(impl->session, &bi);
                    impl->session_running = true;
                } else if (e->state == XR_SESSION_STATE_STOPPING) {
                    xrEndSession(impl->session);
                    impl->session_running = false;
                } else if (e->state == XR_SESSION_STATE_EXITING ||
                           e->state == XR_SESSION_STATE_LOSS_PENDING) {
                    impl->exit_requested = true;
                }
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                impl->exit_requested = true;
                break;
            default:
                break;
        }
        ev = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

bool Session::pumpFrame(const FrameCallbacks& cb) {
    pumpEvents();
    if (impl_->exit_requested) return false;
    if (!impl_->session_running) return true;  // idle; not an error

    XrFrameWaitInfo fwi{XR_TYPE_FRAME_WAIT_INFO};
    impl_->frame_state = {XR_TYPE_FRAME_STATE};
    if (XR_FAILED(xrWaitFrame(impl_->session, &fwi, &impl_->frame_state))) return true;

    XrFrameBeginInfo fbi{XR_TYPE_FRAME_BEGIN_INFO};
    xrBeginFrame(impl_->session, &fbi);

    const int64_t t = impl_->frame_state.predictedDisplayTime;
    frame_.predicted_display_time_ns = t;
    frame_.predicted_delta_s =
        impl_->last_display_time ? float(t - impl_->last_display_time) * 1e-9f : 1.0f / 90.0f;
    impl_->last_display_time = t;
    frame_.should_render = impl_->frame_state.shouldRender != 0;

    // --- locate views at the PREDICTED display time ---
    XrView views[kEyeCount];
    for (auto& v : views) v = {XR_TYPE_VIEW};
    XrViewState vs{XR_TYPE_VIEW_STATE};
    XrViewLocateInfo vli{XR_TYPE_VIEW_LOCATE_INFO};
    vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vli.displayTime = t;
    vli.space = impl_->stage_space;
    uint32_t located = 0;
    xrLocateViews(impl_->session, &vli, &vs, kEyeCount, &located, views);

    const bool pose_ok = (vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) &&
                         (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);
    for (int eye = 0; eye < kEyeCount; ++eye) {
        frame_.eye[eye].pose  = toPose(views[eye].pose);
        frame_.eye[eye].fov   = toFov(views[eye].fov);
        frame_.eye[eye].valid = pose_ok;
    }

    XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};
    xrLocateSpace(impl_->view_space, impl_->stage_space, t, &head);
    frame_.head_valid = (head.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
                        (head.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
    frame_.head = toPose(head.pose);

    // --- simulate ONCE ---
    if (cb.simulate) cb.simulate(frame_);

    XrCompositionLayerProjectionView proj_views[kEyeCount]{};

    if (frame_.should_render) {
        for (int eye = 0; eye < kEyeCount; ++eye) {
            auto& sc = impl_->swapchain[eye];
            uint32_t index = 0;
            XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            xrAcquireSwapchainImage(sc.handle, &ai, &index);
            XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            wi.timeout = XR_INFINITE_DURATION;
            xrWaitSwapchainImage(sc.handle, &wi);

            const uint64_t image = index < sc.images.size() ? sc.images[index] : 0;
            impl_->backend->beginEyeTarget(eye, image, sc.width, sc.height);
            if (cb.render) cb.render(frame_, eye);
            impl_->backend->endEyeTarget(eye);

            XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(sc.handle, &ri);

            proj_views[eye] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            proj_views[eye].pose = views[eye].pose;
            proj_views[eye].fov  = views[eye].fov;
            proj_views[eye].subImage.swapchain = sc.handle;
            proj_views[eye].subImage.imageRect.offset = {0, 0};
            proj_views[eye].subImage.imageRect.extent = {int32_t(sc.width), int32_t(sc.height)};
        }
    }

    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = impl_->stage_space;
    layer.viewCount = kEyeCount;
    layer.views = proj_views;
    const XrCompositionLayerBaseHeader* layers[] =
        {reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer)};

    XrFrameEndInfo fei{XR_TYPE_FRAME_END_INFO};
    fei.displayTime = t;
    fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    fei.layerCount = frame_.should_render ? 1 : 0;
    fei.layers = frame_.should_render ? layers : nullptr;
    xrEndFrame(impl_->session, &fei);

    return true;
}

#else  // !GE_VR_HAVE_OPENXR
// ---------------------------------------------------------------------------
// Null backend. Lets the whole project — and the math tests — build and run on
// a machine with no OpenXR SDK, which is most CI machines.
// ---------------------------------------------------------------------------

struct Session::Impl { int unused = 0; };

Session::Session() : impl_(new Impl) {}
Session::~Session() { delete impl_; }
bool Session::init(IGraphicsBackend*, const char*) {
    std::fprintf(stderr, "[ge-xr] built without OpenXR; running flat.\n");
    return false;
}
void Session::shutdown() {}
bool Session::isRunning() const { return false; }
bool Session::isFocused() const { return false; }
void Session::requestRecenter() {}
void Session::pumpEvents() {}
bool Session::pumpFrame(const FrameCallbacks&) { return false; }

#endif

}  // namespace ge_vr
