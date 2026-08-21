// os_io.h — host control surface for PI (cartridge), VI (video), SI (pads).
//
// The libultra-facing declarations live in os.h. This is what the PORT calls:
// mounting a ROM, feeding controller state, and reading back what the game
// asked to display.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "os.h"

namespace ge_ultra {

// ---------------------------------------------------------------------------
// PI — the cartridge
// ---------------------------------------------------------------------------

enum class RomByteOrder {
    Unknown,
    BigEndian,     // .z64  — 80 37 12 40. Native cartridge order.
    ByteSwapped,   // .v64  — 37 80 40 12. Pairs of bytes swapped.
    LittleEndian,  // .n64  — 40 12 37 80. Words reversed.
};

struct RomInfo {
    bool         mounted = false;
    RomByteOrder order = RomByteOrder::Unknown;
    size_t       size = 0;
    std::string  path;
    char         internal_name[21] = {};
};

// Mount a ROM image. Any of the three byte orders is accepted and normalised to
// big-endian in memory, because every offset in the decomp's imagelist.u.csv
// and filelist.u.csv is expressed against the .z64 layout.
//
// Returns false if the file cannot be read or the magic is unrecognised.
bool piMountRom(const std::string& path);

// Mount an in-memory image. Used by the tests, and by a port that wants to load
// the ROM itself.
bool piMountRomImage(const uint8_t* data, size_t size);

void piUnmount();
const RomInfo& piRomInfo();

/*
 * The mounted image, normalised to big-endian. Null when nothing is mounted.
 *
 * Exposed so the port can copy assets out of it at startup
 * (src/host/ge_assets_load.c). Deliberately const: the ROM is the user's file
 * and the port only ever reads it.
 */
const uint8_t* piRomData();

/*
 * Install a translation from "host address" to "ROM offset" for PI device
 * addresses. The game passes asset symbol addresses to the PI, which on N64 are
 * cartridge offsets; in this port they are host addresses. The shim stays
 * ignorant of what an asset is -- the port supplies the mapping.
 *
 * Returns non-zero and writes *rom_offset when it recognises the address.
 */
using PiDevAddrTranslator = int (*)(const void* host_addr, uint32_t* rom_offset);
void piSetDevAddrTranslator(PiDevAddrTranslator fn);

// ---------------------------------------------------------------------------
// BYTE ORDER — read this before writing an asset loader.
//
// PI DMA is a RAW BYTE COPY. It does not byte-swap. That is a deliberate
// decision and the alternative is worse:
//
// The game is big-endian; the host is not. Something must swap. But a DMA does
// not know what it is carrying. A Vtx is six s16 fields — a 16-bit swap is
// right. A Gfx is two u32 — a 32-bit swap is right. An I8 texture is bytes —
// no swap is right. A blanket rule at the DMA layer is wrong for two of those
// three, and the failures are silent: geometry that is subtly displaced, or
// textures with their colour channels shuffled.
//
// So swapping belongs at the point where the STRUCTURE is known: the asset
// loader. Three things make that tractable here rather than overwhelming:
//
//   1. ~31 MB of the game's data is already in the repo as C SOURCE. The
//      compiler emits it in host order automatically. It never touches this
//      path at all.
//   2. The ROM-derived binaries are few (textures, audio banks, ramrom demos)
//      and each has a known structure. Texture format in particular is carried
//      per-image in g_Textures[], so the correct swap width is known before the
//      data is read.
//   3. Display lists are BUILT AT RUNTIME by game code compiled for the host,
//      so they are already host-order. Only the interpreter's view has to agree,
//      and it reads through typed structs.
//
// Helpers below exist so loaders declare their intent rather than open-coding
// byte shuffles.
// ---------------------------------------------------------------------------

void swap16InPlace(void* data, size_t bytes);  // s16/u16 arrays, RGBA16, IA16
void swap32InPlace(void* data, size_t bytes);  // u32 arrays, Gfx words, RGBA32

// ---------------------------------------------------------------------------
// VI — video interface
// ---------------------------------------------------------------------------

struct ViState {
    uint32_t current_framebuffer = 0;  // physical address, or 0
    // Set by osViSwapBuffer. AFTER the swap takes effect this equals
    // current_framebuffer -- it is not cleared to 0. That is not a detail: the
    // game asks "is a swap still pending?" by comparing the two
    // (__scTaskReady, src/sched.c:419), so a next of 0 reads as "forever
    // pending" and no graphics task is ever ready again. `swap_pending` is what
    // carries the pending state instead.
    uint32_t next_framebuffer = 0;
    bool     swap_pending = false;
    uint32_t retrace_count = 0;
    bool     black = false;
    float    x_scale = 1.0f;
    float    y_scale = 1.0f;
};

const ViState& viState();

// Called by the frame loop. Promotes the pending framebuffer to current and
// returns its physical address — that is the image the renderer should present.
// Returns 0 if the game did not swap this frame (a dropped frame).
uint32_t viLatchFramebuffer();

// ---------------------------------------------------------------------------
// SI — controllers
// ---------------------------------------------------------------------------

// Push host input for a port. The port fills this from OpenXR
// (ge_vr::synthesizePad) or from a gamepad; the game's own input code
// (src/joy.c and above) is untouched and just sees an OSContPad.
void siSetPad(int port, const OSContPad& pad);
void siSetConnected(int port, bool connected);

// Did the game ask for rumble this frame? The port forwards this to
// geVrHaptic() so recoil is felt in the hand rather than kicking the view.
bool siMotorActive(int port);

// ---------------------------------------------------------------------------
// Frame driver — ties the clock, the retrace event, and the scheduler together.
// ---------------------------------------------------------------------------

struct FrameResult {
    uint32_t framebuffer = 0;      // physical address to present, 0 if none
    int      context_switches = 0;
    bool     game_is_idle = false; // nothing runnable and nothing swapped
};

// Advance one video field: bump the clock, fire timers, deliver the retrace
// event, run every thread until they all block, and report what was drawn.
//
// This is the function the OpenXR frame loop calls. On N64 the VI interrupt
// drove the game; here xrWaitFrame drives this, which is the whole reason
// src/sched.c is replaced rather than ported.
FrameResult hostRunFrame(OSTime cycles_per_field);

// NTSC field time in CPU cycles: 46875000 / 60.
constexpr OSTime kCyclesPerFieldNTSC = 781250;
constexpr OSTime kCyclesPerFieldPAL  = 937500;  // /50

void ioReset();

}  // namespace ge_ultra
