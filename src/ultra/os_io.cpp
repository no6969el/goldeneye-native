#include "os_io.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "rdram.h"

namespace ge_ultra {
namespace {

// --- PI state -------------------------------------------------------------
std::vector<uint8_t> g_rom;
RomInfo g_rom_info;
OSPiHandle g_cart_handle{};

// --- VI state -------------------------------------------------------------
ViState g_vi;
OSMesgQueue* g_vi_queue = nullptr;
PiDevAddrTranslator g_devaddr_xlat = nullptr;
OSMesg g_vi_msg = nullptr;
uint32_t g_vi_retrace_interval = 1;

// --- SI state -------------------------------------------------------------
struct PadSlot {
    OSContPad pad{};
    bool connected = false;
    bool motor = false;
};
PadSlot g_pads[MAXCONTROLLERS];
OSMesgQueue* g_cont_queue = nullptr;
bool g_cont_read_pending = false;

}  // namespace

// ---------------------------------------------------------------------------
// Byte order
// ---------------------------------------------------------------------------

void swap16InPlace(void* data, size_t bytes) {
    auto* p = static_cast<uint8_t*>(data);
    for (size_t i = 0; i + 1 < bytes; i += 2) std::swap(p[i], p[i + 1]);
}

void swap32InPlace(void* data, size_t bytes) {
    auto* p = static_cast<uint8_t*>(data);
    for (size_t i = 0; i + 3 < bytes; i += 4) {
        std::swap(p[i], p[i + 3]);
        std::swap(p[i + 1], p[i + 2]);
    }
}

// ---------------------------------------------------------------------------
// PI — cartridge
// ---------------------------------------------------------------------------

bool piMountRomImage(const uint8_t* data, size_t size) {
    if (!data || size < 0x1000) return false;

    // Detect byte order from the magic in the first four bytes and normalise to
    // .z64. Every offset in imagelist.u.csv and scripts/filelist.u.csv is
    // against the .z64 layout, so mounting a .v64 without normalising extracts
    // garbage from addresses that look perfectly plausible.
    RomByteOrder order = RomByteOrder::Unknown;
    if (data[0] == 0x80 && data[1] == 0x37 && data[2] == 0x12 && data[3] == 0x40)
        order = RomByteOrder::BigEndian;
    else if (data[0] == 0x37 && data[1] == 0x80 && data[2] == 0x40 && data[3] == 0x12)
        order = RomByteOrder::ByteSwapped;
    else if (data[0] == 0x40 && data[1] == 0x12 && data[2] == 0x37 && data[3] == 0x80)
        order = RomByteOrder::LittleEndian;
    else
        return false;

    g_rom.assign(data, data + size);
    if (order == RomByteOrder::ByteSwapped) swap16InPlace(g_rom.data(), g_rom.size());
    else if (order == RomByteOrder::LittleEndian) swap32InPlace(g_rom.data(), g_rom.size());

    g_rom_info.mounted = true;
    g_rom_info.order = order;
    g_rom_info.size = size;
    // Internal name lives at 0x20, 20 bytes, space-padded.
    std::memcpy(g_rom_info.internal_name, g_rom.data() + 0x20, 20);
    g_rom_info.internal_name[20] = '\0';
    for (int i = 19; i >= 0 && g_rom_info.internal_name[i] == ' '; --i)
        g_rom_info.internal_name[i] = '\0';
    return true;
}

bool piMountRom(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0) { std::fclose(f); return false; }

    // Note the explicit count-and-value form: `vector<uint8_t> buf(size_t(size))`
    // is a function declaration, not a variable (most vexing parse).
    std::vector<uint8_t> buf(static_cast<size_t>(size), 0u);
    const size_t got = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    if (got != buf.size()) return false;

    if (!piMountRomImage(buf.data(), buf.size())) return false;
    g_rom_info.path = path;
    return true;
}

void piUnmount() {
    g_rom.clear();
    g_rom_info = RomInfo{};
}

void piSetDevAddrTranslator(PiDevAddrTranslator fn) { g_devaddr_xlat = fn; }

const uint8_t* piRomData() { return g_rom.empty() ? nullptr : g_rom.data(); }

const RomInfo& piRomInfo() { return g_rom_info; }

