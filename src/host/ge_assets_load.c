/*
 * ge_assets_load.c — copy the game's assets out of the user's ROM.
 *
 * src/host/ge_assets.c defines 821 symbols at their real sizes so the game
 * links. This puts the data in them.
 *
 * WHY A COPY RATHER THAN A POINTER
 *
 * The game takes the ADDRESS of these symbols and hands it to romCopy, to the
 * decompressor, and (for display lists) into the RSP task stream. It also
 * writes into some of them. Pointing them at a read-only mapping of the ROM
 * would work right up until the first write, and would then fault a long way
 * from here.
 *
 * WHERE THE OFFSETS COME FROM
 *
 * The decomp's linked map file, not from `nm` on the object files. `nm` on an
 * unlinked object reports section-relative addresses -- CarmourguardZ reads as
 * 0x002B4530 there and actually lives at 0x006ECB90. Loading from the object
 * address would have read a completely different part of the ROM, which looks
 * exactly like corrupt assets rather than like a wrong offset.
 *
 * Verified rather than assumed: every obseg asset at its map offset begins with
 * the 1172 magic that marks a compressed asset.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ge_assets.h"

unsigned int geAssetsLoad(const unsigned char *rom, unsigned int rom_size,
                          unsigned int *skipped)
{
    unsigned int loaded = 0, missed = 0, i;

    if (rom == 0) {
        if (skipped != 0) {
            *skipped = ge_asset_count;
        }
        return 0;
    }

    for (i = 0; i < ge_asset_count; ++i) {
        const GeAssetEntry *e = &ge_asset_manifest[i];

        /*
         * Skipped rather than clamped. A partial asset is worse than an empty
         * one: it decompresses into plausible-looking garbage instead of
         * failing, and the resulting corruption shows up somewhere unrelated.
         */
        if (e->rom_offset == 0 || e->size == 0) {
            ++missed;
            continue;
        }
        if (e->rom_offset > rom_size || e->size > rom_size - e->rom_offset) {
            ++missed;
            continue;
        }
        memcpy(e->data, rom + e->rom_offset, e->size);
        ++loaded;
    }

    if (skipped != 0) {
        *skipped = missed;
    }
    return loaded;
}

/*
 * ---------------------------------------------------------------------------
 * Asset address -> ROM offset.
 *
 * On N64, an asset symbol's ADDRESS *is* its position in the cartridge. The
 * game relies on that directly:
 *
 *     doRomCopy(target, source = &LgunE, size);   // src/ramrom.c
 *
 * where &LgunE is fed to the PI as a device address. In this port LgunE is a
 * generated array in the executable's BSS, so its address is a host address
 * that happens to be a small number -- and the DMA read a plausible-looking but
 * completely wrong part of the ROM. The symptom was the game's own zlib
 * faulting while inflating what it thought was a language file.
 *
 * So the DMA path asks here first. An exact match on an asset's base address is
 * unambiguous; an address INSIDE an asset is translated too, because the game
 * also reads sub-ranges of a file.
 *
 * THE AMBIGUITY WAS NOT HYPOTHETICAL. IT WAS THE BUG.
 *
 * The previous note here said a genuine ROM offset "could" fall inside some
 * asset's host address range, and argued it away. Measured instead
 * (tests/test_asset_addr.c), with the -no-pie link the port actually uses:
 *
 *     host BSS span            0x0040A050 .. 0x00839F60   (4.19 MB)
 *     ROM offset space         0x00000000 .. 0x00C00000   (12 MB)
 *
 * They overlap almost entirely. 548 of the 821 assets' OWN recorded ROM
 * offsets, fed in as device addresses, were captured by the translator and
 * rewritten to a different offset -- and NOT ONE of them mapped back to
 * itself. 34.9% of the ROM address space was shadowed.
 *
 * That is the "bytes reaching the decompressor do not start with the 1172
 * magic" symptom in P3e, and it is why zlib_inflate_codes span. It is not a
 * translation that resolves to the wrong asset; &LgunE resolves to 0x008ED250
 * exactly as recorded. It is a genuine ROM offset being translated when it
 * should have passed through untouched.
 *
 * THE FIX IS SEPARATION, NOT ARBITRATION.
 *
 * There is no rule that can tell 0x008ED250-the-ROM-offset from
 * 0x008ED250-the-host-address once they are the same number, because they ARE
 * the same number. So the port links its image above the ROM address space
 * (tools/link_game.sh: -Wl,-Ttext-segment=0x20000000; on Windows /BASE) and
 * geAssetsCheckAddressSpace() REFUSES TO RUN if that did not take effect.
 * After it, the two spaces are disjoint and the question cannot arise:
 *
 *     host BSS span            0x2000A050 .. 0x20439F60
 *     collisions               0 of 821
 *
 * The image must still fit in 32 bits: libultra's osPiStartDma takes a
 * `u32 devAddr`, so a host pointer above 4 GB is truncated before it ever
 * reaches the translator. 0x20000000 is chosen to clear the ROM (12 MB), the
 * PI bus window at 0x10000000, RDRAM at 0x80000000 and the TLB windows at
 * 0x70000000 / 0x7F000000, with room for an image far larger than this one.
 * ---------------------------------------------------------------------------
 */
