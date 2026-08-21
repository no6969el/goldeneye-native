#!/usr/bin/env python3
"""
gen_struct_swap.py -- generate the byte-swap layer from the compiler's own layouts.

WHY GENERATED

The N64 is big-endian and x86-64 is not, so every multi-byte field in cartridge
data is reversed. `src/ultra/os_io.h` said from the start that swapping "belongs
in the asset loaders where the structure is known" -- and then those loaders were
never written, which is what P3h is about.

Writing 34 of them by hand is the obvious approach and the wrong one. A swapper
is a transcription of a struct's field layout, and a transcription that drifts
from the struct it describes fails silently: the data still decodes, just to the
wrong numbers. The struct definition is the specification, so the swapper should
be derived from it rather than written alongside it.

WHERE THE LAYOUTS COME FROM

DWARF, from a 32-bit build of the decomp's own headers. On i386 a pointer is 4
bytes, as on the N64, so the offsets and sizes DWARF reports for that build are
the cartridge's. This is not a parser for C -- it is the compiler's answer, which
is the only one that is not a guess. `tools/check_struct_layout.py` uses the same
oracle to find WHICH structs need this.

THE UNION PROBLEM, WHICH IS REAL AND IS NOT GUESSED AT

A union has no single correct swap. `Vertex` is the case that matters:

    union {
        struct { s16 s; s16 t; };     -> swap as two 2-byte fields
        struct Vertex *LinkedTo;      -> swap as one 4-byte field
        ...
    };

Those produce DIFFERENT bytes. Which one is right depends on whether the vertex
came from a display list or a collision model, and only the caller knows. So the
generator does NOT choose. It emits a call to a hand-written hook and declares it
without defining it, which means an unimplemented union is a LINK ERROR rather
than a silently wrong swap. Getting a compile failure for the ambiguous cases is
the whole point.

Usage:
    gen_struct_swap.py --decomp <root> --out src/host/ge_swap [--struct Name ...]
"""
import argparse, os, re, subprocess, sys, tempfile

STDINT_SHIM = """#ifndef _STDINT_H
#define _STDINT_H
typedef signed char int8_t; typedef unsigned char uint8_t;
typedef short int16_t; typedef unsigned short uint16_t;
typedef int int32_t; typedef unsigned int uint32_t;
typedef long long int64_t; typedef unsigned long long uint64_t;
typedef long intptr_t; typedef unsigned long uintptr_t;
#endif
"""

# The formats that are read straight from the cartridge. Everything here is a
# struct the game does not lay out itself, so its bytes arrive big-endian.
DEFAULT_STRUCTS = [
    "Vertex", "ModelAnimation", "ModelAnimBitField", "ModelFileHeader",
    "ModelNode", "ModelSkeleton",
    "ModelRoData_HeaderRecord", "ModelRoData_BSPRecord", "ModelRoData_Child",
    "ModelRoData_DisplayListRecord", "ModelRoData_DisplayListPrimaryRecord",
    "ModelRoData_GroupRecord", "ModelRoData_GunfireRecord",
    "ModelRoData_LODRecord", "ModelRoData_ShadowRecord",
    "ModelRoData_SwitchRecord", "ModelRoData_Op05Record",
    "ModelRoData_Op06Record", "ModelRoData_Op07Record",
    "ModelRoData_Op11Record", "ModelRoData_Op17Record",
    "StandFileHeader", "StandFileFooter", "BetaStandTile",
    "ModelRoData_DisplayList_CollisionRecord", "struct fontchar",

    # ---- the audio bank file (music.h, PR/libaudio.h) -------------------------
    # Reached through --include, since neither header is pulled in by
    # bondtypes.h. Dropping them here is how the bank file silently went back to
    # being big-endian once already.
    "ALBankFile", "ALBank", "ALInstrument", "ALSound", "ALKeyMap",
    "ALEnvelope", "RareALSeqData",

    # ---- the level setup file's propDef records ------------------------------
    #
    # A setup file is a run of variable-length, TYPE-TAGGED records: the type is
    # a byte at offset 3 of the header, and sizepropdef() (src/game/loadobjectmodel.c)
    # maps it to a length in words. The type byte needs no conversion, which is
    # what makes a dispatching walk possible at all -- the walker can read the tag
    # before anything has been swapped.
    #
    # These are NOT all-word structures. ObjectRecord alone carries `s16 obj`
    # against `s16 pad`, and a word swap would put each in the other's place with
    # its own bytes corrected -- a prop that renders in the wrong spot rather than
    # a crash. Hence one derived swapper per record type.
    "PropDefHeaderRecord",
    "ObjectRecord", "GuardRecord", "DoorRecord", "GlobalDoorScaleRecord",
    "KeyRecord", "TintedGlassRecord", "CCTVRecord", "WeaponObjRecord",
    "MonitorObjRecord", "MultiMonitorObjRecord", "AutogunRecord",
    "LinkRecord", "GuardAttributeRecord", "MultiAmmoCrateRecord",
    "BodyArmourRecord", "TagObjectRecord", "RenameObjectRecord",
    "LockDoorRecord", "VehichleRecord", "AircraftRecord", "TankRecord",
    "CutsceneRecord", "SafeObjectRecord",
    "AmmoCrateRecord", "HatRecord", "GlassRecord", "SafeRecord",
    "GasReleasingRecord",
    # These five are plain `struct X` with no typedef, so they have to be named
    # that way: `objective_entry ge_probe_N;` does not compile, the probe drops
    # the name, and the record silently gets no swapper. Spelled in full they
    # instantiate, and the DWARF lookup below already strips the keyword.
    "struct objective_entry", "struct criteria_roomentered",
    "struct criteria_deposit", "struct criteria_picture",
    "struct setup_objective_text",
]


