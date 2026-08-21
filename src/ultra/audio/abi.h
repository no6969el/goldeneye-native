// abi.h — the N64 audio command list.
//
// Structurally identical to the graphics situation, and the same design follows:
//
//     game code (audi.c, snd.c, music.c)          unmodified
//       -> alAudioFrame()  (libultra audio lib)   unmodified C, already in repo
//         -> Acmd list in RDRAM
//           -> ge_audio::Interpreter              REPLACES the aspMain microcode
//             -> 16-bit stereo PCM
//               -> osAiSetNextBuffer()            AI shim -> host device
//
// The good news, checked rather than assumed: GoldenEye uses the STOCK audio
// library. src/libultra/audio/ contains the ordinary synthesizer.c / seqplayer.c
// / csplayer.c sources, and include/PR/abi.h is the unmodified Nintendo ABI with
// no GoldenEye additions — unlike gbi_extension.h, there is no
// abi_extension.h. GoldenEye's own layer (audi.c, snd.c, music.c) sits ABOVE
// alAudioFrame and never touches the command list.
//
// So unlike G_TRI4, there is nothing bespoke to reverse-engineer here. There
// are sixteen well-specified commands and the work is implementing them
// correctly.

#pragma once

#include <cstdint>

namespace ge_audio {

enum : uint8_t {
    A_SPNOOP     = 0,
    A_ADPCM      = 1,
    A_CLEARBUFF  = 2,
    A_ENVMIXER   = 3,
    A_LOADBUFF   = 4,
    A_RESAMPLE   = 5,
    A_SAVEBUFF   = 6,
    A_SEGMENT    = 7,
    A_SETBUFF    = 8,
    A_SETVOL     = 9,
    A_DMEMMOVE   = 10,
    A_LOADADPCM  = 11,
    A_MIXER      = 12,
    A_INTERLEAVE = 13,
    A_POLEF      = 14,
    A_SETLOOP    = 15,
};

// Flags. Note A_OUT/A_LOOP and A_LEFT share bit 1, and A_MAIN/A_RATE are both
// zero — the meaning depends entirely on which command is reading them. This is
// a frequent source of confusion; each handler documents which set applies.
enum : uint8_t {
    A_INIT     = 0x01,
    A_CONTINUE = 0x00,
    A_LOOP     = 0x02,
    A_OUT      = 0x02,
    A_LEFT     = 0x02,
    A_RIGHT    = 0x00,
    A_VOL      = 0x04,
    A_RATE     = 0x00,
    A_AUX      = 0x08,
    A_NOAUX    = 0x00,
    A_MAIN     = 0x00,
    A_MIX      = 0x10,
};

// One audio command: two big-endian-on-N64 words, read here as native u32 after
// the loader-side swap (see os_io.h).
struct Acmd {
    uint32_t w0;
    uint32_t w1;
};
static_assert(sizeof(Acmd) == 8, "Acmd must be 8 bytes");

// Audio DMEM. The RSP's data memory is 4 KiB and the audio microcode uses all
// of it as a scratch mixing area addressed in BYTES. Sample data inside it is
// s16, so a "sample offset" is a byte offset / 2 — mixing those up produces
// audio that is half-speed, chipmunked, or silent depending on which way you got
// it wrong.
constexpr int kAudioDmemSize = 0x1000;

// VADPCM: 9-byte frames (1 header + 8 data) decode to 16 samples. The header's
// high nibble is a shift, the low nibble selects a predictor set from the
// codebook loaded by A_LOADADPCM.
constexpr int kAdpcmFrameBytes = 9;
constexpr int kAdpcmSamplesPerFrame = 16;
constexpr int kAdpcmOrder = 2;      // taps of history the predictor uses
constexpr int kAdpcmMaxPredictors = 8;

}  // namespace ge_audio