namespace {

s32 doDma(s32 direction, u32 devAddr, void* dramAddr, u32 size) {
    if (!dramAddr || size == 0) return -1;

    // THREE THINGS ARRIVE HERE AS `devAddr`, AND THE ORDER MATTERS.
    //
    //   1. a host address inside an asset symbol -- the game takes &LgunE and
    //      feeds it to the PI, because on N64 a symbol's address IS its
    //      cartridge position;
    //   2. a 0x10000000-based PI bus address;
    //   3. a bare ROM offset.
    //
    // Ask the translator FIRST and about the untouched address, then fall back
    // to the bus-base strip. Doing the strip first would corrupt case 1 for any
    // asset linked above 0x10000000 -- which, after the image was relocated to
    // 0x20000000 to keep host addresses out of the ROM offset space, is every
    // single one of them.
    //
    // The translator is only safe because those two spaces are now disjoint;
    // geAssetsCheckAddressSpace() refuses to start the game otherwise. Before
    // that, 548 of 821 genuine ROM offsets were captured here and silently
    // rewritten to a different offset.
    u32 rom_off = devAddr;
    bool translated_ok = false;
    if (g_devaddr_xlat) {
        u32 translated = 0;
        if (g_devaddr_xlat(reinterpret_cast<const void*>(uintptr_t(devAddr)),
                           &translated)) {
            rom_off = translated;
            translated_ok = true;
        }
    }
    if (!translated_ok && rom_off >= 0x10000000u) rom_off -= 0x10000000u;

    // THE DESTINATION DOES NOT HAVE TO BE IN RDRAM.
    //
    // This used to reject any dramAddr outside the 8 MB window, on the grounds
    // that a DMA into a bogus pointer should be caught here rather than
    // corrupt memory and surface three subsystems away. That reasoning was
    // right for display lists and wrong for DMA, and it stopped the boot:
    //
    //     [ge-ultra] PI DMA to a non-RDRAM address (0x535C80, 896 bytes)
    //
    // 0x535C80 is one of the game's own statics. On N64 every static lives in
    // RDRAM, so "DMA destination" and "RDRAM address" were the same thing. In
    // this port the game's statics are in the host executable's BSS, and only
    // the memory the game explicitly carves out of RDRAM is inside the window.
    // So a DMA destination is simply a CPU pointer, and the RDRAM check was
    // testing something that is no longer true.
    //
    // Physical addressing still matters — display lists carry 32-bit physical
    // addresses and virtualToPhysical() still has to work for those. That is a
    // separate concern from where a DMA may land.
    //
    // Null and zero-size are still rejected above, which is the check that
    // actually catches a bad pointer.

    if (direction == OS_READ) {
        if (!g_rom_info.mounted) {
            // No ROM: zero-fill rather than leaving stale bytes. A port running
            // without assets should see empty data, not whatever was there.
            std::memset(dramAddr, 0, size);
            return 0;
        }
        if (uint64_t(rom_off) + size > g_rom.size()) {
            // REFUSE WITHOUT WRITING.
            //
            // This used to zero `size` bytes "rather than leaving stale bytes",
            // which sounds careful and is the opposite. `size` is precisely the
            // quantity that is untrusted here -- the reason the read was
            // refused -- so honouring it turns a caught error into the overflow
            // it caught. Observed: a bad texture offset asked for 388368 bytes
            // into a 4000-byte stack buffer, and the memset smashed the stack
            // in a caller three frames up, long after this line had correctly
            // printed a diagnostic.
            //
            // The caller gets -1. A caller that ignores it gets its own buffer
            // untouched, which is recoverable; a smashed return address is not.
            std::fprintf(stderr,
                         "[ge-ultra] PI DMA reads past end of ROM "
                         "(offset 0x%X + %u > 0x%zX) -- refused, target untouched\n",
                         rom_off, size, g_rom.size());
            return -1;
        }
        // RAW COPY — no byte swapping. See the long note in os_io.h.
        std::memcpy(dramAddr, g_rom.data() + rom_off, size);
        return 0;
    }

    // OS_WRITE targets SRAM/FlashRAM on hardware. GoldenEye's saves go through
    // the controller pak, not the cartridge, so this is only reachable from
    // src/usb.c (the 64Drive debug path), which the port does not build.
    std::fprintf(stderr, "[ge-ultra] PI DMA write ignored (dev 0x%X, %u bytes)\n",
                 rom_off, size);
    return -1;
}

}  // namespace
}  // namespace ge_ultra

using namespace ge_ultra;

extern "C" OSPiHandle* osCartRomInit(void) {
    g_cart_handle.type = 0;
    g_cart_handle.baseAddress = 0x10000000u;
    g_cart_handle.domain = 0;
    return &g_cart_handle;
}