# Headers bondtypes.h does not pull in, but which hold structs in the list above.
# Defaulted rather than left to the command line: the invocation lives in
# people's shell history, the list of structs lives in this file, and the two
# drifting apart is how ALBankFile and friends vanished from a regeneration and
# took the audio bank back to big-endian.
DEFAULT_INCLUDES = ["music.h", "PR/libaudio.h"]


class Die:
    __slots__ = ("off", "tag", "attrs", "children", "parent")

    def __init__(self, off, tag):
        self.off, self.tag, self.attrs, self.children, self.parent = off, tag, {}, [], None


def parse_dwarf(obj):
    """Build the DIE tree from `objdump --dwarf=info`. Offsets key the tree."""
    out = subprocess.run(["objdump", "--dwarf=info", obj],
                         capture_output=True, text=True).stdout
    dies, stack, cur = {}, [], None
    hdr = re.compile(r'^\s*<(\d+)><([0-9a-f]+)>:\s+Abbrev Number:\s+(\d+)(?:\s+\((\w+)\))?')
    att = re.compile(r'^\s*<[0-9a-f]+>\s+(DW_AT_\w+)\s*:\s*(.*)$')
    for line in out.splitlines():
        m = hdr.match(line)
        if m:
            depth, off, _abbr, tag = int(m.group(1)), int(m.group(2), 16), m.group(3), m.group(4)
            if tag is None:                      # abbrev 0 = end of a sibling chain
                while stack and len(stack) >= depth:
                    stack.pop()
                cur = None
                continue
            d = Die(off, tag)
            dies[off] = d
            while stack and len(stack) >= depth:
                stack.pop()
            if stack:
                d.parent = stack[-1]
                stack[-1].children.append(d)
            stack.append(d)
            cur = d
            continue
        m = att.match(line)
        if m and cur is not None:
            k, v = m.group(1), m.group(2).strip()
            # gcc emits "(indirect string, offset: 0x..): name"; clang with
            # DWARF5 emits "(indexed string: 0x..): name". Handle both, or the
            # tool silently sees no names at all under one of the compilers.
            s = re.match(r'\((?:indirect string, offset:|indexed string:)\s*0x[0-9a-f]+\):\s*(.*)$', v)
            if s:
                v = s.group(1).strip()
            cur.attrs[k] = v
    return dies


