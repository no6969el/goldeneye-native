/*
 * test_asset_addr.c -- the asset address space must not overlap the ROM's.
 *
 * WHAT THIS IS FOR
 *
 * The game passes asset symbol ADDRESSES to the PI as cartridge offsets, so the
 * port translates host addresses back to ROM offsets on every DMA
 * (src/host/ge_assets_load.c). That is only sound while no address the game
 * means as a ROM offset can land inside an asset.
 *
 * At the default load address it does not hold, and nothing says so. The port
 * boots, loads the wrong third of the ROM, and hangs inside the game's own
 * zlib on data that was never compressed -- three subsystems away from the
 * cause. This test is what makes that a build failure instead.
 *
 * It is deliberately a LINK-TIME test: the property being checked is a
 * property of the link, so it has to be built the way the game is built.
 * tools/link_game.sh and the CMake target both pass the image base; this test
 * asserts the result rather than the flag, because a flag that is accepted and
 * has no effect is exactly the failure worth catching.
 *
 * Build (mirrors the game link):
 *   cc -no-pie -Wl,-Ttext-segment=0x20000000 -I src/host \
 *      tests/test_asset_addr.c src/host/ge_assets.c src/host/ge_assets_load.c \
 *      -o build/test_asset_addr && ./build/test_asset_addr
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ge_assets.h"

/* The real ROM's size. Every offset the game can produce lies below this. */
#define GE_ROM_SIZE 0x00C00000u

/* LgunE is the asset the P3e debugging session traced by hand. Its recorded
 * position is checked here so the manifest and the translator are pinned to a
 * value that was verified against the ROM, not against each other. */
extern unsigned char LgunE[];
#define LGUNE_ROM_OFFSET 0x008ED250u

static int failures;

static void check(int ok, const char *what)
{
    printf("%-58s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

int main(void)
{
    GeAssetSpan span;
    unsigned int i, off, captured = 0, identity = 0, shadowed = 0;

    memset(&span, 0, sizeof(span));

    /*
     * 1. The property itself. Everything below is a consequence of it; this is
     *    the one that fails first if the image base is lost.
     */
    {
        const int disjoint = geAssetsCheckAddressSpace(GE_ROM_SIZE, &span);
        printf("assets : %u symbols at %p .. %p\n",
               ge_asset_count, (const void *)span.lo, (const void *)span.hi);
        if (!disjoint) {
            printf("  %u of them overlap the ROM offset space. The image was\n"
                   "  not linked above the ROM -- see tools/link_game.sh.\n",
                   span.conflicts);
        }
        check(disjoint, "asset space is disjoint from the ROM offset space");
    }

    /*
     * 2. A host address inside an asset still translates, and to the right
     *    place. Separation is worthless if it breaks the thing it protects.
     */
    off = 0;
    check(geAssetsRomOffsetFor(LgunE, &off) && off == LGUNE_ROM_OFFSET,
          "&LgunE translates to its recorded ROM offset");

    off = 0;
    check(geAssetsRomOffsetFor(LgunE + 64, &off)
              && off == LGUNE_ROM_OFFSET + 64u,
          "an address inside LgunE translates to the same relative offset");

    /*
     * 3. The regression. Feed every asset's OWN recorded ROM offset in as a
     *    device address: none may be translated. Before the fix, 548 of 821
     *    were, and not one of them mapped back to itself -- so the port read a
     *    different part of the ROM every time and never noticed.
     */
    for (i = 0; i < ge_asset_count; ++i) {
        const GeAssetEntry *e = &ge_asset_manifest[i];
        unsigned int t = 0;
        if (e->rom_offset == 0) {
            continue;
        }
        if (geAssetsRomOffsetFor((const void *)(uintptr_t)e->rom_offset, &t)) {
            ++captured;
            if (t == e->rom_offset) {
                ++identity;
            }
        }
    }
    printf("  %u of %u recorded ROM offsets were captured (%u harmlessly)\n",
           captured, ge_asset_count, identity);
    check(captured == 0, "no genuine ROM offset is captured by the translator");

    /*
     * 4. The whole ROM, not just the offsets that happen to be in the manifest.
     *    The game computes offsets the manifest never lists -- sub-ranges of a
     *    file, and the file table's own entries -- so sampling the entire
     *    address space is the check that actually covers them.
     */
    for (i = 0; i < GE_ROM_SIZE; i += 1024u) {
        unsigned int t = 0;
        if (geAssetsRomOffsetFor((const void *)(uintptr_t)i, &t)) {
            ++shadowed;
        }
    }
    printf("  %u of %u sampled ROM offsets fall inside an asset\n",
           shadowed, GE_ROM_SIZE / 1024u);
    check(shadowed == 0, "no part of the ROM address space is shadowed");

    /*
     * 5. PI bus addresses are ROM offsets too, based at 0x10000000, and
     *    osPiStartDma accepts them. They must pass through untouched as well.
     */
    shadowed = 0;
    for (i = 0; i < GE_ROM_SIZE; i += 1024u) {
        unsigned int t = 0;
        if (geAssetsRomOffsetFor((const void *)(uintptr_t)(0x10000000u + i),
                                 &t)) {
            ++shadowed;
        }
    }
    check(shadowed == 0, "no 0x10000000-based PI bus address is shadowed");

    /*
     * 6. The image has to stay addressable through a u32. libultra's
     *    osPiStartDma takes `u32 devAddr`, so an asset above 4 GB is truncated
     *    before the translator is ever called -- and the truncated value lands
     *    back in the low range this whole test exists to keep clear.
     */
    check((uintptr_t)span.hi <= 0xFFFFFFFFul,
          "every asset address survives truncation to u32");

    printf("\n%s\n", failures ? "FAILED" : "all checks passed");
    return failures != 0;
}
