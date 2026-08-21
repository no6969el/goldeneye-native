/*
 * random.c — the game's RNG, ported from src/random.s.
 *
 * WHY THIS IS NOT A STUB
 *
 * Every other symbol in this directory can be a no-op or an approximation and
 * the game still runs. This one cannot. GoldenEye's RNG drives guard reaction
 * times, weapon spread, animation selection and drop tables; getting it
 * approximately right produces a game that plays subtly wrong in ways nobody
 * can point at. It is also the only piece of hand-written assembly in the game
 * proper, so there is no C to compile instead — it has to be transcribed.
 *
 * THE ALGORITHM
 *
 * A 64-bit xorshift-style generator. src/random.s, translated instruction by
 * instruction:
 *
 *     a2 = dsll32(x, 31)     a2 = x << 63
 *     a1 = dsll  (x, 31)     a1 = x << 31
 *     a2 = dsrl  (a2, 31)    a2 = a2 >> 31        -> (x & 1) << 32
 *     a1 = dsrl32(a1, 0)     a1 = a1 >> 32        -> (x >> 1) & 0xFFFFFFFF
 *     a0 = dsll32(x, 12)     a0 = x << 44
 *     a2 = a2 | a1
 *     a0 = dsrl32(a0, 0)     a0 = a0 >> 32        -> (x & 0xFFFFF) << 12
 *     a2 = a2 ^ a0
 *     a0 = dsrl(a2, 20)
 *     a0 = a0 & 0xFFF
 *     a0 = a0 ^ a2                                 -> new state
 *     v0 = dsra32(dsll32(a0, 0), 0)                -> sign-extended low 32
 *
 * Note `dsll32 rd, rt, sa` shifts by sa + 32; `dsrl32` and `dsra32` likewise.
 * Misreading those as plain dsll/dsrl is the obvious way to get this wrong,
 * which is why the sequence below is written out step by step rather than
 * algebraically simplified. tests/test_random.cpp checks the step-by-step form
 * against a simplified one over a million iterations, so the simplification is
 * verified rather than assumed.
 *
 * The return value is the SIGN-EXTENDED low 32 bits: callers do
 * `randomGetNext() % 3U`, and whether the value is negative changes the result.
 */

#include "random.h"

/*
 * src/random.s: .word 0xAB8D9F77, 0x81280783 — one big-endian doubleword.
 */
uint64_t g_randomSeed = 0xAB8D9F7781280783ull;

int32_t geRandomStep(uint64_t *state)
{
    uint64_t x = *state;
    uint64_t a2, a1, a0;

    a2 = x << 63;          /* dsll32 a2, x, 31 */
    a1 = x << 31;          /* dsll   a1, x, 31 */
    a2 = a2 >> 31;         /* dsrl   a2, a2, 31 */
    a1 = a1 >> 32;         /* dsrl32 a1, a1, 0  */
    a0 = x << 44;          /* dsll32 a0, x, 12  */
    a2 = a2 | a1;
    a0 = a0 >> 32;         /* dsrl32 a0, a0, 0  */
    a2 = a2 ^ a0;
    a0 = a2 >> 20;         /* dsrl   a0, a2, 20 */
    a0 = a0 & 0xFFF;
    a0 = a0 ^ a2;

    *state = a0;

    /* dsll32 v0, a0, 0 then dsra32 v0, v0, 0: sign-extend the low 32 bits. */
    return (int32_t)(uint32_t)a0;
}

/*
 * src/random.h declares these u32-returning. The assembly leaves a
 * sign-extended value in v0 (dsra32), but an o32 caller reading it as u32 takes
 * the low 32 bits either way, so u32 here is exactly equivalent -- and matching
 * the header is what keeps the declaration from conflicting.
 */
uint32_t randomGetNext(void)
{
    return (uint32_t)geRandomStep(&g_randomSeed);
}

uint32_t randomGetNextFrom(uint64_t *state)
{
    return (uint32_t)geRandomStep(state);
}

/*
 * randomSetSeed stores seed + 1, NOT seed:
 *
 *     daddiu $a0, $a0, 1
 *     sd     $a0, g_randomSeed
 *
 * Two details that are easy to lose and impossible to notice later.
 *
 * The +1 is deliberate -- seeding with 0 must land on 1, and "fixing" the
 * off-by-one would desynchronise from the original.
 *
 * The SIGN EXTENSION is the subtle one. The parameter is declared u32, but the
 * MIPS o32 ABI requires 32-bit arguments to arrive sign-extended in the 64-bit
 * register, and `daddiu` then operates on all 64 bits. So a seed of 0x80000000
 * becomes 0xFFFFFFFF80000001, not 0x80000001 -- a completely different starting
 * state and therefore a completely different game. Writing the obvious
 * `state = seed + 1` on a host would silently diverge for exactly half of all
 * possible seeds.
 */
static uint64_t geSeedFromU32(uint32_t seed)
{
    return (uint64_t)((int64_t)(int32_t)seed + 1);
}

void randomSetSeed(uint32_t seed)
{
    g_randomSeed = geSeedFromU32(seed);
}

/*
 * chrObjRandom* is the same generator over a separate state, so that object and
 * character randomness do not consume each other's sequence.
 */
uint64_t g_chrObjRandomSeed = 0xAB8D9F7781280783ull;

uint32_t chrObjRandomGetNext(void)
{
    return (uint32_t)geRandomStep(&g_chrObjRandomSeed);
}

void chrObjRandomSetSeed(uint32_t seed)
{
    g_chrObjRandomSeed = geSeedFromU32(seed);
}

/*
 * tlbRandomGetNext is the SAME generator again, over a third state
 * (src/tlb_random.s, identical instruction sequence and identical initial
 * seed). It picks which TLB entry to evict.
 *
 * The port maps every address directly and never evicts anything, so this
 * result is not used for its original purpose -- but tlb_manage.c still calls
 * it, and it still advances a sequence. Implemented rather than stubbed to zero
 * because a stub would make g_tlbSegmentIndex constant, and any code that
 * happens to depend on that index varying would then behave differently for a
 * reason that would be very hard to find.
 */
uint64_t g_tlbRandomSeed = 0xAB8D9F7781280783ull;

uint32_t tlbRandomGetNext(void)
{
    return (uint32_t)geRandomStep(&g_tlbRandomSeed);
}
