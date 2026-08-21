#!/usr/bin/env python3
"""
gen_struct_expand.py -- convert cartridge records into the HOST's layout.

THE PROBLEM THIS SOLVES, AND WHY SWAPPING IS NOT ENOUGH

gen_struct_swap.py converts a record's bytes IN PLACE. That is only correct when
the host struct has the same layout as the cartridge's, which is what
tools/pin_structs.py arranges with GE_N64PTR -- 4-byte pointers, so every field
sits at the offset the N64 put it at.

Some structs cannot be pinned. The level-setup records are the case that forced
this file. pin_structs.py records the measurement: pinning ObjectRecord and its
family costs 12 files and 525 undefined symbols, because src/game/gobjdata.c and
its siblings are static initializer tables of those very structs and clang will
not put an address-space-cast pointer in a static initializer. The conclusion
drawn at the time -- "pin a struct only when its instances arrive as cartridge
bytes" -- was right about the cost and wrong about this family: propDef records
DO arrive as cartridge bytes, in the setup file, at the same time as those
compiled-in tables exist. Both facts are true at once, so neither layout can win.

    ObjectRecord    128 bytes on the N64, 144 on the host
                    prop and model are 4 bytes there and 8 here, so everything
                    from offset 0x10 on is displaced -- including `damage`,
                    `shadecol`, and the door/key/monitor fields that every
                    derived record appends after it.

The measured symptom: sizepropdef() returns sizeof(the host struct) / 4, so the
walk over the setup file strides 144 bytes through 128-byte records, and by the
thirtieth record it is reading prop IDs out of the middle of the previous one.
The game loaded model 7936.

WHAT IT GENERATES

    void geExpand_T(void *dst, const void *src);

`src` is one record exactly as it sits in the file: N64 offsets, big-endian.
`dst` is sizeof(T) bytes of host memory. Every leaf field is located in BOTH
layouts, by path, and copied from the one offset to the other with its bytes
reversed. Fields the host has and the file does not are zeroed; the destination
is cleared first, so a field this tool cannot pair is zero rather than garbage.

Two DWARF builds are the oracle, not one:

    gcc -m32     GE_N64PTR expands to nothing, pointers are 4 bytes -> the
                 cartridge layout, the same build gen_struct_swap.py uses
    clang -m64   GE_N64PTR is live (it needs clang's __ptr32), so already-pinned
                 members stay 4 bytes -> exactly the layout the port compiles

Pairing is by field PATH -- `ObjectRecord.mtx.m[2][3]` -- not by index or by
offset, so a member added on one side shifts nothing on the other and simply
fails to pair, which is reported.

POINTERS ARE WIDENED, NOT COPIED

A 4-byte pointer in the file becomes an 8-byte pointer on the host. The bytes
are read big-endian into a u32 and stored as an address, which zero-extends.
Truncating or memcpy'ing would reproduce the sign-extension family this port has
already paid for twice (src/host/ge_fault.c).

UNIONS

Reused from the swap layer rather than decided again: the bytes are copied and
then the same geSwapUnion_* hook is called on the destination. That only works
when the union is the same size in both layouts; when it is not, the union holds
a pointer, and it is expanded arm-wise like any other member. A union whose
size differs AND whose arms disagree is reported and left zeroed -- there has
not been one yet, and inventing a policy for a case with no instance would be
guessing.

VERIFY

    tools/check_struct_layout.py     says WHICH structs need this
    the generated _Static_assert     says the host sizes have not moved since
                                     the file was written

Usage:
    gen_struct_expand.py --decomp <root> --out src/host/ge_expand
"""
import argparse, os, re, subprocess, sys, tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_struct_swap import (STDINT_SHIM, DEFAULT_INCLUDES, parse_dwarf, aval,
                             resolve, array_count)

# The records that arrive as cartridge bytes but cannot be pinned. Kept separate
# from gen_struct_swap.DEFAULT_STRUCTS on purpose: that list is "structs whose
# bytes need converting", this one is "structs that also need MOVING", and the
# two memberships are decided by different questions.
DEFAULT_STRUCTS = [
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
    # The setup file's INTRO section: a second type-tagged, variable-length
    # list, with a full s32 tag rather than a byte. Only SetupIntroCamera
    # actually changes shape -- it has two `union { integer; char *; }` members
    # and a `prev` pointer, so it is 40 bytes on the cartridge and larger here.
    # The rest are runs of s32 and are listed so the generator's own sizes can
    # be used as the file strides rather than transcribed.
    "SetupIntroEmpty", "SetupIntroSpawn", "struct SetupIntroItem",
    "struct SetupIntroAmmo", "struct SetupIntroSwirl", "struct SetupIntroAnim",
    "struct SetupIntroCuff", "SetupIntroCamera", "struct SetupIntroWatch",
    "struct SetupIntroCredits",

    "struct objective_entry", "struct criteria_roomentered",
    "struct criteria_deposit", "struct criteria_picture",
    "struct setup_objective_text",
]


