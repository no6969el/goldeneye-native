/*
 * os_tlb.c — the TLB, the inflate trampoline, and two BSD functions Windows
 * does not have.
 *
 * WHY THE TLB DOES NOT NEED EMULATING
 *
 * GoldenEye is unusual among N64 games in leaning on the MIPS TLB: its code and
 * several data segments live at virtual addresses like 0x70000000 and
 * 0x7F000000, mapped on demand, with a miss handler installed at the exception
 * vector. That is why hostcompat/ge_segments.h contains virtual addresses that
 * are nowhere near RDRAM.
 *
 * On a host, every one of those addresses is already reachable — the port maps
 * the whole 8 MB of RDRAM flat and resolves segment references through the
 * table rather than through hardware translation. A TLB miss cannot occur, so
 * the handler can never run, so there is nothing to install and nothing to
 * evict.
 *
 * That makes these no-ops in the strong sense used throughout this shim: doing
 * nothing is the *correct* behaviour, not a shortcut that will need revisiting.
 * The one piece that is NOT a no-op is tlbRandomGetNext(), which lives in
 * random.c — see the note there for why a stub would be a bad idea.
 */

#include <stdint.h>
#include <string.h>

#include "os_tlb.h"

/*
 * Declared here rather than including <inflate.h>, which would drag the decomp's
 * whole header chain (and its IRIX stubs) into this file. The signature is from
 * src/inflate/inflate.h and is checked against it by the link.
 */
struct huft;
unsigned int decompress_entry(void *src, void *dst, struct huft *hlist);

/* ------------------------------------------------------------------------ */
/* TLB. NO-OP — see the header comment.                                     */
/* ------------------------------------------------------------------------ */

/*
 * On N64 this programs the CP0 Context register so the miss handler can find
 * the page table. No handler, no misses, nothing to point at.
 */
void initTLBPrepareContext(void)
{
}

/*
 * On N64 this is the TLB miss handler itself — raw machine code, copied to the
 * exception vector at boot by src/init.c. The copy is guarded out on the host
 * (reading a host function's bytes as data would be meaningless), so this
 * exists only to satisfy the address-of in that guarded code and any other
 * reference.
 *
 * If it is ever actually called, something has gone badly wrong.
 */
void resolve_TLBaddress_for_InvalidHit(void)
{
}

/* ------------------------------------------------------------------------ */
/* The inflate trampoline. REAL — forwards to the game's own decompressor.  */
/* ------------------------------------------------------------------------ */

/*
 * jump_decompressfile is a boot-time trampoline in src/boot.s. On N64 it exists
 * because the inflate code has just been DMA'd into a TLB-mapped segment and is
 * not reachable by a normal call, so boot.s jumps to it by absolute address:
 *
 *     lui  $a3, %hi(decompress_entry)
 *     addiu $a3, $a3, %lo(decompress_entry)
 *     jr   $a3
 *
 * It is a tail jump with the arguments untouched, so it is exactly
 * decompress_entry(a0, a1, a2) and nothing else.
 *
 * On the host the port compiles src/inflate/ directly, so the indirection is
 * unnecessary and this is an ordinary call. The arguments are N64 KSEG0
 * addresses and need NO translation: RDRAM is mapped at 0x80000000, so they are
 * already valid host pointers (see src/ultra/rdram.cpp and
 * tests/test_rdram_map.cpp). That is the whole reason the mapping is at a fixed
 * address rather than wherever the allocator chose.
 */
uint32_t jump_decompressfile(uint32_t source, uint32_t target, uint32_t buffer)
{
    return decompress_entry((void *)(uintptr_t)source,
                            (void *)(uintptr_t)target,
                            (struct huft *)(uintptr_t)buffer);
}

/* ------------------------------------------------------------------------ */
/* BSD string functions. REAL — and needed on Windows, which lacks them.    */
/* ------------------------------------------------------------------------ */

/*
 * glibc still provides bcopy/bzero, so these are dead weight on Linux — but the
 * MSVC CRT does not, and the port targets Windows. Defining them here rather
 * than relying on the platform keeps one less thing that only breaks on the
 * machine you cannot test on.
 *
 * bcopy's arguments are (src, dst) — the OPPOSITE order to memcpy(dst, src).
 * Getting that backwards produces a build that runs and corrupts memory, so it
 * is spelled out rather than assumed. bcopy must also tolerate overlap, which
 * is memmove, not memcpy.
 */
void bcopy(const void *src, void *dst, size_t n)
{
    memmove(dst, src, n);
}

void bzero(void *dst, size_t n)
{
    memset(dst, 0, n);
}

/* ------------------------------------------------------------------------ */
/* libm value. REAL.                                                        */
/* ------------------------------------------------------------------------ */

/*
 * src/libultra/gu/libm_vals.s:
 *
 *     glabel __libm_qnan_f
 *     .word 0x7F810000, 0
 *
 * sinf() and cosf() return it for a non-finite argument (src/libultra/gu/
 * sinf.c:150, cosf.c:131), and guint.h declares it `extern float`.
 *
 * Despite the name it is NOT the canonical quiet NaN. 0x7F810000 has the quiet
 * bit CLEAR — it is a signalling NaN with payload 0x10000. I first wrote
 * __builtin_nanf(""), which is 0x7FC00000: a different value, and one that
 * would have looked completely correct in the source and in any test that only
 * asked "is it NaN".
 *
 * __builtin_nansf("0x10000") reproduces 0x7F810000 exactly — checked by
 * bit-comparing the result rather than by reasoning about it.
 */
float __libm_qnan_f = __builtin_nansf("0x10000");
