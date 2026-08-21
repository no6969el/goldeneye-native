// test_io.cpp — PI (cartridge), VI (video), SI (pads), and the frame driver.
//
// The scenario at the bottom is the important one: a thread structured the way
// GoldenEye's main thread is — block on retrace, read pads, DMA an asset, draw,
// swap — driven by hostRunFrame(). If that works, the shim can boot the game.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ultra/os.h"
#include "ultra/os_io.h"
#include "ultra/rdram.h"

using namespace ge_ultra;

static int g_failures = 0;
static void check(bool cond, const char* what) {
    if (!cond) { std::printf("  FAIL: %s\n", what); ++g_failures; }
    else std::printf("  ok:   %s\n", what);
}

// Build a fake ROM: valid .z64 magic, an internal name, and a recognisable
// payload at a known offset.
static std::vector<uint8_t> makeRom(size_t size = 0x100000) {
    std::vector<uint8_t> rom(size, 0);
    rom[0] = 0x80; rom[1] = 0x37; rom[2] = 0x12; rom[3] = 0x40;
    const char* name = "GOLDENEYE           ";
    std::memcpy(rom.data() + 0x20, name, 20);
    for (int i = 0; i < 256; ++i) rom[0x1000 + i] = uint8_t(i);
    return rom;
}

// ---------------------------------------------------------------------------
// 1. Byte-order normalisation.
//
// Every offset in imagelist.u.csv is against the .z64 layout. Mounting a .v64
// without normalising extracts garbage from addresses that look fine.
// ---------------------------------------------------------------------------
static void testRomByteOrder() {
    std::printf("[ROM byte-order detection and normalisation]\n");

    const std::vector<uint8_t> z64 = makeRom();

    check(piMountRomImage(z64.data(), z64.size()), "mounts .z64");
    check(piRomInfo().order == RomByteOrder::BigEndian, "detects big-endian");
    check(std::string(piRomInfo().internal_name) == "GOLDENEYE",
          "reads internal name, trailing spaces trimmed");

    // .v64: byte pairs swapped.
    std::vector<uint8_t> v64 = z64;
    swap16InPlace(v64.data(), v64.size());
    check(v64[0] == 0x37 && v64[1] == 0x80, "test .v64 image is byte-swapped");
    check(piMountRomImage(v64.data(), v64.size()), "mounts .v64");
    check(piRomInfo().order == RomByteOrder::ByteSwapped, "detects .v64");

    // After normalisation a DMA must produce identical bytes to the .z64 mount.
    rdramInit();
    auto* dst = rdramBase() + 0x40000;
    std::memset(dst, 0xFF, 256);
    __osPiRawStartDma(OS_READ, 0x1000, dst, 256);
    bool same = true;
    for (int i = 0; i < 256; ++i) if (dst[i] != uint8_t(i)) same = false;
    check(same, ".v64 normalises to identical bytes as .z64");

    // .n64: words reversed.
    std::vector<uint8_t> n64 = z64;
    swap32InPlace(n64.data(), n64.size());
    check(n64[0] == 0x40 && n64[1] == 0x12, "test .n64 image is word-reversed");
    check(piMountRomImage(n64.data(), n64.size()), "mounts .n64");
    check(piRomInfo().order == RomByteOrder::LittleEndian, "detects .n64");

    std::memset(dst, 0xFF, 256);
    __osPiRawStartDma(OS_READ, 0x1000, dst, 256);
    same = true;
    for (int i = 0; i < 256; ++i) if (dst[i] != uint8_t(i)) same = false;
    check(same, ".n64 normalises to identical bytes as .z64");

    // Garbage must be refused rather than mounted and read as noise.
    std::vector<uint8_t> junk(0x2000, 0xAA);
    check(!piMountRomImage(junk.data(), junk.size()), "refuses unrecognised magic");

    piUnmount();
    rdramShutdown();
}

