/*
 * ge_addr_compat.h -- turning an N64 address back into a pointer, safely.
 *
 * The port maps RDRAM at KSEG0, so an N64 address IS a valid host address --
 * as a VALUE. What does not survive is the game storing that value in an `s32`,
 * which the decomp does constantly because on the N64 it is lossless:
 *
 *     config->anim.anim = (struct ModelAnimation *)(off + (s32)ptr_animation_table);
 *
 * RDRAM starts at 0x80000000, so the top bit is set and the value is negative
 * as s32. Widening it back to a 64-bit pointer sign-extends:
 *
 *     0x807036D4  ->  0xFFFFFFFF807036D4  ->  SIGSEGV
 *
 * It fails for exactly the half of RDRAM with the top bit set, which is why it
 * presents as intermittent rather than as an obvious porting break.
 *
 * These two macros are for use ONLY inside `#ifdef GE_HOST_PORT` branches. They
 * are deliberately not applied to the N64 path: this is a matching decomp, and
 * the rule (patches/HOST-PORT-PATCHES.md 5) is that the IDO token stream must be
 * identical, which is guaranteed by leaving the original line verbatim in the
 * `#else`. A macro that expanded "to the same thing" would be an argument; the
 * #else is a fact.
 */
#ifndef GE_ADDR_COMPAT_H
#define GE_ADDR_COMPAT_H

#include <stdint.h>

/* An address as the 32-bit quantity the game thinks it is. Zero-extends. */
#define GE_U32(x)     ((u32)(uintptr_t)(x))

/* A 32-bit N64 address back to a real pointer, without sign extension. */
#define GE_PTR(T, x)  ((T)(uintptr_t)(u32)(x))

/*
 * An N64 RELOCATION: base + offset, computed in 32 bits.
 *
 * libaudio patches a bank file from offsets to pointers with
 *
 *     ptr = (T *)((u8 *)ptr + offset);      offset = (s32)file;
 *
 * and on the N64 both sides are 32 bits, so the sum WRAPS -- and that wrap is
 * the whole mechanism: a small file-relative offset plus an RDRAM base like
 * 0x8078B860 comes back out as the right address only because the top bits are
 * discarded.
 *
 * On a 64-bit host there is nothing to wrap against. `offset` is a negative
 * s32, it sign-extends, and the sum lands at 0xFFFFFFFF8xxxxxxx. Doing the
 * arithmetic in u32 and converting once at the end restores the N64's
 * behaviour exactly, rather than approximating it.
 */
#define GE_RELOC(T, base, off)  GE_PTR(T, GE_U32(base) + (u32)(off))

#endif /* GE_ADDR_COMPAT_H */