def aval(d, k, default=None):
    v = d.attrs.get(k)
    if v is None:
        return default
    m = re.match(r'<0x([0-9a-f]+)>', v)
    if m:
        return int(m.group(1), 16)
    m = re.match(r'(\d+)$', v)
    if m:
        return int(m.group(1))
    m = re.match(r'0x([0-9a-f]+)$', v)
    if m:
        return int(m.group(1), 16)
    return v


def resolve(dies, off):
    """Strip typedefs/const/volatile down to the underlying type DIE."""
    seen = set()
    while off is not None and off in dies and off not in seen:
        seen.add(off)
        d = dies[off]
        if d.tag in ("DW_TAG_typedef", "DW_TAG_const_type",
                     "DW_TAG_volatile_type"):
            off = aval(d, "DW_AT_type")
            continue
        return d
    return None


def array_count(dies, d):
    n = 1
    for c in d.children:
        if c.tag == "DW_TAG_subrange_type":
            ub = aval(c, "DW_AT_upper_bound")
            cnt = aval(c, "DW_AT_count")
            if cnt is not None:
                n *= cnt
            elif ub is not None:
                n *= ub + 1
            else:
                return None
    return n


def emit(dies, d, base, out, ctx, hooks, depth=0):
    """Append swap statements for the aggregate `d` located at `base`."""
    if depth > 8:
        out.append(f"    /* depth limit reached at {ctx} */")
        return
    for m in d.children:
        if m.tag != "DW_TAG_member":
            continue
        name = aval(m, "DW_AT_name", "<anon>")
        off = aval(m, "DW_AT_data_member_location", 0) or 0
        if "DW_AT_bit_size" in m.attrs:
            out.append(f"    /* {ctx}.{name}: bitfield, swapped with its storage unit */")
            continue
        t = resolve(dies, aval(m, "DW_AT_type"))
        emit_type(dies, t, base + off, out, f"{ctx}.{name}", hooks, depth)