extern "C" s32 osPiStartDma(OSIoMesg* mb, s32 pri, s32 direction, u32 devAddr,
                            void* dramAddr, u32 size, OSMesgQueue* mq) {
    (void)pri;
    const s32 r = doDma(direction, devAddr, dramAddr, size);

    if (mb) {
        mb->hdr.status = u8(r == 0 ? 0 : 1);
        mb->dramAddr = dramAddr;
        mb->devAddr = devAddr;
        mb->size = size;
    }

    // The transfer completed synchronously — a host memcpy is instant, and
    // pretending otherwise would mean modelling PI latency for no benefit. But
    // the completion message still has to be posted, because game code does
    // osPiStartDma() then blocks on osRecvMesg() (src/ramrom.c:34,
    // src/audi.c:710). Posting first means the recv finds it already waiting
    // and returns without blocking, which is the behaviour the game expects,
    // just faster.
    if (mq) osSendMesg(mq, mb, OS_MESG_NOBLOCK);
    return r;
}

extern "C" s32 __osPiRawStartDma(s32 direction, u32 devAddr, void* dramAddr,
                                 u32 size) {
    return doDma(direction, devAddr, dramAddr, size);
}

extern "C" u32 osPiGetStatus(void) {
    // Always idle. src/init.c:133 spins on `while (osPiGetStatus() &
    // PI_STATUS_DMA_BUSY)` — returning busy here would hang the boot on a
    // cooperative scheduler, because nothing else can run to clear it.
    return 0;
}

// ---------------------------------------------------------------------------
// VI
// ---------------------------------------------------------------------------

extern "C" void osViInit(void) { g_vi = ViState{}; }
extern "C" void osViSetMode(void* mode) { (void)mode; }

extern "C" void osViSetEvent(OSMesgQueue* mq, OSMesg msg, u32 retraceCount) {
    g_vi_queue = mq;
    g_vi_msg = msg;
    g_vi_retrace_interval = retraceCount ? retraceCount : 1;
    osSetEventMesg(7 /* OS_EVENT_VI */, mq, msg);
}

extern "C" void osViSwapBuffer(void* framebuffer) {
    // Record the request; the frame loop latches it. On hardware the swap took
    // effect at the next retrace, so deferring is faithful as well as convenient.
    g_vi.next_framebuffer = virtualToPhysical(framebuffer);
    g_vi.swap_pending = true;
}

extern "C" void* osViGetCurrentFramebuffer(void) {
    return physicalToVirtual(g_vi.current_framebuffer);
}

extern "C" void* osViGetNextFramebuffer(void) {
    return physicalToVirtual(g_vi.next_framebuffer);
}

extern "C" void osViBlack(u8 active) { g_vi.black = active != 0; }
extern "C" void osViSetSpecialFeatures(u32 func) { (void)func; }
extern "C" void osViSetXScale(f32 v) { g_vi.x_scale = v; }
extern "C" void osViSetYScale(f32 v) { g_vi.y_scale = v; }
extern "C" void osViRepeatLine(u8 active) { (void)active; }

namespace ge_ultra {

const ViState& viState() { return g_vi; }

uint32_t viLatchFramebuffer() {
    if (!g_vi.swap_pending) return 0;

    /*
     * current CATCHES UP to next. next is deliberately left alone.
     *
     * libultra's VI manager keeps two blocks, __osViCurr and __osViNext, and at
     * retrace it copies next over current -- so once the swap has taken effect
     * both name the SAME buffer. osViGetCurrentFramebuffer() and
     * osViGetNextFramebuffer() then agree, and that agreement is a signal the
     * game reads:
     *
     *     if (osViGetCurrentFramebuffer() != osViGetNextFramebuffer())
     *         return 0;               // sched.c:419, __scTaskReady
     *
     * The first version of this cleared next to 0 to mark the swap consumed.
     * physicalToVirtual(0) is NULL and current is not, so the two never agreed
     * again, __scTaskReady returned "not ready" for the rest of the run, and
     * the game submitted exactly three graphics tasks and then stopped for
     * good -- while audio carried on, which made it look like a rendering
     * problem rather than a scheduler one.
     */
    g_vi.current_framebuffer = g_vi.next_framebuffer;
    g_vi.swap_pending = false;
    return g_vi.current_framebuffer;
}

}  // namespace ge_ultra

// ---------------------------------------------------------------------------
// SI — controllers
// ---------------------------------------------------------------------------

