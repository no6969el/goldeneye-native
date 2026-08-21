/*
 * test_rdram_map.cpp — is RDRAM where the game thinks it is?
 *
 * The port maps RDRAM at KSEG0 (0x80000000) so that an N64 address IS a valid
 * host address. That is not a convenience: src/init.c takes the result of
 * get_csegmentSegmentStart() -- 0x80020D90 -- puts it in a `u8 *` and
 * dereferences it, on the first function the game runs. There are hundreds of
 * sites like it and no realistic way to find them all, so the mapping has to be
 * right rather than the call sites.
 *
 * This test reproduces that exact pattern, because the failure mode without it
 * is a wild pointer rather than a compile error.
 */

#include <cstdint>
#include <cstdio>

#include "ultra/rdram.h"

static int failures = 0;

static void check(bool ok, const char *what)
{
    std::printf("  %-52s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) ++failures;
}

int main()
{
    std::printf("test_rdram_map\n");

    if (!ge_ultra::rdramInit()) {
        std::printf("  rdramInit() failed -- the fixed mapping is unavailable\n");
        return 1;
    }

    uint8_t *base = ge_ultra::rdramBase();
    check(reinterpret_cast<uintptr_t>(base) == 0x80000000u,
          "RDRAM base is exactly 0x80000000");

    /* Exactly what src/init.c does. */
    volatile uint8_t *k0 = reinterpret_cast<uint8_t *>(uintptr_t(0x80020D90u));
    *k0 = 0xAB;
    check(base[0x20D90] == 0xAB,
          "a raw KSEG0 pointer writes into RDRAM");

    /*
     * KSEG1 is the same memory uncached. It must ALIAS -- two separate blocks
     * would give the CPU and the interpreter different views of the same
     * display list, which presents as random corruption rather than as an
     * obvious failure.
     */
    volatile uint8_t *k1 = reinterpret_cast<uint8_t *>(uintptr_t(0xA0020D90u));
    check(*k1 == 0xAB, "KSEG1 sees a KSEG0 write (it aliases, not copies)");
    *k1 = 0x5C;
    check(*k0 == 0x5C, "KSEG0 sees a KSEG1 write");

    check(ge_ultra::virtualToPhysical(reinterpret_cast<void *>(uintptr_t(0x80020D90u)))
              == 0x20D90u,
          "virtualToPhysical strips the segment bits");

    if (failures == 0) std::printf("  all checks passed\n");
    return failures != 0;
}