/*
 * Sorted by host address so the lookup is a binary search rather than a scan of
 * 821 entries. That is not premature: this runs on EVERY PI DMA, and the game
 * DMAs constantly while loading a level. The linear version made a 40-frame run
 * take longer than the two-minute timeout.
 */
static const GeAssetEntry **ge_by_addr;
static unsigned int ge_by_addr_count;

/*
 * The span the sorted index covers, kept so the translator can reject an
 * address outside it without a search. This is a fast path, NOT the safety
 * property -- the safety property is geAssetsCheckAddressSpace() below.
 */
static const unsigned char *ge_span_lo;
static const unsigned char *ge_span_hi;

static int cmpEntry(const void *a, const void *b)
{
    const unsigned char *pa = (*(const GeAssetEntry * const *)a)->data;
    const unsigned char *pb = (*(const GeAssetEntry * const *)b)->data;
    return (pa < pb) ? -1 : (pa > pb) ? 1 : 0;
}

static void ensureIndex(void)
{
    unsigned int i;
    if (ge_by_addr != 0) {
        return;
    }
    ge_by_addr = (const GeAssetEntry **)
        malloc(sizeof(*ge_by_addr) * (ge_asset_count ? ge_asset_count : 1));
    if (ge_by_addr == 0) {
        return;
    }
    for (i = 0; i < ge_asset_count; ++i) {
        ge_by_addr[i] = &ge_asset_manifest[i];
    }
    qsort(ge_by_addr, ge_asset_count, sizeof(*ge_by_addr), cmpEntry);
    ge_by_addr_count = ge_asset_count;

    if (ge_by_addr_count != 0) {
        ge_span_lo = ge_by_addr[0]->data;
        ge_span_hi = ge_by_addr[0]->data + ge_by_addr[0]->size;
        for (i = 0; i < ge_by_addr_count; ++i) {
            const unsigned char *end =
                ge_by_addr[i]->data + ge_by_addr[i]->size;
            if (end > ge_span_hi) {
                ge_span_hi = end;
            }
        }
    }
}

/*
 * Is the asset address space disjoint from every address the game could mean
 * as a ROM offset? Called once at startup, before the translator is installed.
 *
 * Checked rather than assumed because the property is established by a LINKER
 * FLAG, and a linker flag is exactly the kind of thing that gets dropped when
 * a build script is edited or a second toolchain is added. If it is dropped,
 * the port does not fail -- it silently loads the wrong 35% of the ROM, which
 * is how this cost a session the first time.
 *
 * Two windows count as ROM offsets: bare offsets, which the game passes
 * directly, and PI bus addresses based at 0x10000000, which osPiStartDma also
 * accepts. Returns 1 when safe. *out is filled either way, so the caller can
 * report the actual numbers rather than just "no".
 */
int geAssetsCheckAddressSpace(unsigned int rom_size, GeAssetSpan *out)
{
    unsigned int i, conflicts = 0;

    ensureIndex();

    if (out != 0) {
        out->lo = ge_span_lo;
        out->hi = ge_span_hi;
        out->conflicts = 0;
    }
    if (ge_by_addr == 0 || ge_by_addr_count == 0) {
        return 1;
    }

    for (i = 0; i < ge_by_addr_count; ++i) {
        const GeAssetEntry *e = ge_by_addr[i];
        unsigned long lo = (unsigned long)(uintptr_t)e->data;
        unsigned long hi = lo + e->size;

        if (lo < (unsigned long)rom_size && hi > 0ul) {
            ++conflicts;
            continue;
        }
        if (lo < 0x10000000ul + rom_size && hi > 0x10000000ul) {
            ++conflicts;
        }
    }

    if (out != 0) {
        out->conflicts = conflicts;
    }
    return conflicts == 0;
}

int geAssetsRomOffsetFor(const void *host_addr, unsigned int *rom_offset)
{
    const unsigned char *p = (const unsigned char *)host_addr;
    unsigned int lo, hi;

    if (p == 0 || rom_offset == 0) {
        return 0;
    }
    ensureIndex();
    if (ge_by_addr == 0) {
        return 0;
    }
    /*
     * Outside the asset span entirely -- a bare ROM offset, a PI bus address,
     * an RDRAM pointer. Not an asset, so not ours to rewrite. This runs on
     * every PI DMA, and it is also what makes the disjointness guarantee
     * cheap to rely on rather than merely true.
     */
    if (p < ge_span_lo || p >= ge_span_hi) {
        return 0;
    }

    /* Last entry whose base is <= p. */
    lo = 0;
    hi = ge_by_addr_count;
    while (lo < hi) {
        const unsigned int mid = lo + (hi - lo) / 2u;
        if (ge_by_addr[mid]->data <= p) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    if (lo == 0) {
        return 0;
    }
    {
        const GeAssetEntry *e = ge_by_addr[lo - 1u];
        if (e->rom_offset != 0 && p < e->data + e->size) {
            *rom_offset = e->rom_offset + (unsigned int)(p - e->data);
            return 1;
        }
    }
    return 0;
}
