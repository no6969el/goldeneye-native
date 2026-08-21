// rdram.h — the flat memory the game lives in.
//
// This is the constraint that shapes the whole port, and getting it wrong is
// not recoverable later:
//
//   Game code does pointer arithmetic on PHYSICAL addresses and writes them
//   into display lists.
//
// src/fr.c:705 does `gSPViewport(gdl++, OS_K0_TO_PHYSICAL(&...->viewports[i]))`.
// src/game/model.c:4651 does `gSPVertex(gdl++, osVirtualToPhysical(vtx1), ...)`.
// The display-list interpreter then has to turn those 32-bit values back into
// something it can read.
//
// So physical addresses must be REAL offsets into a REAL contiguous buffer. You
// cannot hand the game malloc'd blocks and hope: the moment a 64-bit host
// pointer is truncated into a 32-bit display-list word, the data is gone.
//
// One 8 MB allocation (4 MB base + 4 MB Expansion Pak). Physical address ==
// offset from the base. `osVirtualToPhysical` and `OS_K0_TO_PHYSICAL` become
// subtraction rather than bit-masking — see the note below.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace ge_ultra {

// 4 MB base + 4 MB Expansion Pak. GoldenEye ships 4 MB but the decomp's TLB
// code and the debug builds assume 8 is available; allocating 8 costs nothing
// on a host and removes a class of "works until you load Aztec" failures.
constexpr uint32_t kGameRdramSize = 8u * 1024u * 1024u;

/*
 * Four more megabytes above what the game can see, for THREAD STACKS.
 *
 * The game's stacks belong in RDRAM: the renderer takes `Mtxf` and
 * `ModelRenderData` off the stack and writes their addresses into display
 * lists, and osVirtualToPhysical has to be able to translate them. The port
 * cannot carve that out of the low 8 MB -- the game's own allocator runs from
 * 0x8008E360 all the way to 0x807FE000, measured -- so the space is added on
 * top instead.
 *
 * The game never reaches here. Its heap top comes from
 * tlbmanageGetTlbAllocatedBlock(), which is computed from its own constants,
 * not from this number, so growing the mapping does not hand the allocator more
 * room. This region exists only for the port to allocate stacks out of.
 */
constexpr uint32_t kStackRegionBase = kGameRdramSize;
constexpr uint32_t kStackRegionSize = 4u * 1024u * 1024u;

constexpr uint32_t kRdramSize = kGameRdramSize + kStackRegionSize;

// Physical address 0 is a legal RDRAM address on N64, but in ported code a
// zero physical address is overwhelmingly likely to be a null pointer that got
// translated. The first page is reserved so that 0 stays diagnosable.
constexpr uint32_t kReservedLowPage = 0x1000;

bool rdramInit();
void rdramShutdown();

uint8_t* rdramBase();
uint32_t rdramSize();

// Host pointer -> 32-bit physical address. Returns 0 for null or out-of-range,
// which the interpreter treats as unresolvable rather than reading garbage.
uint32_t virtualToPhysical(const void* p);

// 32-bit physical address -> host pointer, bounds-checked. Returns nullptr if
// [addr, addr+len) is not entirely inside RDRAM.
void* physicalToVirtual(uint32_t addr, size_t len = 1);

// Resolver to hand to ge_gbi::Interpreter.
std::function<const void*(uint32_t, size_t)> rdramResolver();

// Diagnostics: how many resolves failed. A nonzero count after a frame means
// the game handed the interpreter an address that isn't in RDRAM — almost
// always a pointer that was never allocated out of the flat buffer.
uint64_t rdramBadResolveCount();
void rdramResetStats();

}  // namespace ge_ultra

// ---------------------------------------------------------------------------
// PORT NOTE — replacing the libultra macros
//
// The original definitions bit-mask, because on N64 a KSEG0 virtual address and
// its physical address differ only in the top bits:
//
//     #define OS_K0_TO_PHYSICAL(x) ((u32)(x) & 0x1FFFFFFF)
//
// That cannot work with 64-bit host pointers. The port must redefine them as
// calls, and every translation unit must see the redefinition — a single file
// that picks up the old macro will silently truncate pointers and produce
// display lists that address nothing:
//
//     #define OS_K0_TO_PHYSICAL(x)  ge_ultra::virtualToPhysical((const void*)(x))
//     #define OS_PHYSICAL_TO_K0(x)  ge_ultra::physicalToVirtual((uint32_t)(x))
//
// Grep for both names across src/ before declaring M1 done. There are also raw
// `& 0x1FFFFFFF` and `| 0x80000000` sites that never went through the macro.
// ---------------------------------------------------------------------------