// ---------------------------------------------------------------------------
// 2. PI DMA: raw copy, bounds, completion messages.
// ---------------------------------------------------------------------------
static void testPiDma() {
    std::printf("[PI DMA]\n");
    bootScheduler();
    rdramInit();
    const std::vector<uint8_t> rom = makeRom();
    piMountRomImage(rom.data(), rom.size());

    auto* dst = rdramBase() + 0x50000;
    std::memset(dst, 0, 256);

    OSMesg buf[4];
    OSMesgQueue q;
    osCreateMesgQueue(&q, buf, 4);
    OSIoMesg io{};

    check(osPiStartDma(&io, OS_MESG_PRI_NORMAL, OS_READ, 0x1000, dst, 256, &q) == 0,
          "DMA succeeds");
    check(dst[0] == 0 && dst[1] == 1 && dst[255] == 255, "bytes copied verbatim");

    // The completion message must be waiting, so a blocking recv returns at once
    // — that is the pattern in src/ramrom.c:34 and src/audi.c:710.
    OSMesg m = nullptr;
    check(osRecvMesg(&q, &m, OS_MESG_NOBLOCK) == 0 && m == &io,
          "completion message posted to the return queue");

    // PI bus addressing: 0x10001000 must reach the same bytes as 0x1000.
    std::memset(dst, 0, 256);
    __osPiRawStartDma(OS_READ, 0x10001000, dst, 256);
    check(dst[1] == 1 && dst[255] == 255, "0x10000000-based PI address accepted");

    // Reading past the end must fail loudly and zero the target, not read
    // whatever follows the ROM buffer in host memory.
    std::memset(dst, 0xAB, 256);
    check(__osPiRawStartDma(OS_READ, u32(rom.size() - 8), dst, 256) == -1,
          "read past end of ROM is refused");
    // A refused read must NOT touch the target. It used to zero `size` bytes,
    // which is the one quantity that is untrusted when a read is refused --
    // see the note in src/ultra/os_io.cpp.
    check(dst[0] == 0xAB, "refused read leaves the target untouched");

    // A DMA destination outside RDRAM is ALLOWED, and this test used to assert
    // the opposite.
    //
    // On the N64 every static lives in RDRAM, so "DMA destination" and "RDRAM
    // address" were the same thing and rejecting anything else was a real
    // check. In this port the game's statics are in the host executable's BSS,
    // so a DMA destination is simply a CPU pointer. P3e removed the check for
    // that reason -- it was rejecting correct calls and stopping the boot -- but
    // this assertion was left behind, and it has been failing ever since while
    // the docs claimed the suite was green.
    //
    // What actually catches a bad pointer is the null/size check below.
    int stack_var = 0;
    check(__osPiRawStartDma(OS_READ, 0x1000, &stack_var, 4) == 0,
          "DMA to a host pointer outside RDRAM is accepted");
    check(__osPiRawStartDma(OS_READ, 0x1000, nullptr, 4) == -1,
          "DMA to a null pointer is still refused");
    check(__osPiRawStartDma(OS_READ, 0x1000, &stack_var, 0) == -1,
          "zero-size DMA is still refused");

    // No ROM mounted: zero-fill rather than stale bytes.
    piUnmount();
    std::memset(dst, 0xCC, 64);
    check(__osPiRawStartDma(OS_READ, 0x1000, dst, 64) == 0 && dst[0] == 0,
          "no ROM mounted: reads return zeroes");

    check(osPiGetStatus() == 0, "PI never reports busy (src/init.c:133 spins on it)");

    rdramShutdown();
    shutdownScheduler();
}

// ---------------------------------------------------------------------------
// 3. VI: swap is deferred to the frame boundary.
// ---------------------------------------------------------------------------
static void testVi() {
    std::printf("[VI framebuffer latching]\n");
    bootScheduler();
    rdramInit();
    ioReset();
    osViInit();

    auto* fb1 = rdramBase() + 0x100000;
    auto* fb2 = rdramBase() + 0x180000;

    check(viLatchFramebuffer() == 0, "no swap requested: latch returns 0");

    osViSwapBuffer(fb1);
    check(viState().current_framebuffer == 0,
          "swap does not take effect immediately (deferred to retrace)");
    check(viLatchFramebuffer() == virtualToPhysical(fb1), "latch promotes it");
    check(osViGetCurrentFramebuffer() == fb1, "current framebuffer reads back");

    check(viLatchFramebuffer() == 0, "second latch with no swap reports a dropped frame");

    osViSwapBuffer(fb2);
    check(viLatchFramebuffer() == virtualToPhysical(fb2), "second buffer latches");

    rdramShutdown();
    shutdownScheduler();
}

