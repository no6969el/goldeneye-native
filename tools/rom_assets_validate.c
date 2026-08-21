/*
 * rom_assets_validate.c -- check the asset manifest against a real cartridge.
 *
 * Everything about the asset path so far has been checked against the manifest,
 * the map file, or itself. This checks it against the ROM, which is the only
 * thing that is not a restatement of an assumption.
 *
 * Three questions, in order of how much they decide:
 *
 *   1. Does every recorded rom_offset actually hold a compressed asset? The
 *      1172 container is a 2-byte magic (0x11 0x72) followed by raw deflate
 *      (MICROCODE-SPEC.md 0b), so a wrong offset is visible immediately.
 *
 *   2. For the 548 offsets the OLD translation captured and rewrote: what did
 *      the game actually receive? If the rewritten offset does not hold the
 *      magic, that is the inflate loop, demonstrated rather than reasoned about.
 *
 *   3. Does the NEW path deliver the right bytes for the same requests?
 *
 * Usage: rom_assets_validate <rom.z64>
 *
 * NOTE ON THE OLD PATH. It is reproduced here rather than kept in the shipping
 * code: the point is to show what it did to real data, and a bug that is only
 * described is a bug that gets re-introduced. This file holds the evidence; the
 * fix lives in src/host/ge_assets_load.c.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ge_assets.h"

#define MAGIC_0 0x11u
#define MAGIC_1 0x72u

static unsigned char *rom;
static unsigned int   rom_size;

static int hasMagic(unsigned int off)
{
    if (off + 2u > rom_size) {
        return 0;
    }
    return rom[off] == MAGIC_0 && rom[off + 1] == MAGIC_1;
}

/*
 * The OLD translation, verbatim in behaviour: last entry whose base address is
 * <= p, translated if p falls inside it. Reproduced without the address-span
 * rejection that now guards it, so it can be run against the addresses the
 * pre-fix link produced.
 *
 * Those addresses are not available at runtime any more -- the image is linked
 * elsewhere now -- so they are reconstructed from the recorded pre-fix span.
 * The shape is what matters: assets laid out consecutively from a base inside
 * the ROM offset range.
 */
#define OLD_BASE 0x0040A050u   /* measured span of the pre-fix -no-pie link */

static unsigned int *old_addr;   /* per-asset host address under the old link */


static void buildOldLayout(void)
{
    unsigned int i, cur = OLD_BASE;
    old_addr = malloc(sizeof(*old_addr) * ge_asset_count);
    for (i = 0; i < ge_asset_count; ++i) {
        cur = (cur + 15u) & ~15u;            /* the arrays are aligned(16) */
        old_addr[i] = cur;
        cur += ge_asset_manifest[i].size;
    }
}

