#!/usr/bin/env python3
"""
gen_asset_symbols.py -- define the game's asset symbols for a host link.

THE PROBLEM

822 of the symbols the game references are asset DATA: character models
(C...Z), props (P...), animations (ANIM_DATA_*), global display lists. On N64
they come from the decomp's asset pipeline, which assembles them into MIPS
object files with `.incbin` of data extracted from the cartridge. Those objects
are MIPS ELF; a host link cannot use them.

WHAT THIS EMITS

One C file defining each symbol as a correctly-sized, correctly-aligned array in
BSS, plus a manifest (name, address, size) so the data can be loaded into them
at startup.

WHY BSS AND A LOADER, RATHER THAN INITIALISED ARRAYS

Emitting the bytes as C initialisers would produce tens of megabytes of source,
compile slowly, and -- the part that actually matters -- would bake the user's
cartridge data into a source file this project distributes. Everything else here
follows the rule that no game data is shipped, and this is not the place to
break it.

A NOTE ON EDITING THE OUTPUT

The header this emits is generated, and it had been hand-edited afterwards --
`geAssetsRomOffsetFor` was declared in src/host/ge_assets.h but not here, so the
next regeneration would have deleted it and the link would fail somewhere that
looks nothing like the cause. Every declaration the port needs is emitted from
this template. Do not add one to the output file.

WHAT THIS DOES NOT DO

It does not make the game work. Uninitialised assets mean models and animations
are all zeros. It makes the game LINK, and gives the loader somewhere to put the
data. Those are different milestones and the manifest exists so the second one
is a data problem rather than a linking problem.

Sizes come from the decomp's own built asset objects (`nm -S`), so they are the
real sizes rather than guesses.

Usage:
    gen_asset_symbols.py <decomp-root> <undefined-syms.txt> <out.c> <out.h>
"""
import os, re, subprocess, sys

def find_nm():
    for c in ("mips-linux-gnu-nm", "mips64-linux-gnuabi64-nm", "mips-elf-nm"):
        try:
            subprocess.run([c, "--version"], capture_output=True, check=True)
            return c
        except Exception:
            pass
    return None