// ---------------------------------------------------------------------------
// 4. SI: pads, disconnection, rumble.
// ---------------------------------------------------------------------------
static void testSi() {
    std::printf("[SI controllers]\n");
    bootScheduler();
    ioReset();

    OSContPad p{};
    p.button = CONT_A | CONT_G;
    p.stick_x = 40;
    p.stick_y = -20;
    siSetConnected(0, true);
    siSetPad(0, p);

    u8 bits = 0;
    OSContStatus st[MAXCONTROLLERS]{};
    OSMesg buf[4];
    OSMesgQueue q;
    osCreateMesgQueue(&q, buf, 4);

    osContInit(&q, &bits, st);
    check(bits == 0x1, "only port 0 reports connected");
    check(st[0].type == CONT_TYPE_NORMAL, "port 0 is a standard controller");
    check(st[1].errnum == 8, "port 1 reports no response");

    osContStartReadData(&q);
    OSMesg m = nullptr;
    check(osRecvMesg(&q, &m, OS_MESG_NOBLOCK) == 0,
          "read completion posted (src/joy.c blocks on this)");

    OSContPad pads[MAXCONTROLLERS]{};
    osContGetReadData(pads);
    check(pads[0].button == (CONT_A | CONT_G), "buttons round-trip");
    check(pads[0].stick_x == 40 && pads[0].stick_y == -20, "stick round-trips");
    check(pads[0].errnum == 0, "connected pad has no error");
    check(pads[1].errnum == 8, "disconnected pad reports an error");

    check(!siMotorActive(0), "rumble off by default");
    osMotorStart(nullptr);
    check(siMotorActive(0), "osMotorStart is visible to the port (-> haptics)");
    osMotorStop(nullptr);
    check(!siMotorActive(0), "osMotorStop clears it");

    shutdownScheduler();
}

// ---------------------------------------------------------------------------
// 5. The whole thing: a GoldenEye-shaped main thread driven by hostRunFrame().
//
// This mirrors the real structure — block on retrace, read pads, DMA, draw,
// swap — and is the closest thing to "does the shim boot the game" that can be
// written without the game.
// ---------------------------------------------------------------------------
namespace {
OSMesgQueue g_retrace_q, g_dma_q, g_cont_q;
OSMesg g_retrace_buf[8], g_dma_buf[4], g_cont_buf[4];
OSThread g_main_thread;
int g_frames_drawn = 0;
u16 g_last_buttons = 0;
uint8_t g_first_asset_byte = 0;

void mainThreadBody(void*) {
    static uint8_t* fb[2];
    fb[0] = rdramBase() + 0x200000;
    fb[1] = rdramBase() + 0x280000;

    for (;;) {
        // 1. Wait for retrace. This is the game's frame boundary.
        OSMesg m = nullptr;
        osRecvMesg(&g_retrace_q, &m, OS_MESG_BLOCK);

        // 2. Read controllers.
        osContStartReadData(&g_cont_q);
        osRecvMesg(&g_cont_q, &m, OS_MESG_BLOCK);
        OSContPad pads[MAXCONTROLLERS]{};
        osContGetReadData(pads);
        g_last_buttons = pads[0].button;

        // 3. Stream an asset in, the way src/ramrom.c does.
        static OSIoMesg io;
        auto* scratch = rdramBase() + 0x300000;
        osPiStartDma(&io, OS_MESG_PRI_NORMAL, OS_READ, 0x1000, scratch, 256,
                     &g_dma_q);
        osRecvMesg(&g_dma_q, &m, OS_MESG_BLOCK);
        g_first_asset_byte = scratch[1];

        // 4. Draw, then present.
        osViSwapBuffer(fb[g_frames_drawn & 1]);
        ++g_frames_drawn;
    }
}
}  // namespace

