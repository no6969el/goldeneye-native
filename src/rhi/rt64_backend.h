// rt64_backend.h — hand our RDRAM and display lists to RT64.
//
// RT64 is not fed a digested command stream. It performs deferred RSP *and*
// texture decode in compute shaders, so it wants raw memory plus display list
// addresses and runs its own interpreter. That means there is no adapter layer
// to write and no risk of two paths disagreeing — see RT64-INTEGRATION.md.
//
// What this file actually does is narrow:
//
//   * owns the N64 register block RT64 wants pointers into (it was written for
//     emulator front-ends, so it expects DPC/VI/MI registers to exist even in
//     HLE, where nothing drives them);
//   * points RT64's RDRAM at OUR flat 8 MB buffer;
//   * publishes the framebuffer through VI_ORIGIN_REG, which is how RT64 learns
//     where to present from — and which our VI shim (ge_ultra::viLatchFramebuffer)
//     already produces;
//   * places the microcode blobs so ucode detection can hash them.
//
// The flat-RDRAM decision from M1 is what makes this possible at all: RT64 takes
// a `uint8_t*` plus 32-bit offsets into it. Had the game been handed malloc'd
// blocks, this interface would be unusable.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace RT64 {
    struct Application;
}
struct SDL_Window;

namespace ge_rhi {

struct Rt64InitResult {
    bool ok = false;
    std::string message;
};

// The N64 register file RT64 reads through. Real hardware has these behind MMIO;
// a native port has no such thing, so we synthesise a block and keep it
// coherent enough for RT64's purposes.
struct N64Registers {
    uint32_t MI_INTR = 0;
    uint32_t DPC_START = 0, DPC_END = 0, DPC_CURRENT = 0, DPC_STATUS = 0;
    uint32_t DPC_CLOCK = 0, DPC_BUFBUSY = 0, DPC_PIPEBUSY = 0, DPC_TMEM = 0;
    uint32_t VI_STATUS = 0, VI_ORIGIN = 0, VI_WIDTH = 0, VI_INTR = 0;
    uint32_t VI_V_CURRENT_LINE = 0, VI_TIMING = 0, VI_V_SYNC = 0, VI_H_SYNC = 0;
    uint32_t VI_LEAP = 0, VI_H_START = 0, VI_V_START = 0, VI_V_BURST = 0;
    uint32_t VI_X_SCALE = 0, VI_Y_SCALE = 0;
};

class Rt64Backend {
public:
    Rt64Backend();
    ~Rt64Backend();

    Rt64Backend(const Rt64Backend&) = delete;
    Rt64Backend& operator=(const Rt64Backend&) = delete;

    // `window` may be null only to probe how far setup gets without a surface —
    // useful in CI, where there is no display. A real run needs a window.
    //
    // ge_ultra::rdramInit() must have been called first: RT64 is given that
    // buffer directly and keeps the pointer.
    Rt64InitResult init(SDL_Window* window, const std::string& app_id);

    // Copy the graphics microcode into RDRAM at the given addresses and record
    // them. Nothing executes it — the RSP does not exist here — but RT64
    // identifies the GBI dialect by XXH3-hashing these bytes, so they must be
    // present and at a known address.
    //
    // For the retail NTSC-U ROM the resulting hashes are NOT in RT64's table;
    // see patches/rt64-goldeneye-hashes.patch.
    bool loadMicrocode(const void* text, uint32_t text_size,
                       const void* data, uint32_t data_size);

    // Set the VI state RT64 presents from. `framebuffer` is a physical address,
    // i.e. exactly what ge_ultra::viLatchFramebuffer() returns.
    void setFramebuffer(uint32_t framebuffer, uint32_t width, uint32_t height);

    // Walk a display list. `start`/`end` are physical addresses into our RDRAM.
    void processDisplayList(uint32_t start, uint32_t end);

    void present();
    void shutdown();

    N64Registers& registers() { return regs_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    N64Registers regs_{};

    // RT64 wants DMEM and IMEM to exist. In HLE nothing runs in them, but the
    // ucode addresses are expressed relative to them and the pointers are
    // dereferenced, so they cannot be null.
    std::unique_ptr<uint8_t[]> dmem_;
    std::unique_ptr<uint8_t[]> imem_;
    std::unique_ptr<uint8_t[]> header_;

    uint32_t ucode_text_addr_ = 0;
    uint32_t ucode_data_addr_ = 0;
};

}  // namespace ge_rhi
