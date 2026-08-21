/*
 * test_random.cpp — is the RNG transcription actually right?
 *
 * src/ultra/random.c is a hand transcription of MIPS assembly, which is exactly
 * the kind of code that looks correct and is not. Three checks:
 *
 *   1. The step-by-step form agrees with an independently-derived algebraic
 *      form over a million iterations. Both were written from the same
 *      instruction listing, but by different routes: if I misread `dsll32` as
 *      `dsll` (a shift of 63 vs 31 — the single most likely mistake here), the
 *      two derivations disagree.
 *   2. The generator does not collapse. A xorshift with a transcription error
 *      very often degenerates to a short cycle or to zero, which a
 *      "does it produce numbers" test would happily pass.
 *   3. Seeding sign-extends. This is the detail a host port silently gets wrong
 *      for half of all seeds.
 */

#include <cstdint>
#include <cstdio>
#include <set>

extern "C" {
#include "ultra/random.h"
}

static int failures = 0;

static void check(bool ok, const char *what)
{
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++failures;
    }
}

/*
 * Independent derivation. Each MIPS shift is resolved to what it selects,
 * rather than replayed as a shift:
 *
 *   (x << 63) >> 31   selects bit 0 of x, placed at bit 32
 *   (x << 31) >> 32   selects bits 1..32 of x, placed at bits 0..31
 *   (x << 44) >> 32   selects bits 0..19 of x, placed at bits 12..31
 */
static int32_t stepAlgebraic(uint64_t *state)
{
    const uint64_t x = *state;

    const uint64_t hi  = (x & 1ull) << 32;
    const uint64_t lo  = (x >> 1) & 0xFFFFFFFFull;
    const uint64_t mix = (x & 0xFFFFFull) << 12;

    uint64_t t = (hi | lo) ^ mix;
    t = t ^ ((t >> 20) & 0xFFFull);

    *state = t;
    return (int32_t)(uint32_t)t;
}

int main()
{
    std::printf("test_random\n");

    /* 1. The two derivations must agree, step for step, from the real seed. */
    {
        uint64_t a = 0xAB8D9F7781280783ull;
        uint64_t b = a;
        bool agree = true;
        for (int i = 0; i < 1000000 && agree; ++i) {
            const int32_t va = geRandomStep(&a);
            const int32_t vb = stepAlgebraic(&b);
            if (va != vb || a != b) {
                std::printf("  diverged at iteration %d: %08x/%016llx vs "
                            "%08x/%016llx\n", i,
                            (unsigned)va, (unsigned long long)a,
                            (unsigned)vb, (unsigned long long)b);
                agree = false;
            }
        }
        check(agree, "step-by-step and algebraic forms agree over 1e6 steps");
    }

    /* 2. It must not collapse to a short cycle or to zero. */
    {
        uint64_t s = 0xAB8D9F7781280783ull;
        std::set<uint64_t> seen;
        bool hitZero = false;
        for (int i = 0; i < 200000; ++i) {
            geRandomStep(&s);
            if (s == 0) { hitZero = true; break; }
            seen.insert(s);
        }
        check(!hitZero, "state never reaches zero (an absorbing state)");
        check(seen.size() == 200000, "200k steps give 200k distinct states");
    }

    /* 3. Seeding must sign-extend, and must add one. */
    {
        randomSetSeed(0);
        check(g_randomSeed == 1, "seed 0 -> state 1 (the +1 is real)");

        randomSetSeed(0x80000000u);
        check(g_randomSeed == 0xFFFFFFFF80000001ull,
              "seed 0x80000000 sign-extends to 0xFFFFFFFF80000001");

        randomSetSeed(0x7FFFFFFFu);
        check(g_randomSeed == 0x0000000080000000ull,
              "seed 0x7FFFFFFF -> 0x80000000 (no sign extension below the top bit)");
    }

    /* 4. The two generators are independent. */
    {
        randomSetSeed(1234);
        chrObjRandomSetSeed(1234);
        const uint32_t a = randomGetNext();
        randomGetNext();                 /* advance one only */
        const uint32_t b = chrObjRandomGetNext();
        check(a == b, "same seed, same first value -- one algorithm");
        check(g_randomSeed != g_chrObjRandomSeed,
              "separate state: one does not consume the other's sequence");
    }

    if (failures == 0) {
        std::printf("  all checks passed\n");
    }
    return failures != 0;
}