def emit_type(dies, t, at, out, ctx, hooks, depth):
    if t is None:
        out.append(f"    /* {ctx}: unresolved type, NOT swapped */")
        return
    if t.tag == "DW_TAG_union_type":
        # FIRST: is it actually ambiguous?
        #
        # Many of these unions are two spellings of the same bytes -- a coord3d
        # as `f32 v[3]` or as `struct { f32 x, y, z; }`, or a pointer that has
        # two names because the callers disagree about what it points at. Every
        # arm then produces the IDENTICAL swap, and there is nothing to decide.
        #
        # Comparing the generated operations settles that by computation rather
        # than by eye. Only a union whose arms genuinely disagree -- Vertex's
        # `{s16 s; s16 t}` against a `Vertex *` -- becomes a hook. That keeps the
        # hooks meaningful: every one that survives is a real decision, so an
        # undefined one is worth stopping the link for.
        arms = []
        for m in t.children:
            if m.tag != "DW_TAG_member":
                continue
            sub = []
            mt = resolve(dies, aval(m, "DW_AT_type"))
            try:
                emit_type(dies, mt, at, sub, ctx, [], depth + 1)
            except Exception:
                sub = None
            arms.append(sub)
        real = [a for a in arms if a is not None]
        ops = [tuple(re.findall(r'geSwap(\d+)\(\(unsigned char \*\)p \+ (\d+)\)', "\n".join(a)))
               for a in real]
        if ops and all(o == ops[0] for o in ops) and ops[0]:
            out.append(f"    /* union {ctx}: every arm swaps identically -- no ambiguity */")
            out.extend(real[0])
            return
        # No single correct answer. Emit a hook and let the linker demand it.
        # The offset is part of the name. Two anonymous unions in one struct
        # otherwise collapse to the same symbol -- Vertex has exactly that, at
        # 0x08 and 0x0C -- and implementing one would silently implement the
        # other with the wrong field widths.
        sym = "geSwapUnion_%s_at%d" % (re.sub(r'\W', '_', ctx.strip('.')), at)
        hooks.append((sym, ctx, aval(t, "DW_AT_byte_size", 0)))
        out.append(f"    {sym}((unsigned char *)p + {at});"
                   f"  /* union: {ctx} -- see the hook */")
        return
    if t.tag in ("DW_TAG_structure_type", "DW_TAG_class_type"):
        emit(dies, t, at, out, ctx, hooks, depth + 1)
        return
    if t.tag == "DW_TAG_array_type":
        el = resolve(dies, aval(t, "DW_AT_type"))
        n = array_count(dies, t)
        esz = aval(el, "DW_AT_byte_size", 0) if el is not None else 0
        if n is None or el is None or not esz:
            out.append(f"    /* {ctx}: array of unknown extent, NOT swapped */")
            return
        if el.tag == "DW_TAG_base_type" and esz == 1:
            return                                   # bytes need no swap
        for i in range(n):
            emit_type(dies, el, at + i * esz, out, f"{ctx}[{i}]", hooks, depth + 1)
        return
    # scalar, enum or pointer
    sz = aval(t, "DW_AT_byte_size", 0)
    if t.tag == "DW_TAG_pointer_type":
        sz = 4                                       # 4 in the 32-bit layout
    if sz in (2, 4, 8):
        out.append(f"    geSwap{sz*8}((unsigned char *)p + {at});   /* {ctx} */")
    elif sz == 1:
        pass
    else:
        out.append(f"    /* {ctx}: size {sz}, NOT swapped */")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decomp", required=True)
    ap.add_argument("--hostcompat", default="hostcompat")
    ap.add_argument("--out", default="src/host/ge_swap")
    ap.add_argument("--struct", action="append", default=None)
    ap.add_argument("--include", action="append", default=None,
                    help="extra headers to include in the probe, e.g. music.h")
    a = ap.parse_args()

    D = a.decomp
    want = a.struct or DEFAULT_STRUCTS
    tmp = tempfile.mkdtemp(prefix="ge_swap_")
    shim = os.path.join(tmp, "shim")
    os.makedirs(shim)
    open(os.path.join(shim, "stdint.h"), "w").write(STDINT_SHIM)

    inc = " ".join("-I " + x for x in [
        shim, a.hostcompat, D, D + "/include", D + "/include/PR", D + "/src",
        D + "/src/game", D + "/src/inflate", D + "/src/libultra"])
    dfn = ("-D_LANGUAGE_C -DGE_HOST_PORT -DVERSION_US -DLANG_US -DREFRESH_NTSC "
           "-DLEFTOVERDEBUG -DLEFTOVERSPECTRUM -DBUGFIX_R0 -DBYTEMATCH")

    # Instantiate each wanted struct so the compiler emits DWARF for it. Names
    # the compiler rejects are dropped -- same convergence loop as the audit.
    src = os.path.join(tmp, "dw.c")
    obj = os.path.join(tmp, "dw32.o")
    for _round in range(6):
        with open(src, "w") as f:
            f.write("#include <ultra64.h>\n#include <bondtypes.h>\n")
            for h in (a.include or DEFAULT_INCLUDES):
                f.write(f"#include <{h}>\n")
            for i, n in enumerate(want):
                f.write(f"{n} ge_probe_{i};\n")
        r = subprocess.run(f"cc -m32 -g -c -w -std=gnu99 -fms-extensions {inc} {dfn} "
                           f"{src} -o {obj}", shell=True, capture_output=True, text=True)
        if r.returncode == 0:
            break
        q = "['‘’]"
        bad = set(re.findall(q + r"(\w+)" + q, r.stderr))
        n0 = len(want)
        want = [w for w in want if w.replace("struct ", "") not in bad]
        if len(want) == n0:
            sys.exit("32-bit probe failed:\n" + r.stderr[:1500])
    else:
        sys.exit("32-bit probe did not converge")

    dies = parse_dwarf(obj)
    byname = {}
    for d in dies.values():
        if d.tag in ("DW_TAG_structure_type", "DW_TAG_union_type"):
            n = aval(d, "DW_AT_name")
            if isinstance(n, str) and n not in byname and d.children:
                byname[n] = d
    # typedef name -> underlying struct
    for d in dies.values():
        if d.tag == "DW_TAG_typedef":
            n = aval(d, "DW_AT_name")
            u = resolve(dies, aval(d, "DW_AT_type"))
            if isinstance(n, str) and u is not None and u.children and n not in byname:
                byname[n] = u

    hooks, bodies, done = [], [], []
    for n in want:
        bare = n.replace("struct ", "")
        d = byname.get(bare)
        if d is None:
            print(f"  no DWARF for {n} -- skipped", file=sys.stderr)
            continue
        size = aval(d, "DW_AT_byte_size", 0)
        out = []
        emit(dies, d, 0, out, bare, hooks)
        bodies.append((bare, size, out))
        done.append(bare)

    hdr = os.path.abspath(a.out + ".h")
    csrc = os.path.abspath(a.out + ".c")
    os.makedirs(os.path.dirname(csrc), exist_ok=True)

    with open(hdr, "w") as f:
        f.write("/*\n * ge_swap.h -- GENERATED by tools/gen_struct_swap.py. Do not edit.\n"
                " *\n * Byte-swap cartridge data in place, from the N64's layout.\n"
                " * Sizes and offsets come from a 32-bit DWARF build of the decomp's own\n"
                " * headers, so they are the compiler's answer rather than a transcription.\n"
                " *\n * Each function swaps ONE record in place. The struct must be pinned to the\n"
                " * N64 layout (pointer members as u32) for these offsets to describe it --\n"
                " * see PRIORITIES.md P3h.\n */\n"
                "#ifndef GE_SWAP_H\n#define GE_SWAP_H\n\n"
                "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
        f.write("/* The primitives, exposed: a loader sometimes has to swap a single\n"
                " * field (a count it needs before it can size the rest of the table).\n */\n"
                "void geSwap16(unsigned char *p);\n"
                "void geSwap32(unsigned char *p);\n"
                "void geSwap64(unsigned char *p);\n\n")
        for n, size, _ in bodies:
            f.write(f"void geSwap_{n}(void *p);   /* {size} bytes on the N64 */\n")
        if hooks:
            f.write("\n/*\n * UNION HOOKS -- declared, deliberately not defined.\n"
                    " *\n * A union has no single correct swap: which member is live decides the\n"
                    " * field widths, and only the caller knows. These are left undefined so an\n"
                    " * unimplemented one is a LINK ERROR. A generated guess would produce data\n"
                    " * that decodes to the wrong numbers rather than failing.\n */\n")
            for sym, ctx, sz in dict.fromkeys((h[0], h[1], h[2]) for h in hooks):
                pass
            seen = set()
            for sym, ctx, sz in hooks:
                if sym in seen:
                    continue
                seen.add(sym)
                f.write(f"void {sym}(void *p);   /* {ctx}, {sz} bytes */\n")
        f.write("\n#ifdef __cplusplus\n}\n#endif\n\n#endif /* GE_SWAP_H */\n")

    with open(csrc, "w") as f:
        f.write("/*\n * ge_swap.c -- GENERATED by tools/gen_struct_swap.py. Do not edit.\n */\n"
                "#include \"ge_swap.h\"\n\n"
                "void geSwap16(unsigned char *p)\n{\n"
                "    unsigned char t = p[0]; p[0] = p[1]; p[1] = t;\n}\n\n"
                "void geSwap32(unsigned char *p)\n{\n"
                "    unsigned char t;\n"
                "    t = p[0]; p[0] = p[3]; p[3] = t;\n"
                "    t = p[1]; p[1] = p[2]; p[2] = t;\n}\n\n"
                "void geSwap64(unsigned char *p)\n{\n"
                "    unsigned char t; int i;\n"
                "    for (i = 0; i < 4; ++i) { t = p[i]; p[i] = p[7-i]; p[7-i] = t; }\n}\n\n")
        for n, size, out in bodies:
            f.write(f"/* {n}: {size} bytes on the N64 */\n"
                    f"void geSwap_{n}(void *p)\n{{\n")
            f.write("\n".join(out) if out else "    (void)p;")
            f.write("\n}\n\n")

    print(f"{len(bodies)} swap functions -> {csrc}")
    uniq = sorted({h[0] for h in hooks})
    if uniq:
        print(f"{len(uniq)} union hooks left UNDEFINED on purpose "
              f"(an unimplemented one is a link error, not a wrong swap):")
        for s in uniq:
            print(f"    {s}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