# ---------------------------------------------------------------------------
# Leaf collection. One walk, run over each of the two DWARF trees.
# ---------------------------------------------------------------------------

class Leaf:
    """One copyable field.

    `path` disambiguates anonymous members by ordinal, because PAIRING is by
    path and ObjectRecord has three members all called `<anon>`: without the
    ordinal the two sides agree on a name that names three different fields, the
    dictionary keeps whichever came last, and two of them get written to the
    same host offset. That produced an ObjectRecord whose bitflags and its
    projectile pointer both landed at +120.

    `sym` keeps the UNDECORATED path, because the union hook symbols were
    generated by gen_struct_swap.py from that spelling and the two tools have to
    agree on the name.
    """
    __slots__ = ("path", "sym", "off", "size", "kind")

    def __init__(self, path, off, size, kind, sym=None):
        self.path, self.off, self.size, self.kind = path, off, size, kind
        self.sym = sym if sym is not None else path

    def __repr__(self):
        return f"<{self.kind} {self.path} @{self.off} {self.size}>"


def collect(dies, d, base, out, ctx, depth=0, seen=None, ctxsym=None):
    """Every leaf of aggregate `d`, as (path, offset, size, kind).

    kind is one of:
       'p'  pointer      -- 4 bytes in the file, whatever the host says here
       's'  scalar       -- an integer, float or enum; byte-reversed on copy
       'b'  bytes        -- a char/u8 run; copied verbatim, no order to fix
       'u'  union        -- opaque; the swap layer's hook decides its contents
    """
    if seen is None:
        seen = set()
    if depth > 8:
        return
    if ctxsym is None:
        ctxsym = ctx
    nanon = 0
    for m in d.children:
        if m.tag != "DW_TAG_member":
            continue
        name = aval(m, "DW_AT_name")
        if not isinstance(name, str):
            name, rawname = f"<anon:{nanon}>", "<anon>"
            nanon += 1
        else:
            rawname = name
        off = aval(m, "DW_AT_data_member_location", 0) or 0
        t = resolve(dies, aval(m, "DW_AT_type"))
        if "DW_AT_bit_size" in m.attrs:
            # A bitfield has no independent bytes: it shares a storage unit with
            # its neighbours. Copy the unit once, keyed by (offset, size), and
            # let the bits ride along. Naming the leaf after the unit rather
            # than the field is deliberate -- the two sides may split the same
            # word into different numbers of bitfields, and pairing by field
            # name would then fail on a word that is byte-identical.
            sz = aval(t, "DW_AT_byte_size", 4) if t is not None else 4
            key = (base + off, sz)
            if key in seen:
                continue
            seen.add(key)
            out.append(Leaf(f"{ctx}#bits@{off}", base + off, sz, 's'))
            continue
        collect_type(dies, t, base + off, out, f"{ctx}.{name}", depth, seen,
                     f"{ctxsym}.{rawname}")


