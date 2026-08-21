#!/usr/bin/env python3
"""
resolve_assets.py -- repair src/host/ge_assets.c against the user's own ROM.

WHY THIS EXISTS

gen_asset_symbols.py derives each asset's ROM offset and size from the decomp's
map file and its built asset objects. Two things it cannot see there are wrong,
and both are wrong SILENTLY -- the port links, boots, loads 764 assets, reports
success, and then hangs a long way away:

  1. OFFSETS. The map records a symbol's address in ITS SEGMENT's address space.
     Five segments have a VRAM base that is not their ROM position, so for the
     symbols in those the map value is not a ROM offset at all:

         animation_data      vram 0x00000000  rom 0x0028E980
         animation_entries   vram 0x00000000  rom 0x00124AC0
         Globalimagetable    vram 0x02000000  rom 0x0029D160
         rarewarelogo        vram 0x02000000  rom 0x0029E560
         fontdl              vram 0x01000000  rom 0x00117880

     35 ANIM_DATA_* symbols were loading the ROM HEADER and boot code as
     animation data -- ANIM_DATA_idle read the ASCII string "GOLDENEY". Another
     57 (every global display list, the font display lists, the Rareware logo)
     had an address past the end of the ROM and were skipped entirely, so the
     port was running with no global display lists at all.

  2. SIZES. 167 of 683 compressed assets have the wrong recorded length, 115 of
     them too small. This is the one that stopped the boot. The game computes a
     file's compressed length as the distance between consecutive entries in
     file_resource_table, and in the port those entries are host addresses -- so
     an array that is too short makes the game DMA too FEW bytes, and its
     decompressor runs off the end of a truncated deflate stream and never
     terminates. LgunE: recorded 896, true 1822.

THE AUTHORITY IS THE CARTRIDGE

For a 1172 asset the true compressed length is not a matter of opinion: inflate
it and see how much input the stream consumes before its final block. All 683
terminate cleanly, so this is ground truth rather than an estimate. Sizes are
rounded up to 16 because ob_seg.s emits `.balign 16` before each end label.

Assets that are not 1172 containers (the uncompressed level p_seg tables, the
ramrom demo recordings, the animation tables) have no such oracle, so their
sizes are LEFT ALONE. Guessing at them would be the same mistake in a new place.

Usage:
    resolve_assets.py --rom <rom.z64> --ld <decomp>/ge007.ld \\
                      --segments hostcompat/ge_segments.h \\
                      [--write] src/host/ge_assets.c
"""
import argparse, os, re, sys, zlib

ENTRY = re.compile(r'\{ "([A-Za-z0-9_]+)", \1, (\d+)u, 0x([0-9A-Fa-f]{8})u \}')
ALIGN = 16


def read_segments(path):
    """The extracted per-ROM segment table: name -> {Start, End, RomStart, RomEnd}."""
    seg = {}
    pat = re.compile(r'#define GE_SEG__(\w+?)Segment(RomStart|RomEnd|Start|End)\s+0x([0-9A-Fa-f]+)u')
    for m in pat.finditer(open(path).read()):
        seg.setdefault(m.group(1), {})[m.group(2)] = int(m.group(3), 16)
    return seg


def read_ld_objects(path):
    """object basename -> segment name, straight out of ge007.ld's BEGIN_SEG blocks.

    Attribution has to come from the link script rather than from the address,
    because two segments (Globalimagetable and rarewarelogo) share the same VRAM
    base of 0x02000000. An address alone cannot say which one a symbol is in.
    """
    txt = open(path).read()
    out = {}
    for m in re.finditer(r'BEGIN_SEG\(\s*(\w+)\s*,[^)]*\)\s*\{(.*?)\}\s*END_SEG', txt, re.S):
        name, body = m.group(1), m.group(2)
        for o in re.finditer(r'([A-Za-z0-9_]+)\.o', body):
            out[o.group(1)] = name
    return out


def attribute(symbols, decomp_root, obj2seg):
    """symbol -> segment, by finding which asset source file defines it."""
    srcs = {}
    for root, _d, files in os.walk(os.path.join(decomp_root, "assets")):
        for f in files:
            base, ext = os.path.splitext(f)
            if base in obj2seg and ext in (".c", ".s", ".h"):
                srcs.setdefault(obj2seg[base], []).append(os.path.join(root, f))
    text = {k: "\n".join(open(p, errors="ignore").read() for p in v)
            for k, v in srcs.items()}
    out = {}
    for s in symbols:
        hits = [k for k, t in text.items() if re.search(r'\b' + re.escape(s) + r'\b', t)]
        if len(hits) == 1:
            out[s] = hits[0]
    return out


