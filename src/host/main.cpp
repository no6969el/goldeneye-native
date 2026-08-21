/*
 * main.cpp — the host entry point.
 *
 * On N64 the boot sequence is: the PIF hands control to boot.s, which sets up
 * the stack, DMAs the code segment in, decompresses it, and calls init(). init()
 * creates the main thread and starts the scheduler; from there the game runs
 * itself.
 *
 * The port replaces the first half of that and keeps the second. There is no
 * PIF, no cartridge bus and no code to DMA — the code is already resident,
 * because it was compiled into this executable. What still has to happen, in
 * this order:
 *
 *   1. RDRAM at 0x80000000, before anything else touches memory. The game
 *      dereferences N64 addresses directly, so nothing is safe until the
 *      mapping exists (src/ultra/rdram.cpp explains why the address is fixed).
 *   2. The ROM mounted, because assets and audio banks are read from it at
 *      runtime by PI DMA, not just at build time.
 *   3. The ROM checked against the segment table. Every ROM offset the port
 *      uses is a constant extracted from one specific build; a different ROM
 *      makes all of them subtly wrong, and the symptom appears much later as
 *      corrupt data rather than as a wrong-file error.
 *   4. RSP task handlers registered, so display lists and audio command lists
 *      have somewhere to go.
 *   5. The scheduler booted, and init() called — the game's own entry point,
 *      unchanged.
 *
 * WHAT THIS IS NOT, YET
 *
 * It links and it starts. Assets are defined but not yet loaded (see
 * src/host/ge_assets.c), and no renderer is attached, so a graphics task
 * reports that it was dropped rather than drawing. Both of those are named
 * loudly at startup rather than left to be discovered.
 */

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "ultra/os.h"
#include "ultra/os_io.h"
#include "ultra/rdram.h"

extern "C" {
#include "ge_assets.h"
#include "ge_fault.h"
#include "sha1.h"
#include "ultra/os_sp.h"
#include "ultra/segments.h"

/* The game's own entry point, from src/init.c. Unchanged. */
void init(void);
}

namespace {

/*
 * One NTSC field at the VR4300's 62.5 MHz counter rate: 62500000 / 59.94.
 * hostRunFrame advances the scheduler's clock by this much, which is what makes
 * the game's own timers and osGetTime() behave as they did on hardware.
 */
constexpr uint64_t kCyclesPerNtscField = 1042707;

struct Options {
    std::string rom;
    bool        strict_rom = true;   /* refuse a ROM the segment table does not describe */
    int         frames     = 0;      /* 0 = run until idle */
};

void usage(const char *argv0)
{
    std::printf(
        "usage: %s --rom <file.z64> [--frames N] [--any-rom]\n"
        "\n"
        "  --rom      your own cartridge dump. Nothing is bundled; the port\n"
        "             reads assets out of this file at runtime.\n"
        "  --frames   stop after N frames instead of running until idle.\n"
        "  --any-rom  skip the SHA1 check. Expect corrupt data: every ROM\n"
        "             offset the port uses was extracted from one build.\n",
        argv0);
}

bool parse(int argc, char **argv, Options &o)
{
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--rom" && i + 1 < argc)          { o.rom = argv[++i]; }
        else if (a == "--frames" && i + 1 < argc)  { o.frames = std::atoi(argv[++i]); }
        else if (a == "--any-rom")                 { o.strict_rom = false; }
        else                                       { return false; }
    }
    return !o.rom.empty();
}

/*
 * Placeholder RSP handlers.
 *
 * They exist so the boot path can be exercised before RT64 is attached, and
 * they COUNT rather than silently discarding: "the game issued 300 graphics
 * tasks and nothing was drawn" and "the game issued none" are completely
 * different bugs, and without a counter they look identical from outside.
 */
unsigned g_gfx_seen = 0;
unsigned g_aud_seen = 0;

extern "C" void geGfxProbeHandler(struct OSTask_s *task, void *user);

void onGraphicsTask(struct OSTask_s *task, void *user)
{
    ++g_gfx_seen;
    /*
     * Walk the list rather than discard it. Counting tasks proves the plumbing;
     * walking proves the DATA. See src/host/ge_gfx_probe.cpp.
     */
    geGfxProbeHandler(task, user);
}
void onAudioTask(struct OSTask_s *, void *)    { ++g_aud_seen; }

}  // namespace