def collect_type(dies, t, at, out, ctx, depth, seen, sym=None):
    if sym is None:
        sym = ctx
    if t is None:
        return
    if t.tag == "DW_TAG_union_type":
        arms = [resolve(dies, aval(m, "DW_AT_type"))
                for m in t.children if m.tag == "DW_TAG_member"]

        # THE FILE CANNOT CONTAIN A HOST POINTER.
        #
        # Every union in these records that changes size does so because one arm
        # is a pointer, and that arm is always the RUNTIME one -- the file holds
        # an index or a pair of ids and the game overwrites it with an address
        # once the thing it names exists:
        #
        #     union { struct PropRecord *first; s32 Index1; };
        #     union { u16 lang_index[2]; char *lang_ptr; };
        #
        # So the pointer arms are dropped and the rest decide the conversion. If
        # what remains all describes the same bytes, that is the answer; the
        # comparison is by computed leaf shape rather than by eye, the same test
        # gen_struct_swap.py applies to decide whether a union is ambiguous at
        # all.
        #
        # This also keeps the two generators agreeing about which unions need a
        # hand-written hook: `union { s8 keyID; u32 keyflags; }` has two
        # non-pointer arms that disagree, so it stays a hook here exactly as it
        # is there, and `Mtxf` and `rgba_u8` likewise.
        shapes = []
        for x in arms:
            if x is not None and x.tag == "DW_TAG_pointer_type":
                continue
            sub = []
            try:
                collect_type(dies, x, at, sub, ctx, depth + 1, set(), sym)
            except Exception:
                sub = None
            shapes.append(sub)
        real = [s for s in shapes if s is not None]
        sigs = [tuple((l.off, l.size, l.kind) for l in s) for s in real]
        if sigs and sigs[0] and all(s == sigs[0] for s in sigs):
            out.extend(real[0])
            return

        # Nothing left but pointers: one address however it is spelled.
        if arms and all(x is not None and x.tag == "DW_TAG_pointer_type"
                        for x in arms):
            out.append(Leaf(ctx, at, 4, 'p', sym))
            return

        out.append(Leaf(ctx, at, aval(t, "DW_AT_byte_size", 0), 'u', sym))
        return
    if t.tag in ("DW_TAG_structure_type", "DW_TAG_class_type"):
        collect(dies, t, at, out, ctx, depth + 1, seen, sym)
        return
    if t.tag == "DW_TAG_array_type":
        el = resolve(dies, aval(t, "DW_AT_type"))
        n = array_count(dies, t)
        esz = aval(el, "DW_AT_byte_size", 0) if el is not None else 0
        if el is not None and el.tag == "DW_TAG_pointer_type":
            esz = esz or 4
        if n is None or el is None or not esz:
            return
        if el.tag == "DW_TAG_base_type" and esz == 1:
            # A byte run. One leaf for the whole thing: emitting 256 one-byte
            # copies for a name field is correct and unreadable.
            out.append(Leaf(ctx, at, n, 'b', sym))
            return
        for i in range(n):
            collect_type(dies, el, at + i * esz, out, f"{ctx}[{i}]", depth + 1, seen, f"{sym}[{i}]")
        return
    if t.tag == "DW_TAG_pointer_type":
        out.append(Leaf(ctx, at, aval(t, "DW_AT_byte_size", 8), 'p', sym))
        return
    sz = aval(t, "DW_AT_byte_size", 0)
    out.append(Leaf(ctx, at, sz, 'b' if sz == 1 else 's', sym))


# ---------------------------------------------------------------------------

def build(cc, extra, tmp, tag, inc, dfn, want, includes):
    """Compile the probe and return its DIE tree, dropping names cc rejects."""
    src = os.path.join(tmp, f"dw_{tag}.c")
    obj = os.path.join(tmp, f"dw_{tag}.o")
    for _round in range(6):
        with open(src, "w") as f:
            f.write("#include <ultra64.h>\n#include <bondtypes.h>\n")
            for h in includes:
                f.write(f"#include <{h}>\n")
            for i, n in enumerate(want):
                f.write(f"{n} ge_probe_{i};\n")
        r = subprocess.run(f"{cc} {extra} -g -c -w -std=gnu99 -fms-extensions "
                           f"{inc} {dfn} {src} -o {obj}",
                           shell=True, capture_output=True, text=True)
        if r.returncode == 0:
            return parse_dwarf(obj), want
        q = "['‘’]"
        bad = set(re.findall(q + r"(\w+)" + q, r.stderr))
        n0 = len(want)
        want = [w for w in want if w.replace("struct ", "") not in bad]
        if len(want) == n0:
            sys.exit(f"{tag} probe failed:\n" + r.stderr[:2000])
    sys.exit(f"{tag} probe did not converge")


def index(dies):
    byname = {}
    for d in dies.values():
        if d.tag in ("DW_TAG_structure_type", "DW_TAG_union_type"):
            n = aval(d, "DW_AT_name")
            if isinstance(n, str) and n not in byname and d.children:
                byname[n] = d
    for d in dies.values():
        if d.tag == "DW_TAG_typedef":
            n = aval(d, "DW_AT_name")
            u = resolve(dies, aval(d, "DW_AT_type"))
            if isinstance(n, str) and u is not None and u.children and n not in byname:
                byname[n] = u
    return byname