static void testFrameLoop() {
    std::printf("[frame loop: a GoldenEye-shaped main thread]\n");
    bootScheduler();
    rdramInit();
    ioReset();
    osViInit();

    const std::vector<uint8_t> rom = makeRom();
    piMountRomImage(rom.data(), rom.size());

    osCreateMesgQueue(&g_retrace_q, g_retrace_buf, 8);
    osCreateMesgQueue(&g_dma_q, g_dma_buf, 4);
    osCreateMesgQueue(&g_cont_q, g_cont_buf, 4);
    osViSetEvent(&g_retrace_q, (OSMesg)0x5649, 1);

    siSetConnected(0, true);
    OSContPad p{};
    p.button = CONT_START;
    siSetPad(0, p);

    g_frames_drawn = 0;
    osCreateThread(&g_main_thread, 3, mainThreadBody, nullptr, nullptr, 10);
    osStartThread(&g_main_thread);
    runUntilIdle();

    check(g_frames_drawn == 0, "main thread blocks on retrace before the first frame");

    // Ten fields, plus one.
    //
    // The extra field is not padding. hostRunFrame latches the pending
    // framebuffer BEFORE it raises the retrace event, because that is the order
    // the hardware uses: the VI reloads its origin register during the vertical
    // blank and only then interrupts. A swap the game requests during field N is
    // therefore presented at the START of field N+1, so ten swaps need eleven
    // fields to all come out.
    //
    // The previous version of this test latched after the event and expected
    // ten from ten. That ordering also made osViGetNextFramebuffer() disagree
    // with osViGetCurrentFramebuffer() for a whole field, which the game reads
    // as "a swap is still pending" (__scTaskReady, src/sched.c) -- and the port
    // submitted exactly three graphics tasks and then stopped forever.
    uint32_t last_fb = 0;
    int presented = 0;
    for (int i = 0; i < 11; ++i) {
        const FrameResult r = hostRunFrame(kCyclesPerFieldNTSC);
        if (r.framebuffer) { ++presented; last_fb = r.framebuffer; }
    }

    check(g_frames_drawn == 11, "eleven retraces produce eleven frames");
    // Ten, not eleven. The first field latches before the game has drawn
    // anything, so it has nothing to present; each of the ten swaps that follow
    // comes out on the field after the one that requested it.
    check(presented == 10, "every swap was presented, one field later");
    check(g_last_buttons == CONT_START, "controller state reached the game");
    check(g_first_asset_byte == 1, "asset DMA delivered real ROM bytes");
    check(last_fb != 0, "framebuffer address is valid");
    check(osGetTime() == 11 * kCyclesPerFieldNTSC, "virtual clock advanced 11 fields");

    // Input changes must be visible on the very next frame.
    p.button = CONT_A;
    siSetPad(0, p);
    hostRunFrame(kCyclesPerFieldNTSC);
    check(g_last_buttons == CONT_A, "input change visible the next frame");

    // Double-buffering: consecutive frames must alternate targets, or the
    // renderer would be reading the buffer the game is drawing into.
    const uint32_t a = hostRunFrame(kCyclesPerFieldNTSC).framebuffer;
    const uint32_t b = hostRunFrame(kCyclesPerFieldNTSC).framebuffer;
    check(a != b && a != 0 && b != 0, "framebuffers alternate");

    // The deliberate bad-pointer case above no longer produces a failed
    // resolve, because that DMA is now legitimately accepted. Nothing else in
    // this suite should be resolving a bad address.
    check(rdramBadResolveCount() == 0,
          "no unexpected bad address resolutions");

    rdramShutdown();
    shutdownScheduler();
}

int main() {
    testRomByteOrder();
    testPiDma();
    testVi();
    testSi();
    testFrameLoop();

    if (g_failures) {
        std::printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    std::printf("\nall io tests passed\n");
    return 0;
}
