#include "rt64_backend.h"

#include <cstdio>
#include <cstring>

#include "ultra/rdram.h"

// RT64 headers. These pull in SDL and plume; keep them out of our public header
// so the rest of the port does not inherit the dependency.
#include "hle/rt64_application.h"

namespace ge_rhi {
namespace {

// The N64's SP has 4 KiB each of DMEM and IMEM.
constexpr uint32_t kSpMemSize = 0x1000;
// The ROM header RT64 may inspect for game identification.
constexpr uint32_t kHeaderSize = 0x40;

void noopCheckInterrupts() {
    // On hardware the RDP raises an interrupt when it finishes a task and the
    // CPU acknowledges it. A native port has no interrupt controller, and the
    // frame loop is driven by the host (OpenXR or the VI shim) rather than by
    // the RCP, so there is nothing to signal. Deliberately empty rather than
    // unimplemented.
}

}  // namespace

struct Rt64Backend::Impl {
    std::unique_ptr<RT64::Application> app;
    RT64::Application::Core core{};
};

Rt64Backend::Rt64Backend()
    : impl_(std::make_unique<Impl>()),
      dmem_(std::make_unique<uint8_t[]>(kSpMemSize)),
      imem_(std::make_unique<uint8_t[]>(kSpMemSize)),
      header_(std::make_unique<uint8_t[]>(kHeaderSize)) {
    std::memset(dmem_.get(), 0, kSpMemSize);
    std::memset(imem_.get(), 0, kSpMemSize);
    std::memset(header_.get(), 0, kHeaderSize);
}

Rt64Backend::~Rt64Backend() { shutdown(); }

Rt64InitResult Rt64Backend::init(SDL_Window* window, const std::string& app_id) {
    Rt64InitResult result;

    if (ge_ultra::rdramBase() == nullptr) {
        result.message = "ge_ultra::rdramInit() must be called before RT64 init";
        return result;
    }

    RT64::Application::Core& core = impl_->core;
    core.window = window;
    core.HEADER = header_.get();
    core.RDRAM = ge_ultra::rdramBase();   // our flat 8 MB buffer, handed over directly
    core.DMEM = dmem_.get();
    core.IMEM = imem_.get();

    core.MI_INTR_REG = &regs_.MI_INTR;
    core.DPC_START_REG = &regs_.DPC_START;
    core.DPC_END_REG = &regs_.DPC_END;
    core.DPC_CURRENT_REG = &regs_.DPC_CURRENT;
    core.DPC_STATUS_REG = &regs_.DPC_STATUS;
    core.DPC_CLOCK_REG = &regs_.DPC_CLOCK;
    core.DPC_BUFBUSY_REG = &regs_.DPC_BUFBUSY;
    core.DPC_PIPEBUSY_REG = &regs_.DPC_PIPEBUSY;
    core.DPC_TMEM_REG = &regs_.DPC_TMEM;
    core.VI_STATUS_REG = &regs_.VI_STATUS;
    core.VI_ORIGIN_REG = &regs_.VI_ORIGIN;
    core.VI_WIDTH_REG = &regs_.VI_WIDTH;
    core.VI_INTR_REG = &regs_.VI_INTR;
    core.VI_V_CURRENT_LINE_REG = &regs_.VI_V_CURRENT_LINE;
    core.VI_TIMING_REG = &regs_.VI_TIMING;
    core.VI_V_SYNC_REG = &regs_.VI_V_SYNC;
    core.VI_H_SYNC_REG = &regs_.VI_H_SYNC;
    core.VI_LEAP_REG = &regs_.VI_LEAP;
    core.VI_H_START_REG = &regs_.VI_H_START;
    core.VI_V_START_REG = &regs_.VI_V_START;
    core.VI_V_BURST_REG = &regs_.VI_V_BURST;
    core.VI_X_SCALE_REG = &regs_.VI_X_SCALE;
    core.VI_Y_SCALE_REG = &regs_.VI_Y_SCALE;
    core.checkInterrupts = &noopCheckInterrupts;

    // A plausible NTSC 320x240 VI state, matching what the game programs through
    // our own VI shim. RT64 decodes these to work out the presented image's
    // geometry, so they are not decorative.
    regs_.VI_STATUS = 0x0000320E;   // 16-bit colour, AA + dither enabled
    regs_.VI_WIDTH = 320;
    regs_.VI_X_SCALE = 0x00000200;  // 512 = 1.0 in 2.10 fixed point
    regs_.VI_Y_SCALE = 0x00000400;  // 1024 = 1.0 in 2.10, doubled for interlace
    regs_.VI_H_START = 0x006C02EC;
    regs_.VI_V_START = 0x002501FF;
    regs_.VI_V_SYNC = 0x020D;
    regs_.VI_H_SYNC = 0x00000C15;

    RT64::ApplicationConfiguration appConfig;
    appConfig.appId = app_id;
    appConfig.detectDataPath = true;
    appConfig.useConfigurationFile = false;  // no user config in a port

    impl_->app = std::make_unique<RT64::Application>(core, appConfig);

    const RT64::Application::SetupResult setup = impl_->app->setup(0);
    switch (setup) {
        case RT64::Application::SetupResult::Success:
            result.ok = true;
            result.message = "RT64 ready";
            return result;
        case RT64::Application::SetupResult::DynamicLibrariesNotFound:
            result.message = "RT64: graphics dynamic libraries not found";
            break;
        case RT64::Application::SetupResult::InvalidGraphicsAPI:
            result.message = "RT64: invalid graphics API for this platform";
            break;
        case RT64::Application::SetupResult::GraphicsAPINotFound:
            result.message = "RT64: graphics API not found";
            break;
        case RT64::Application::SetupResult::GraphicsDeviceNotFound:
            result.message = "RT64: no graphics device (headless, or no ICD)";
            break;
        default:
            result.message = "RT64: unknown setup failure";
            break;
    }
    impl_->app.reset();
    return result;
}

bool Rt64Backend::loadMicrocode(const void* text, uint32_t text_size,
                                const void* data, uint32_t data_size) {
    if (!text || !data) return false;

    // Park the blobs high in RDRAM, clear of the game's heap. Nothing executes
    // them; they exist so GBIManager::getGBIForUCode can hash them and pick the
    // dialect. See RT64-INTEGRATION.md for why detection currently fails on the
    // retail NTSC-U ROM and the one-line fix.
    ucode_text_addr_ = ge_ultra::kRdramSize - 0x20000;
    ucode_data_addr_ = ucode_text_addr_ + 0x8000;

    void* t = ge_ultra::physicalToVirtual(ucode_text_addr_, text_size);
    void* d = ge_ultra::physicalToVirtual(ucode_data_addr_, data_size);
    if (!t || !d) return false;

    std::memcpy(t, text, text_size);
    std::memcpy(d, data, data_size);
    return true;
}

void Rt64Backend::setFramebuffer(uint32_t framebuffer, uint32_t width,
                                 uint32_t height) {
    // VI_ORIGIN is how RT64 learns which buffer to present. This is the direct
    // consumer of ge_ultra::viLatchFramebuffer(): the game's osViSwapBuffer
    // becomes RT64's present source with no translation in between.
    regs_.VI_ORIGIN = framebuffer;
    regs_.VI_WIDTH = width;
    (void)height;  // height is derived from V_START/V_SYNC, not set directly
}

void Rt64Backend::processDisplayList(uint32_t start, uint32_t end) {
    if (!impl_->app) return;
    // isHLE = true: we hand over a display list, not a raw RDP command stream.
    impl_->app->processDisplayLists(ge_ultra::rdramBase(), start, end, true);
}

void Rt64Backend::present() {
    if (!impl_->app) return;
    impl_->app->updateScreen();
}

void Rt64Backend::shutdown() {
    if (impl_ && impl_->app) {
        impl_->app->end();
        impl_->app.reset();
    }
}

}  // namespace ge_rhi
