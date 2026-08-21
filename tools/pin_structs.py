#!/usr/bin/env python3
"""
pin_structs.py -- make cartridge-data structs match the cartridge.

WHAT IT DOES

Inserts `GE_N64PTR` into every ordinary pointer member of the structs that
describe data read from the ROM, so those pointers are 4 bytes on a 64-bit host
and the struct matches the N64 layout byte for byte. See hostcompat/ge_n64ptr.h
for why that macro rather than redeclaring each member as `u32`.

WHY THIS IS SAFE FOR THE MATCHING BUILD

`GE_N64PTR` expands to NOTHING when `GE_HOST_PORT` is not defined. The
preprocessed token stream for the N64 build is therefore identical, which is the
standing requirement (patches/HOST-PORT-PATCHES.md 5) -- and identical here by
construction rather than by keeping a verbatim copy in an `#else`. That is the
reason this approach is preferred to 800 guarded member declarations: there is
nothing to keep in sync.

`make` still decides. Run it.

WHAT IT DELIBERATELY LEAVES ALONE

- **Function pointers.** Their layout matters just as much, but a call through a
  32-bit function pointer needs the code to be in the low 4 GB, and the port's
  image is at 0x20000000 by choice rather than by guarantee. Reported, not
  changed.
- **Structs the port itself allocates.** They have no layout obligation, and
  capping them to 4 GB buys nothing.
- **Members already annotated**, so the tool is idempotent.

VERIFICATION IS NOT OPTIONAL

After running this, run tools/check_struct_layout.py again. Every struct listed
here should have dropped out of its mismatch report. That is the check that this
tool did what it claims -- the two tools use independent mechanisms (text edit
vs. DWARF measurement), so agreement between them means something.

Usage:
    pin_structs.py --decomp <root> [--header src/bondtypes.h] [--apply]
"""
import argparse, os, re, sys

# Formats read straight from the cartridge. Every one of these appeared in
# check_struct_layout.py's mismatch list.
CARTRIDGE_STRUCTS = [
    "Vertex", "ModelAnimation", "ModelAnimBitField", "ModelFileHeader",
    "ModelNode", "ModelSkeleton",
    "ModelRoData_HeaderRecord", "ModelRoData_BSPRecord", "ModelRoData_Child",
    "ModelRoData_DisplayListRecord", "ModelRoData_DisplayListPrimaryRecord",
    "ModelRoData_DisplayList_CollisionRecord",
    "ModelRoData_GroupRecord", "ModelRoData_GunfireRecord",
    "ModelRoData_LODRecord", "ModelRoData_ShadowRecord",
    "ModelRoData_SwitchRecord", "ModelRoData_Op05Record",
    "ModelRoData_Op06Record", "ModelRoData_Op07Record",
    "ModelRoData_Op11Record", "ModelRoData_Op17Record",
    "StandFileHeader", "StandFileFooter", "BetaStandTile",

    # NOT the level-setup records. They were tried and backed out, and the
    # reason is worth keeping: src/game/gobjdata.c, pobjdata.c and cobjdata.c
    # are object tables COMPILED INTO the executable, not read from the
    # cartridge. Pinning ObjectRecord and friends therefore buys nothing and
    # costs two things clang will not do:
    #
    #   - a static initializer cannot hold an address-space-cast pointer
    #     ("Unsupported expression in static initializer: addrspacecast"), and
    #     those files are nothing but static initializers taking addresses;
    #   - `&rec->prop->stan` becomes `StandTile *__ptr32 *`, which does not
    #     convert to the `StandTile **` out-parameter that stan.h expects.
    #
    # 12 files and 525 undefined symbols, measured. The rule that falls out:
    # pin a struct only when its instances actually arrive as cartridge BYTES.
]

# NOT pinned, deliberately: structs the port allocates at runtime. `ChrRecord`,
# `Model`, `ModelRenderData`, `Projectile`, `BulletHit`, `HitThing`, `ShotData`,
# `ModelHitEntry`, `AttachedObj`, `Embedment` and the `ModelRwData_*` pair have
# no layout obligation -- nothing reads them from the cartridge -- and a 4-byte
# pointer in them would cap them to the low 4 GB for no benefit. They will still
# appear in check_struct_layout.py's report, and that is correct: the report
# says which structs DIFFER, not which are wrong.