def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    root, wanted_path, out_c, out_h = sys.argv[1:5]

    # ROM OFFSETS COME FROM THE MAP, NOT FROM THE OBJECTS.
    #
    # `nm` on an unlinked object reports section-relative addresses:
    # CarmourguardZ reads as 0x002B4530 there. Its real position in the ROM is
    # 0x006ECB90. Loading from the object address would have read a completely
    # different part of the file and looked like corrupt assets rather than a
    # wrong offset.
    #
    # Verified rather than assumed: the ROM at each obseg offset begins with the
    # 1172 magic that marks a compressed asset.
    mapfile = os.path.join(root, "build", "u", "ge007.u.map")
    rom_off = {}
    if os.path.exists(mapfile):
        pat = re.compile(r"^\s+0x([0-9a-fA-F]+)\s+([A-Za-z_][A-Za-z0-9_]*)\s*$")
        for line in open(mapfile):
            m = pat.match(line)
            if m:
                rom_off.setdefault(m.group(2), int(m.group(1), 16))
    else:
        print(f"warning: {mapfile} not found -- assets will have no ROM offsets "
              f"and cannot be loaded", file=sys.stderr)

    nm = find_nm()
    if nm is None:
        sys.exit("no MIPS nm found; cannot read asset symbol sizes")

    objs = []
    for dirpath, _dirs, files in os.walk(os.path.join(root, "build")):
        if "asset" not in dirpath:
            continue
        objs += [os.path.join(dirpath, f) for f in files if f.endswith(".o")]
    if not objs:
        sys.exit("no asset objects under build/ -- build the decomp first")

    # Sizes arrive two different ways, and using only the first silently loses
    # two thirds of them.
    #
    #   1. `nm -S` reports an ELF size for symbols the assembler gave a .size
    #      directive -- the animation and image tables.
    #   2. assets/obseg/ob_seg.s emits `SYM:` ... `end_SYM:` label pairs with no
    #      .size at all, which is every character model, prop and level. For
    #      those the size is end_SYM - SYM.
    #
    # Reading only (1) produced 267 of 822 symbols and looked like a plausible
    # result. Anything still without a size is reported, never emitted as a
    # zero-length array: that would link and then be overrun silently.
    sizes = {}
    addrs = {}
    for i in range(0, len(objs), 200):                 # keep argv under the limit
        out = subprocess.run([nm, "-S"] + objs[i:i + 200],
                             capture_output=True, text=True).stdout
        for line in out.splitlines():
            f = line.split()
            if len(f) == 4 and re.fullmatch(r"[0-9a-fA-F]+", f[0]):
                sizes[f[3]] = int(f[1], 16)
            elif len(f) == 3 and re.fullmatch(r"[0-9a-fA-F]+", f[0]):
                addrs[f[2]] = int(f[0], 16)

    empty = set()
    for name, start in addrs.items():
        if name in sizes or name.startswith("end_"):
            continue
        end = addrs.get("end_" + name)
        if end is None:
            continue
        if end > start:
            sizes[name] = end - start
        else:
            # end_X == X. Not a failure: these are genuinely empty segment
            # markers in the ROM build (unused levels), and the game only ever
            # takes their address. Emitted as a one-byte placeholder so the
            # address exists and is distinct.
            sizes[name] = 1
            empty.add(name)

    # Third source of sizes: symbols whose .s file uses a `_end` suffix rather
    # than an `end_` prefix (assets/ramrom/ramrom.s), or that are a plain
    # `.incbin` of a file on disk. Scanning the assembly directly catches both.
    # Without this, 17 symbols -- the attract-mode demo recordings -- have no
    # size and the link fails on exactly those.
    for name, start in list(addrs.items()):
        if name in sizes:
            continue
        end = addrs.get(name + "_end")
        if end is not None and end > start:
            sizes[name] = end - start

    for dirpath, _d, files in os.walk(os.path.join(root, "assets")):
        for fn in files:
            if not fn.endswith(".s"):
                continue
            try:
                text = open(os.path.join(dirpath, fn), errors="ignore").read()
            except OSError:
                continue
            for m in re.finditer(r'^\s*([A-Za-z_][A-Za-z0-9_]*):\s*\n'
                                 r'\s*\.incbin\s+"([^"]+)"', text, re.M):
                nm_, path = m.group(1), m.group(2)
                if nm_ in sizes:
                    continue
                full = os.path.join(root, path)
                if os.path.exists(full):
                    sizes[nm_] = os.path.getsize(full)

    wanted = [l.strip() for l in open(wanted_path) if l.strip()]

    # A wanted symbol of the form X_end, where X is itself a wanted array, is an
    # END LABEL: the game uses the DIFFERENCE of the two addresses as a size
    #
    #     romCopy(dst, &unknown2, (u32)&unknown2_end - (u32)&unknown2);
    #
    # C cannot place one object's symbol at another object's end, so these
    # become macros naming the byte one past the array -- the same technique the
    # segment table uses. The decomp's `extern` for them is guarded, because a
    # macro and a declaration of the same name cannot coexist.
    end_aliases = {}
    for s_ in list(wanted):
        if s_.endswith("_end") and s_[:-4] in sizes:
            base = s_[:-4]
            end_aliases[s_] = (base, sizes[base])

    have    = [s for s in wanted
               if s in sizes and sizes[s] > 0 and s not in end_aliases]
    missing = [s for s in wanted
               if s not in end_aliases and (s not in sizes or sizes[s] == 0)]

    with open(out_c, "w") as c:
        c.write(f"""/*
 * ge_assets.c -- GENERATED by tools/gen_asset_symbols.py. Do not edit.
 *
 * {len(have)} asset symbols, defined at their real sizes so the game links.
 * They are ZERO until geAssetsLoad() fills them -- see the generator's
 * docstring for why the data is not baked in here.
 */
#include "ge_assets.h"

""")
        for s in have:
            c.write(f"unsigned char {s}[{sizes[s]}] "
                    "__attribute__((aligned(16)));\n")
        c.write(f"\nconst GeAssetEntry ge_asset_manifest[] = {{\n")
        for s in have:
            c.write(f'    {{ "{s}", {s}, {sizes[s]}u, '
                    f'0x{rom_off.get(s, 0):08X}u }},\n')
        c.write("};\n\nconst unsigned int ge_asset_count = "
                f"{len(have)}u;\n")

    with open(out_h, "w") as h:
        h.write("""/*
 * ge_assets.h -- GENERATED by tools/gen_asset_symbols.py. Do not edit.
 */
#ifndef GE_ASSETS_H
#define GE_ASSETS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GeAssetEntry {
    const char    *name;
    unsigned char *data;       /* where the game expects it */
    unsigned int   size;
    unsigned int   rom_offset; /* where it lives in the user's ROM; 0 = unknown */
} GeAssetEntry;

/*
 * Copy every asset from the mounted ROM into its symbol. Returns the number
 * loaded; entries with rom_offset 0, or that would run past the end of the ROM,
 * are skipped and counted in *skipped.
 */
unsigned int geAssetsLoad(const unsigned char *rom, unsigned int rom_size,
                          unsigned int *skipped);

/*
 * Translate a host address that lies inside a known asset into its ROM offset.
 * Returns 1 on success. The DMA path uses this because the game passes asset
 * symbol ADDRESSES to the PI as device addresses -- on N64 they are the same
 * number. See ge_assets_load.c.
 */
int geAssetsRomOffsetFor(const void *host_addr, unsigned int *rom_offset);

typedef struct GeAssetSpan {
    const unsigned char *lo;
    const unsigned char *hi;
    unsigned int         conflicts;  /* assets overlapping a ROM-offset window */
} GeAssetSpan;

/*
 * Are the asset symbols linked clear of every address the game could mean as a
 * ROM offset? Returns 1 when they are. Call this at startup, BEFORE installing
 * the translator: if it returns 0 the translator will capture genuine ROM
 * offsets and hand back different ones, and the failure surfaces as the game's
 * own decompressor spinning on data that is not compressed.
 */
int geAssetsCheckAddressSpace(unsigned int rom_size, GeAssetSpan *out);

extern const GeAssetEntry ge_asset_manifest[];
extern const unsigned int ge_asset_count;
""")
        if end_aliases:
            h.write("\n/*\n"
                    " * End labels. The game uses `&X_end - &X` as a size:\n"
                    " *\n"
                    " *     romCopy(dst, &unknown2, (u32)&unknown2_end - (u32)&unknown2);\n"
                    " *\n"
                    " * C cannot place one object's symbol at another object's end, so\n"
                    " * these name the byte one past the array -- the same technique the\n"
                    " * segment table uses. The decomp's `extern` for them is guarded,\n"
                    " * because a macro and a declaration of that name cannot coexist.\n"
                    " */\n")
            for name, (base, size) in sorted(end_aliases.items()):
                h.write(f"extern unsigned char {base}[{size}];\n")
                h.write(f"#define {name} (*({base} + {size}u))\n")
        h.write("""
#ifdef __cplusplus
}
#endif

#endif /* GE_ASSETS_H */
""")

    total = sum(sizes[s] for s in have)
    print(f"{len(have)} asset symbols, {total/1024/1024:.1f} MB of BSS",
          file=sys.stderr)
    n_empty = len([s for s in have if s in empty])
    if n_empty:
        print(f"  ({n_empty} are empty markers in the ROM build -- unused "
              f"levels; address only)", file=sys.stderr)
    if missing:
        print(f"WARNING: {len(missing)} symbols had no size in the asset "
              f"objects and were NOT defined:", file=sys.stderr)
        for s in missing[:10]:
            print(f"    {s}", file=sys.stderr)
        if len(missing) > 10:
            print(f"    ... and {len(missing) - 10} more", file=sys.stderr)

if __name__ == "__main__":
    main()
