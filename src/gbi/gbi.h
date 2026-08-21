// gbi.h — the GoldenEye "gsp3D" display-list dialect.
//
// Findings that define this file (see ../../MICROCODE-SPEC.md for evidence):
//
//   * The game builds against PLAIN F3D, not F3DEX. Neither F3DEX_GBI nor
//     F3DEX_GBI_2 is defined anywhere in the build, so gbi.h resolves to the F3D
//     branch and the microcode's own decode matches F3D packing. This means a
//     16-entry vertex cache, F3D command packing, and NO G_MODIFYVTX /
//     G_BRANCH_Z / G_LOAD_UCODE / G_QUAD.
//
//   * Exactly ONE opcode is nonstandard: G_TRI4 (0xB1), which occupies the slot
//     F3DEX would use for G_TRI2.
//
//   * G_SETTEX (0xC0) is NOT an RSP command. 0xC0 is G_NOOP in the F3D
//     immediate range; the RSP forwards it to the RDP, which discards it. It is
//     expanded on the CPU by texLoadFromGdl() in src/game/tex.c before the list
//     is ever submitted. An interpreter running post-expansion lists never sees
//     it. This is the single most important correction to the original plan.

#pragma once

#include <cstdint>

namespace ge_gbi {

// --- opcodes (F3D immediate range; G_IMMFIRST = -65) ---------------------
enum : uint8_t {
    G_SPNOOP            = 0x00,
    G_MTX               = 0x01,
    G_MOVEMEM           = 0x03,
    G_VTX               = 0x04,
    G_DL                = 0x06,

    G_TRI4              = 0xB1,  // GoldenEye extension. Four triangles/command.

    // 0xB2. Stock F3D, and the reason a renderer written against F3DEX2 cannot
    // draw GoldenEye's sky: F3DEX2 reassigned this slot, so an F3DEX2 decoder
    // either drops the command or decodes it as something else entirely.
    //
    // src/game/sky.c emits it in pairs with G_RDPHALF_1 to stream the extra
    // words of long RDP commands (sky.c:1821-1933, 2344+). MICROCODE-SPEC.md
    // listed 0xB2 as "F3DEX2-only, not present", which was wrong: this port
    // never saw it because it had only ever walked ROOM display lists, and the
    // sky is built at runtime rather than stored per room.
    G_RDPHALF_CONT      = 0xB2,
    G_RDPHALF_2         = 0xB3,
    G_RDPHALF_1         = 0xB4,
    G_LINE3D            = 0xB5,
    G_CLEARGEOMETRYMODE = 0xB6,
    G_SETGEOMETRYMODE   = 0xB7,
    G_ENDDL             = 0xB8,
    G_SETOTHERMODE_L    = 0xB9,
    G_SETOTHERMODE_H    = 0xBA,
    G_TEXTURE           = 0xBB,
    G_MOVEWORD          = 0xBC,
    G_POPMTX            = 0xBD,
    G_CULLDL            = 0xBE,
    G_TRI1              = 0xBF,

    // 0xC0 == G_NOOP == G_SETTEX. See the header comment: CPU-side, not RSP.
    G_NOOP              = 0xC0,