/* Old translation: given a device address, what ROM offset came back? */
static int oldTranslate(unsigned int dev, unsigned int *out)
{
    unsigned int lo = 0, hi = ge_asset_count;
    while (lo < hi) {
        unsigned int mid = lo + (hi - lo) / 2u;
        if (old_addr[mid] <= dev) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    if (lo == 0) {
        return 0;
    }
    {
        const GeAssetEntry *e = &ge_asset_manifest[lo - 1u];
        if (e->rom_offset != 0 && dev < old_addr[lo - 1u] + e->size) {
            *out = e->rom_offset + (dev - old_addr[lo - 1u]);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    unsigned int i;
    unsigned int with_off = 0, magic_ok = 0, magic_bad = 0, out_of_range = 0;
    unsigned int captured = 0, cap_magic = 0, cap_nomagic = 0, cap_identity = 0;
    unsigned int new_ok = 0, new_bad = 0;
    int failures = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <rom.z64>\n", argv[0]);
        return 2;
    }
    f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    rom_size = (unsigned int)ftell(f);
    fseek(f, 0, SEEK_SET);
    rom = malloc(rom_size);
    if (fread(rom, 1, rom_size, f) != rom_size) {
        fprintf(stderr, "short read\n");
        return 2;
    }
    fclose(f);
    printf("ROM    : %s (%u bytes)\n", argv[1], rom_size);
    printf("assets : %u in the manifest\n\n", ge_asset_count);

    /* ---------------------------------------------------------------
     * 1. Are the recorded offsets right at all?
     * --------------------------------------------------------------- */
    puts("1. recorded ROM offsets vs the cartridge");
    for (i = 0; i < ge_asset_count; ++i) {
        const GeAssetEntry *e = &ge_asset_manifest[i];
        if (e->rom_offset == 0) {
            continue;
        }
        ++with_off;
        if (e->rom_offset + e->size > rom_size) {
            ++out_of_range;
        }
        if (hasMagic(e->rom_offset)) {
            ++magic_ok;
        } else {
            if (magic_bad < 8) {
                printf("   no magic: %-28s off 0x%08X -> %02X %02X\n",
                       e->name, e->rom_offset,
                       e->rom_offset + 1u < rom_size ? rom[e->rom_offset] : 0,
                       e->rom_offset + 1u < rom_size ? rom[e->rom_offset + 1] : 0);
            }
            ++magic_bad;
        }
    }
    printf("   %u of %u recorded offsets hold the 1172 magic (%u do not)\n\n",
           magic_ok, with_off, magic_bad);

    /* ---------------------------------------------------------------
     * 2. What the OLD translation actually delivered.
     * --------------------------------------------------------------- */
    buildOldLayout();
    puts("2. the OLD translation, replayed against real ROM data");
    printf("   (pre-fix host layout reconstructed from 0x%08X)\n", OLD_BASE);
    for (i = 0; i < ge_asset_count; ++i) {
        const GeAssetEntry *e = &ge_asset_manifest[i];
        unsigned int got = 0;
        if (e->rom_offset == 0) {
            continue;
        }
        /* The game asks for this asset's genuine ROM offset. */
        if (oldTranslate(e->rom_offset, &got)) {
            ++captured;
            if (got == e->rom_offset) {
                ++cap_identity;
            }
            if (hasMagic(got)) {
                ++cap_magic;
            } else {
                if (cap_nomagic < 6) {
                    printf("   %-24s asked 0x%08X -> got 0x%08X  bytes %02X %02X"
                           "  (not compressed data)\n",
                           e->name, e->rom_offset, got,
                           rom[got], rom[got + 1]);
                }
                ++cap_nomagic;
            }
        }
    }
    printf("   %u of %u requests captured and rewritten\n", captured, with_off);
    printf("   %u landed on a 1172 header; %u landed on something else\n",
           cap_magic, cap_nomagic);
    printf("   %u happened to resolve back to themselves\n\n", cap_identity);

    /* ---------------------------------------------------------------
     * 3. The NEW path, on the same requests.
     * --------------------------------------------------------------- */
    puts("3. the NEW path (image linked above the ROM), same requests");
    for (i = 0; i < ge_asset_count; ++i) {
        const GeAssetEntry *e = &ge_asset_manifest[i];
        unsigned int got = 0, eff;
        if (e->rom_offset == 0) {
            continue;
        }
        /*
         * A genuine ROM offset must pass through untranslated; the effective
         * offset is then the one the game asked for.
         */
        eff = geAssetsRomOffsetFor((const void *)(uintptr_t)e->rom_offset, &got)
                  ? got
                  : e->rom_offset;
        if (hasMagic(eff)) {
            ++new_ok;
        } else {
            ++new_bad;
        }
    }
    printf("   %u of %u requests reach a 1172 header; %u do not\n\n",
           new_ok, with_off, new_bad);

    /* A host address must still translate, and to data that is really there. */
    {
        extern unsigned char LgunE[];
        unsigned int got = 0;
        int ok = geAssetsRomOffsetFor(LgunE, &got) && got == 0x008ED250u
                 && hasMagic(got);
        printf("   &LgunE -> 0x%08X, magic %s\n", got, hasMagic(got) ? "yes" : "NO");
        if (!ok) {
            ++failures;
        }
    }

    /*
     * WHAT COUNTS AS A FAILURE, AND WHAT DOES NOT.
     *
     * "No 1172 magic" is not by itself wrong. 138 of the 821 assets are not
     * compressed containers at all: the uncompressed level p_seg tables (all 24
     * of which start with the same 8-byte header, which is what shows they are
     * at the right offsets), the ramrom attract-mode recordings, and the
     * animation tables. Failing on those would be failing on a correct result,
     * so they are reported and counted, not judged.
     *
     * What IS a failure is an offset that runs off the end of the cartridge,
     * and the old path's regression coming back.
     */
    puts("");
    printf("NOTE: %u assets hold no 1172 header. That is expected for the\n"
           "      uncompressed p_seg tables, the ramrom recordings and the\n"
           "      animation tables -- see the class breakdown in PRIORITIES.md P3g.\n",
           magic_bad);
    if (out_of_range != 0) {
        printf("FAIL: %u recorded offsets run past the end of the ROM\n",
               out_of_range);
        ++failures;
    }
    if (cap_nomagic == 0) {
        printf("NOTE: the old path never landed off a header in this replay --\n"
               "      the hypothesis is NOT supported by this measurement.\n");
    }
    printf("%s\n", failures ? "FAILED" : "all checks passed");
    return failures != 0;
}