int main(int argc, char **argv)
{
    Options opt;
    if (!parse(argc, argv, opt)) {
        usage(argv[0]);
        return 2;
    }

    /*
     * 0. Fault reporting, before anything can fault.
     *
     * The port's remaining known bug class is RDRAM pointers truncated to s32
     * and sign-extended back. They all fault at a recognisable address and the
     * default handler says only "Segmentation fault". See src/host/ge_fault.c.
     */
    geFaultInstall();

    /* 1. Memory, before anything else. */
    if (!ge_ultra::rdramInit()) {
        std::fprintf(stderr, "fatal: could not map RDRAM\n");
        return 1;
    }
    std::printf("RDRAM  : 8 MB at %p\n", (void *)ge_ultra::rdramBase());

    /* 2. The ROM. */
    if (!ge_ultra::piMountRom(opt.rom)) {
        std::fprintf(stderr, "fatal: could not mount ROM '%s'\n", opt.rom.c_str());
        return 1;
    }
    const ge_ultra::RomInfo &rom = ge_ultra::piRomInfo();
    char sha1[41] = {};
    if (!geSha1File(opt.rom.c_str(), sha1)) {
        std::fprintf(stderr, "fatal: could not re-read ROM for hashing\n");
        return 1;
    }
    std::printf("ROM    : %s (%zu bytes, \"%s\")\n         sha1 %s\n",
                opt.rom.c_str(), rom.size, rom.internal_name, sha1);

    /*
     * 3. Is it the ROM the segment table describes?
     *
     * This is checked at startup rather than trusted, because the failure mode
     * is so poor: a different revision has the same size and the same header,
     * and every ROM offset the port holds would be silently wrong.
     */
    if (!geSegmentsCheckRom(sha1)) {
        std::fprintf(stderr,
            "%s: ROM does not match the segment table.\n"
            "  expected SHA1 %s\n"
            "  got          %s\n"
            "  Every ROM offset this port uses was extracted from that build,\n"
            "  so a different one produces corrupt assets rather than an error.\n",
            opt.strict_rom ? "fatal" : "warning",
            geSegmentsExpectedRomSha1(), sha1);
        if (opt.strict_rom) {
            return 1;
        }
    }

    /*
     * Assets, straight out of the ROM the user supplied. Anything skipped is
     * reported: a silent field of zeros looks exactly like a rendering bug once
     * something is on screen, and "we loaded 805 of 821" is a very different
     * situation from "we loaded all of them".
     */
    {
        unsigned skipped = 0;
        const unsigned loaded = geAssetsLoad(
            static_cast<const unsigned char *>(ge_ultra::piRomData()),
            static_cast<unsigned>(rom.size), &skipped);
        std::printf("assets : %u loaded from ROM", loaded);
        if (skipped) {
            std::printf(", %u SKIPPED (no ROM offset, or out of range)", skipped);
        }
        std::printf("\n");
    }

    /*
     * Before installing the translator: prove the two address spaces cannot be
     * confused.
     *
     * The translator rewrites a device address that lands inside an asset. That
     * is only safe while no address the game means as a ROM OFFSET can land
     * inside an asset too. With the default load address they overlap almost
     * completely -- 548 of 821 asset offsets were captured and rewritten, none
     * of them back to themselves -- and the port loaded the wrong third of the
     * ROM without saying anything. Disjointness comes from a linker flag
     * (tools/link_game.sh), and a linker flag is precisely the kind of thing a
     * later build-script edit drops silently. So it is checked, once, here.
     */
    {
        GeAssetSpan span = {};
        if (!geAssetsCheckAddressSpace(static_cast<unsigned>(rom.size), &span)) {
            std::fprintf(stderr,
                "fatal: asset symbols overlap the ROM address space.\n"
                "  assets   %p .. %p\n"
                "  ROM      0x00000000 .. 0x%08X (and 0x10000000-based)\n"
                "  %u assets conflict.\n"
                "  The image must be linked above the ROM: -no-pie with\n"
                "  -Wl,-Ttext-segment=0x20000000 (GCC/clang) or /BASE:0x20000000\n"
                "  (MSVC). Without it a genuine ROM offset is silently rewritten\n"
                "  to a different one and the game's decompressor spins.\n",
                (const void *)span.lo, (const void *)span.hi,
                static_cast<unsigned>(rom.size), span.conflicts);
            return 1;
        }
        std::printf("addrs  : assets %p..%p, clear of the ROM offset space\n",
                    (const void *)span.lo, (const void *)span.hi);
    }

    /*
     * The game passes asset symbol ADDRESSES to the PI as device addresses; on
     * N64 those are cartridge offsets. Teach the DMA path the difference.
     */
    ge_ultra::piSetDevAddrTranslator(
        [](const void *p, uint32_t *off) -> int {
            unsigned int o = 0;
            if (geAssetsRomOffsetFor(p, &o)) { *off = o; return 1; }
            return 0;
        });

    /*
     * 3b. A controller in port 1.
     *
     * Nothing had ever set this, so osContInit reported an empty bit pattern
     * and the game did the correct thing with it: it drew
     * constructor_menu16_nocontrollers ("PLEASE POWER OFF AND ATTACH A
     * CONTROLLER") and stayed there. Rendering that screen is actually
     * evidence the port works -- but it is not the screen anyone wants, and
     * the title sequence cannot advance past it without input.
     *
     * Port 1 only. The game counts ports 2-4 separately, and pretending four
     * controllers are attached would put it into a multiplayer state nobody
     * asked for.
     */
    ge_ultra::siSetConnected(0, true);
    ge_ultra::siSetPad(0, OSContPad{});

    /* 4. Somewhere for RSP work to go. */
    geSpSetGraphicsHandler(onGraphicsTask, nullptr);
    geSpSetAudioHandler(onAudioTask, nullptr);
    std::printf("rsp    : display-list probe on graphics, counter on audio\n");

    /* 5. The game's own boot, unchanged from here on. */
    ge_ultra::bootScheduler();
    std::printf("boot   : calling init()\n");
    std::fflush(stdout);

    init();

    int frames = 0;
    for (;;) {
        const ge_ultra::FrameResult fr =
            ge_ultra::hostRunFrame(kCyclesPerNtscField);
        ++frames;
        if (fr.game_is_idle) {
            std::printf("idle   : nothing runnable after %d frames\n", frames);
            break;
        }
        /*
         * A heartbeat, because "it has not printed anything for 40 seconds" is
         * the same output whether the port is hung, decompressing a 300 KB
         * asset, or making steady progress. The task counts are the useful
         * part: the moment graphics goes above zero, the game is rendering.
         */
        if ((frames % 10) == 0) {
            unsigned g = 0, a = 0, u = 0;
            geSpGetCounts(&g, &a, &u);
            std::printf("  frame %4d   tasks: %u gfx, %u audio\n", frames, g, a);
            std::fflush(stdout);

            /*
             * A graphics stall is not the same failure as a hang, and the
             * heartbeat alone cannot tell them apart: audio keeps ticking, the
             * frame counter keeps climbing, and the port looks alive while the
             * game thread is parked forever on a message that never comes.
             * That is what the run did -- 3 graphics tasks, then 31 million
             * frames of nothing.
             *
             * Report ONCE. A dump every ten frames would produce megabytes and
             * bury the first one, which is the only one that describes the
             * moment the stall began rather than the steady state after it.
             */
            static bool stall_reported = false;
            static unsigned last_gfx = 0;
            static int last_gfx_frame = 0;
            if (g != last_gfx) {
                last_gfx = g;
                last_gfx_frame = frames;
            } else if (!stall_reported && g > 0 &&
                       frames - last_gfx_frame >= 120) {
                stall_reported = true;
                std::printf("STALL  : no graphics task for %d frames "
                            "(audio still running -- the game thread is "
                            "blocked, not the scheduler)\n",
                            frames - last_gfx_frame);
                ge_ultra::dumpThreads("graphics stalled");
                std::fflush(stdout);
            }
        }
        if (opt.frames && frames >= opt.frames) {
            break;
        }
    }

    ge_ultra::dumpThreads("at exit");

    unsigned gfx = 0, aud = 0, unknown = 0;
    geSpGetCounts(&gfx, &aud, &unknown);
    std::printf("frames : %d\n"
                "tasks  : %u graphics, %u audio, %u unrecognised\n",
                frames, gfx, aud, unknown);

    ge_ultra::piUnmount();
    ge_ultra::rdramShutdown();
    return 0;
}