INCLUDE_BLOCK = """#ifdef GE_HOST_PORT
/* GE_N64PTR: 4-byte pointers inside structs that describe cartridge data, so
   the host layout matches the N64's. Expands to nothing off the host path, so
   the N64 token stream is unchanged. See hostcompat/ge_n64ptr.h. */
#include <ge_n64ptr.h>
#else
#define GE_N64PTR
#endif
"""

# A plain pointer member: `Type *name;` or `Type **name[4];`. Function pointers
# -- `Type (*name)(...)` -- do not match, which is intended.
MEMBER = re.compile(
    r'^(?P<lead>\s*)(?P<type>(?:const\s+|volatile\s+|struct\s+|union\s+|unsigned\s+|signed\s+)*'
    r'[A-Za-z_]\w*)(?P<sp>\s*)(?P<stars>\*+)(?P<gap>\s*)(?P<name>[A-Za-z_]\w*)'
    r'(?P<tail>\s*(?:\[[^\]]*\])?\s*;.*)$')


def find_body(text, name):
    """The brace-matched body of `name`, as (start, end) into text.

    Brace matching rather than a regex with `.*?`: a non-greedy match starting
    from the first `typedef struct` in the file happily spans a dozen unrelated
    structs before it finds the closing name, which is how a first attempt at
    this reported that Vertex had 18 pointer members.
    """
    for m in re.finditer(r'\b(?:typedef\s+struct|struct)\s+(\w+)?\s*\{', text):
        open_brace = text.index('{', m.start())
        depth, i = 0, open_brace
        while i < len(text):
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        if i >= len(text):
            continue
        after = text[i + 1:i + 200]
        tag = m.group(1)
        closing = re.match(r'\s*([A-Za-z_]\w*)?\s*;', after)
        closing_name = closing.group(1) if closing else None
        if name in (tag, closing_name):
            return open_brace + 1, i
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decomp", required=True)
    ap.add_argument("--header", default="src/bondtypes.h")
    ap.add_argument("--struct", action="append", default=None,
                    help="pin these instead of the built-in cartridge list")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--unpin", action="store_true",
                    help="remove the annotations again. Reversibility matters: this\n                          is an experiment with a known collision -- see the docstring.")
    a = ap.parse_args()

    path = os.path.join(a.decomp, a.header)
    text = open(path).read()
    total, fnptr, already, changed_structs = 0, 0, 0, []

    for name in (a.struct or CARTRIDGE_STRUCTS):
        span = find_body(text, name)
        if span is None:
            print(f"  {name:42} NOT FOUND")
            continue
        s, e = span
        body = text[s:e]
        if a.unpin:
            new = body.replace("* GE_N64PTR ", "*").replace("** GE_N64PTR ", "**")
            n_here = body.count("GE_N64PTR") - new.count("GE_N64PTR")
            text = text[:s] + new + text[e:]
            total += n_here
            print(f"  {name:42} {n_here:2} unpinned")
            continue
        lines, n_here = body.split("\n"), 0
        for i, line in enumerate(lines):
            if "GE_N64PTR" in line:
                already += 1
                continue
            if re.search(r'\(\s*\*+\s*\w+\s*\)\s*\(', line):
                fnptr += 1
                continue
            m = MEMBER.match(line)
            if not m:
                continue
            lines[i] = (f"{m.group('lead')}{m.group('type')}{m.group('sp')}"
                        f"{m.group('stars')} GE_N64PTR {m.group('name')}{m.group('tail')}")
            n_here += 1
        if n_here:
            text = text[:s] + "\n".join(lines) + text[e:]
            changed_structs.append((name, n_here))
        total += n_here
        print(f"  {name:42} {n_here:2} pointer members pinned")

    if "ge_n64ptr.h" not in text:
        m = re.search(r'^#define _BONDTYPES_H_.*$', text, re.M) or \
            re.search(r'^#include .*$', text, re.M)
        if m:
            text = text[:m.end()] + "\n\n" + INCLUDE_BLOCK + text[m.end():]

    print(f"\n{total} pointer members pinned across {len(changed_structs)} structs")
    print(f"{fnptr} function-pointer members left alone (see the docstring)")
    if already:
        print(f"{already} were already annotated")
    if not a.apply:
        print("\n(report only; pass --apply to write)")
        return 0
    open(path, "w").write(text)
    print(f"\nwrote {path}")
    print("NOW RUN: tools/check_struct_layout.py -- these should have dropped out "
          "of its mismatch list.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