extern "C" s32 osContInit(OSMesgQueue* mq, u8* bitpattern, OSContStatus* status) {
    g_cont_queue = mq;
    u8 bits = 0;
    for (int i = 0; i < MAXCONTROLLERS; ++i) {
        if (status) {
            status[i].type = g_pads[i].connected ? CONT_TYPE_NORMAL : 0;
            status[i].status = 0;
            status[i].errnum = g_pads[i].connected ? 0 : 8 /* NO_RESPONSE */;
        }
        if (g_pads[i].connected) bits |= u8(1 << i);
    }
    if (bitpattern) *bitpattern = bits;
    return 0;
}

extern "C" s32 osContStartReadData(OSMesgQueue* mq) {
    g_cont_read_pending = true;
    // Same reasoning as PI: the read is instant, but the completion message is
    // still posted because src/joy.c blocks on it.
    if (mq) osSendMesg(mq, nullptr, OS_MESG_NOBLOCK);
    return 0;
}

extern "C" void osContGetReadData(OSContPad* pad) {
    if (!pad) return;
    for (int i = 0; i < MAXCONTROLLERS; ++i) {
        if (g_pads[i].connected) {
            pad[i] = g_pads[i].pad;
            pad[i].errnum = 0;
        } else {
            pad[i] = OSContPad{};
            pad[i].errnum = 8;  // CONT_NO_RESPONSE_ERROR
        }
    }
    g_cont_read_pending = false;
}

extern "C" s32 osContStartQuery(OSMesgQueue* mq) {
    if (mq) osSendMesg(mq, nullptr, OS_MESG_NOBLOCK);
    return 0;
}

extern "C" void osContGetQuery(OSContStatus* status) {
    if (!status) return;
    for (int i = 0; i < MAXCONTROLLERS; ++i) {
        status[i].type = g_pads[i].connected ? CONT_TYPE_NORMAL : 0;
        status[i].status = 0;
        status[i].errnum = g_pads[i].connected ? 0 : 8;
    }
}

extern "C" s32 osMotorInit(OSMesgQueue* mq, void* pfs, int channel) {
    (void)mq;
    (void)pfs;
    return (channel >= 0 && channel < MAXCONTROLLERS && g_pads[channel].connected)
               ? 0
               : 5 /* PFS_ERR_NOPACK */;
}

extern "C" s32 osMotorStart(void* pfs) {
    // The pak handle does not carry a channel in a form we can read portably,
    // so rumble is applied to port 0. GoldenEye is single-controller for
    // rumble purposes in the VR build.
    // TODO(M1): thread the channel through when the port defines OSPfs.
    (void)pfs;
    g_pads[0].motor = true;
    return 0;
}

extern "C" s32 osMotorStop(void* pfs) {
    (void)pfs;
    g_pads[0].motor = false;
    return 0;
}

namespace ge_ultra {

void siSetPad(int port, const OSContPad& pad) {
    if (port < 0 || port >= MAXCONTROLLERS) return;
    g_pads[port].pad = pad;
}

void siSetConnected(int port, bool connected) {
    if (port < 0 || port >= MAXCONTROLLERS) return;
    g_pads[port].connected = connected;
}

bool siMotorActive(int port) {
    if (port < 0 || port >= MAXCONTROLLERS) return false;
    return g_pads[port].motor;
}

// ---------------------------------------------------------------------------
// Frame driver
// ---------------------------------------------------------------------------

FrameResult hostRunFrame(OSTime cycles_per_field) {
    FrameResult r{};

    // Order matters. Timers first: the game arms timers relative to retrace and
    // expects them to have fired by the time its retrace handler runs.
    advanceTime(cycles_per_field);

    g_vi.retrace_count++;

    /*
     * Latch BEFORE the interrupt, not after. On hardware the VI reloads its
     * origin register at the vertical blank and only then raises the retrace
     * interrupt, so the handler -- and everything it wakes -- already sees the
     * new buffer as current. Latching afterwards left the scheduler looking at
     * a swap that was still pending for the whole frame it was meant to have
     * completed in.
     */
    r.framebuffer = viLatchFramebuffer();

    if (g_vi.retrace_count % g_vi_retrace_interval == 0) sendEvent(7 /* VI */);

    r.context_switches = runUntilIdle();
    r.game_is_idle = (r.context_switches == 0 && r.framebuffer == 0);
    return r;
}

void ioReset() {
    piUnmount();
    g_vi = ViState{};
    g_vi_queue = nullptr;
    g_vi_msg = nullptr;
    g_vi_retrace_interval = 1;
    for (auto& p : g_pads) p = PadSlot{};
    g_cont_queue = nullptr;
    g_cont_read_pending = false;
}

}  // namespace ge_ultra