    // RDP commands needing segment fixup on w1.
    G_SETTIMG           = 0xFD,
    G_SETZIMG           = 0xFE,
    G_SETCIMG           = 0xFF,
};

// --- G_MTX parameter bits (F3D encoding, NOT F3DEX's inverted form) ------
enum : uint8_t {
    G_MTX_MODELVIEW  = 0x00,
    G_MTX_PROJECTION = 0x01,
    G_MTX_MUL        = 0x00,
    G_MTX_LOAD       = 0x02,
    G_MTX_NOPUSH     = 0x00,
    G_MTX_PUSH       = 0x04,
};

// --- G_MOVEWORD indices ---------------------------------------------------
enum : uint8_t {
    G_MW_MATRIX     = 0x00,
    G_MW_NUMLIGHT   = 0x02,
    G_MW_CLIP       = 0x04,
    G_MW_SEGMENT    = 0x06,
    G_MW_FOG        = 0x08,
    G_MW_LIGHTCOL   = 0x0A,
    G_MW_PERSPNORM  = 0x0E,
};

// --- geometry mode bits ---------------------------------------------------
enum : uint32_t {
    G_ZBUFFER            = 0x00000001,
    G_TEXTURE_ENABLE     = 0x00000002,
    G_SHADE              = 0x00000004,
    G_SHADING_SMOOTH     = 0x00000200,
    G_CULL_FRONT         = 0x00001000,
    G_CULL_BACK          = 0x00002000,
    G_FOG                = 0x00010000,
    G_LIGHTING           = 0x00020000,
    G_TEXTURE_GEN        = 0x00040000,
    G_TEXTURE_GEN_LINEAR = 0x00080000,
};

// Hard limits, all four independently confirmed against the microcode's DMEM
// map. The cache is 16 because 0x420 + 16*40 == 0x6A0, exactly the DL buffer
// base — the layout tiles with no gap, which is what makes this conclusive.
constexpr int kVertexCacheSize   = 16;
constexpr int kSegmentCount      = 16;
constexpr int kMatrixStackDepth  = 10;   // 0x280 / 0x40; the OS allocates 16
constexpr int kDlStackDepth      = 10;

// One 64-bit display-list command.
struct Gfx {
    uint32_t w0;
    uint32_t w1;
};

// An N64 Vtx as it sits in RDRAM. 16 bytes.
struct Vtx {
    int16_t  x, y, z;
    int16_t  flag;
    int16_t  s, t;
    union {
        struct { uint8_t r, g, b, a; } color;
        struct { int8_t nx, ny, nz; uint8_t a; } normal;
    };
};
static_assert(sizeof(Vtx) == 16, "Vtx must be 16 bytes");

// A 4x4 fixed-point N64 matrix, as stored in RDRAM: 16 bits of integer part for
// all 16 elements, then 16 bits of fraction for all 16. NOT interleaved.
struct Mtx {
    uint16_t intpart[16];
    uint16_t fracpart[16];
};
static_assert(sizeof(Mtx) == 64, "Mtx must be 64 bytes");

void mtxToFloat(const Mtx& in, float out[4][4]);

// ---------------------------------------------------------------------------
// G_TRI4 decode
// ---------------------------------------------------------------------------

struct Tri {
    uint8_t v[3];
};

// Decode a G_TRI4 command into up to four triangles.
//
// The microcode does not loop internally: it consumes ONE triangle, shifts
// w0 >>= 4 / w1 >>= 8, writes the shifted words back into its DMEM command
// buffer, rewinds the DL pointer by 8 and re-dispatches. The loop below is that
// behaviour unrolled.
//
// The termination test is the subtle part and is faithfully reproduced here:
// the microcode branches on the ENTIRE remaining w1 being zero, tested BEFORE
// extracting the current triangle's nibbles. It never looks at the z nibbles.
// So a command whose trailing slots are zero terminates early (the intended
// use), but a command with a zeroed slot FOLLOWED by a nonzero one does not —
// the zeroed slot is emitted as a degenerate triangle. The macro's claim that
// "triangles with all points set to 0 are not drawn" only holds for trailing
// slots. Any interpreter that "helpfully" skips interior zero slots will
// silently disagree with hardware.
//
// Returns the number of triangles written to out[] (0..4).
int decodeTri4(uint32_t w0, uint32_t w1, Tri out[4]);

// Decode a G_TRI1 command. Indices in the command ARE pre-multiplied by 10
// (stock F3D), so they are divided back down here. G_TRI4 indices are raw
// nibbles and are NOT — the microcode restores the *10 form via a 16-entry
// lookup table at DMEM 0x2D0.
Tri decodeTri1(uint32_t w0, uint32_t w1);

}  // namespace ge_gbi