PRIM = {2: "geExpand16", 4: "geExpand32", 8: "geExpand64"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decomp", required=True)
    ap.add_argument("--hostcompat", default="hostcompat")
    ap.add_argument("--out", default="src/host/ge_expand")
    ap.add_argument("--struct", action="append", default=None)
    ap.add_argument("--include", action="append", default=None)
    a = ap.parse_args()

    D = a.decomp
    want = a.struct or DEFAULT_STRUCTS
    includes = a.include or DEFAULT_INCLUDES
    tmp = tempfile.mkdtemp(prefix="ge_expand_")
    shim = os.path.join(tmp, "shim")
    os.makedirs(shim)
    open(os.path.join(shim, "stdint.h"), "w").write(STDINT_SHIM)

    inc = " ".join("-I " + x for x in [
        shim, a.hostcompat, D, D + "/include", D + "/include/PR", D + "/src",
        D + "/src/game", D + "/src/inflate", D + "/src/libultra"])
    dfn = ("-D_LANGUAGE_C -DGE_HOST_PORT -DVERSION_US -DLANG_US -DREFRESH_NTSC "
           "-DLEFTOVERDEBUG -DLEFTOVERSPECTRUM -DBUGFIX_R0 -DBYTEMATCH")

    # gcc for the 32-bit side (it is what gen_struct_swap.py measures with, so
    # the two generators cannot disagree about the file layout); clang for the
    # host side, because GE_N64PTR is __ptr32 and gcc does not have it -- under
    # gcc the "host" probe would silently report UNPINNED offsets for structs
    # the port compiles pinned.
    n32, want = build("cc", "-m32", tmp, "n64", inc, dfn, want, includes)
    n64, want = build("clang", "", tmp, "host", inc, dfn, want, includes)
    i32, i64 = index(n32), index(n64)

    bodies, problems = [], []
    for n in want:
        bare = n.replace("struct ", "")
        d32, d64 = i32.get(bare), i64.get(bare)
        if d32 is None or d64 is None:
            problems.append((bare, "no DWARF on one side"))
            continue
        s32 = aval(d32, "DW_AT_byte_size", 0)
        s64 = aval(d64, "DW_AT_byte_size", 0)

        L32, L64 = [], []
        collect(n32, d32, 0, L32, bare)
        collect(n64, d64, 0, L64, bare)
        by64 = {l.path: l for l in L64}

        out, unpaired = [], []
        for l in L32:
            h = by64.get(l.path)
            if h is None:
                unpaired.append(l.path)
                continue
            if l.kind == 'p':
                out.append(f"    geExpandPtr((unsigned char *)dst + {h.off},"
                           f" (const unsigned char *)src + {l.off});"
                           f"   /* {l.path} */")
            elif l.kind == 'b':
                nbytes = min(l.size, h.size)
                out.append(f"    geExpandBytes((unsigned char *)dst + {h.off},"
                           f" (const unsigned char *)src + {l.off}, {nbytes});"
                           f"   /* {l.path} */")
            elif l.kind == 'u':
                if l.size == h.size:
                    sym = "geSwapUnion_%s_at%d" % (
                        re.sub(r'\W', '_', l.sym.strip('.')), l.off)
                    out.append(f"    geExpandBytes((unsigned char *)dst + {h.off},"
                               f" (const unsigned char *)src + {l.off}, {l.size});")
                    out.append(f"    {sym}((unsigned char *)dst + {h.off});"
                               f"   /* union {l.path}: bytes moved, the swap layer's"
                               f" hook decides them */")
                else:
                    # It changed size, so it holds a pointer. There is no
                    # instance of this yet; left zeroed and REPORTED rather than
                    # guessed, because a wrong guess here is silent.
                    problems.append((bare, f"union {l.path} is {l.size} bytes in "
                                           f"the file and {h.size} on the host; "
                                           f"left zeroed"))
            else:
                fn = PRIM.get(l.size)
                if fn is None:
                    problems.append((bare, f"{l.path}: size {l.size}, not copied"))
                    continue
                if h.size != l.size:
                    problems.append((bare, f"{l.path}: {l.size} bytes in the file, "
                                           f"{h.size} on the host; not copied"))
                    continue
                out.append(f"    {fn}((unsigned char *)dst + {h.off},"
                           f" (const unsigned char *)src + {l.off});"
                           f"   /* {l.path} */")
        for p in unpaired:
            out.append(f"    /* {p}: no host member of this path -- NOT copied */")
        bodies.append((bare, n, s32, s64, out))

    hdr = os.path.abspath(a.out + ".h")
    csrc = os.path.abspath(a.out + ".c")
    os.makedirs(os.path.dirname(csrc), exist_ok=True)

    with open(hdr, "w") as f:
        f.write("/*\n * ge_expand.h -- GENERATED by tools/gen_struct_expand.py. Do not edit.\n"
                " *\n * Convert one cartridge record into the host's layout.\n"
                " *\n * `src` is the record as it sits in the file: N64 offsets, big-endian.\n"
                " * `dst` is sizeof(T) bytes of host memory, cleared first, then filled\n"
                " * field by field. Use these where the struct CANNOT be pinned with\n"
                " * GE_N64PTR -- otherwise geSwap_*() in ge_swap.h is cheaper and converts\n"
                " * in place. See the tool's docstring for which is which.\n */\n"
                "#ifndef GE_EXPAND_H\n#define GE_EXPAND_H\n\n"
                "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
        f.write("/* The N64 size of each record: what to stride by in the FILE.\n"
                " * sizeof(T) is the host size, which is what to stride by in memory --\n"
                " * confusing the two is the bug this whole file exists for. */\n")
        for bare, _n, s32, s64, _o in bodies:
            f.write(f"#define GE_N64SIZEOF_{bare} {s32}   /* host: {s64} */\n")
        f.write("\n")
        for bare, n, s32, s64, _o in bodies:
            f.write(f"void geExpand_{bare}(void *dst, const void *src);"
                    f"   /* {s32} -> {s64} bytes */\n")
        f.write("\n#ifdef __cplusplus\n}\n#endif\n\n#endif /* GE_EXPAND_H */\n")

    with open(csrc, "w") as f:
        f.write("/*\n * ge_expand.c -- GENERATED by tools/gen_struct_expand.py. Do not edit.\n */\n"
                "#include \"ge_expand.h\"\n#include \"ge_swap.h\"\n\n"
                "#include <string.h>\n\n"
                "static void geExpandBytes(unsigned char *d, const unsigned char *s,\n"
                "                          unsigned int n)\n{\n"
                "    memcpy(d, s, n);\n}\n\n"
                "static void geExpand16(unsigned char *d, const unsigned char *s)\n{\n"
                "    d[0] = s[1]; d[1] = s[0];\n}\n\n"
                "static void geExpand32(unsigned char *d, const unsigned char *s)\n{\n"
                "    d[0] = s[3]; d[1] = s[2]; d[2] = s[1]; d[3] = s[0];\n}\n\n"
                "static void geExpand64(unsigned char *d, const unsigned char *s)\n{\n"
                "    int i;\n    for (i = 0; i < 8; ++i) d[i] = s[7 - i];\n}\n\n"
                "/*\n"
                " * A 4-byte pointer in the file becomes an 8-byte one here.\n"
                " *\n"
                " * Read big-endian into an unsigned 32-bit value and store it as an\n"
                " * address. Going through `unsigned int` is what makes it ZERO-extend:\n"
                " * every RDRAM address has the top bit set, so reading it as signed and\n"
                " * widening would produce 0xFFFFFFFF8xxxxxxx -- the family in\n"
                " * src/host/ge_fault.c, which this port has paid for twice already.\n"
                " */\n"
                "static void geExpandPtr(unsigned char *d, const unsigned char *s)\n{\n"
                "    unsigned int v = ((unsigned int)s[0] << 24) |\n"
                "                     ((unsigned int)s[1] << 16) |\n"
                "                     ((unsigned int)s[2] << 8)  |\n"
                "                     ((unsigned int)s[3]);\n"
                "    void *p = (void *)(unsigned long)v;\n"
                "    memcpy(d, &p, sizeof(p));\n}\n\n")
        for bare, n, s32, s64, out in bodies:
            f.write(f"/* {bare}: {s32} bytes in the file -> {s64} bytes here */\n"
                    f"void geExpand_{bare}(void *dst, const void *src)\n{{\n"
                    f"    memset(dst, 0, {s64});\n")
            f.write("\n".join(out) if out else "    (void)src;")
            f.write("\n}\n\n")

    print(f"{len(bodies)} expanders -> {csrc}")
    for name, why in problems:
        print(f"  {name}: {why}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