def true_compressed_length(rom, off):
    """How many bytes of input the 1172 stream at `off` actually consumes.

    Returns None when the stream does not terminate, which is the honest answer
    for anything that is not a complete deflate stream -- never a guess.
    """
    window = rom[off + 2: off + 2 + 0x100000]
    d = zlib.decompressobj(-15)
    try:
        out = d.decompress(window)
    except zlib.error:
        return None, None
    if not d.eof:
        return None, None
    used = len(window) - len(d.unused_data) + 2
    return (used + ALIGN - 1) & ~(ALIGN - 1), len(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", required=True)
    ap.add_argument("--ld", required=True)
    ap.add_argument("--segments", required=True)
    ap.add_argument("--decomp", required=True)
    ap.add_argument("--write", action="store_true",
                    help="rewrite the file; without it, report only")
    ap.add_argument("assets_c")
    a = ap.parse_args()

    rom = open(a.rom, "rb").read()
    seg = read_segments(a.segments)
    obj2seg = read_ld_objects(a.ld)
    text = open(a.assets_c).read()
    ents = [(m.group(1), int(m.group(2)), int(m.group(3), 16))
            for m in ENTRY.finditer(text)]
    print(f"{len(ents)} assets, ROM {len(rom)} bytes, "
          f"{len(seg)} segments, {len(obj2seg)} objects mapped")

    # ---- 1. offsets -------------------------------------------------------
    # An entry needs re-resolving when its recorded offset is not a plausible
    # ROM position: past the end of the file, or not holding compressed data
    # while its segment says it should.
    suspect = [e for e in ents
               if e[2] and (e[2] + e[1] > len(rom) or rom[e[2]:e[2] + 2] != b"\x11\x72")]
    segof = attribute([n for n, _s, _o in suspect], a.decomp, obj2seg)

    new_off = {}
    for n, s, o in suspect:
        sg = segof.get(n)
        if sg is None or sg not in seg:
            continue
        d = seg[sg]
        if d.get("Start", 0) == d.get("RomStart", 0):
            continue                       # identity segment; nothing to fix
        cand = o - d["Start"] + d["RomStart"]
        if d["RomStart"] <= cand and cand + s <= d.get("RomEnd", len(rom)):
            new_off[n] = (o, cand, sg)
    print(f"\noffsets re-resolved via the segment table: {len(new_off)}")
    for sg in sorted({v[2] for v in new_off.values()}):
        members = [k for k, v in new_off.items() if v[2] == sg]
        ex = sorted(members)[0]
        print(f"   {sg:20} {len(members):3}  e.g. {ex} "
              f"0x{new_off[ex][0]:08X} -> 0x{new_off[ex][1]:08X}")

    # ---- 2. sizes ---------------------------------------------------------
    new_size, unchanged, nomagic = {}, 0, 0
    for n, s, o in ents:
        off = new_off.get(n, (None, o, None))[1]
        if off + 2 > len(rom) or rom[off:off + 2] != b"\x11\x72":
            nomagic += 1
            continue                        # no oracle -- leave it alone
        tl, _outlen = true_compressed_length(rom, off)
        if tl is None:
            continue
        if tl != s:
            new_size[n] = (s, tl)
        else:
            unchanged += 1
    short = sum(1 for old, new in new_size.values() if new > old)
    print(f"\nsizes checked by inflating: {unchanged + len(new_size)} compressed assets")
    print(f"   already correct : {unchanged}")
    print(f"   corrected       : {len(new_size)}  ({short} were too small)")
    print(f"   no 1172 oracle, left alone : {nomagic}")
    for n in sorted(new_size)[:6]:
        old, new = new_size[n]
        print(f"      {n:26} {old:7} -> {new:7}")

    if not a.write:
        print("\n(report only; pass --write to apply)")
        return 0

    # ---- 3. rewrite -------------------------------------------------------
    def fix_entry(m):
        n, s, o = m.group(1), int(m.group(2)), int(m.group(3), 16)
        o = new_off.get(n, (None, o, None))[1]
        s = new_size.get(n, (None, s))[1]
        return f'{{ "{n}", {n}, {s}u, 0x{o:08X}u }}'

    out = ENTRY.sub(fix_entry, text)

    # The array declarations have to match the manifest sizes: the game derives
    # a file's compressed length from the DISTANCE between consecutive entries,
    # so a declaration that disagrees with the manifest reintroduces exactly the
    # bug this is fixing.
    for n, (_old, new) in new_size.items():
        out = re.sub(rf'^unsigned char {n}\[\d+\]', f'unsigned char {n}[{new}]',
                     out, count=1, flags=re.M)

    open(a.assets_c, "w").write(out)
    print(f"\nwrote {a.assets_c}: {len(new_off)} offsets, {len(new_size)} sizes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
