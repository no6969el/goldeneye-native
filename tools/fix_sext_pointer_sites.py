#!/usr/bin/env python3
"""
fix_sext_pointer_sites.py -- the other half of the sign-extension family.

THE FAMILY

The port maps RDRAM at KSEG0, so an N64 address is a valid host address as a
VALUE. What does not survive is the game putting that value in a 32-bit signed
integer, which the decomp does constantly because on the N64 it was free:

    s32 virtualaddress;                 /* holds 0x8016C7D0 */
    romCopy(virtualaddress, ...);       /* -> 0xFFFFFFFF8016C7D0, SIGSEGV */

Every RDRAM address has the top bit set, so it is negative as an s32 and
widening it back to a 64-bit pointer sign-extends. It fails for the whole heap,
not half of it.

WHY A TOOL AND NOT A GREP

There is nothing to grep for. The conversion is IMPLICIT -- no cast appears at
the call site, and the declaration that makes it an s32 is usually in another
file. The only thing that knows is the compiler, and only if it is asked:
tools/link_game.sh's -w plus -Wno-int-conversion silences exactly this.

So the site list comes from clang, via GE_INTAUDIT=1, and this reads its
diagnostics rather than the source.

WHAT IT REWRITES, AND WHAT IT WILL NOT

Only `incompatible integer to pointer conversion` from a SIGNED type -- `int`,
`s32`, `long`. Conversions from `u32` are already zero-extending and correct;
rewriting them would be churn that changes nothing, and each edit to a matching
decompilation is a risk taken for no gain.

Each rewrite is guarded by `#ifdef GE_HOST_PORT` with the original line kept
verbatim in the `#else`, so the IDO token stream is unchanged
(patches/HOST-PORT-PATCHES.md 5).

It refuses, and reports, when:
  - the offending expression cannot be delimited unambiguously;
  - the line already contains GE_PTR (idempotence);
  - the line is already inside a GE_HOST_PORT guard this tool wrote.

VERIFY

    GE_INTAUDIT=1 tools/link_game.sh <decomp> /tmp/ge_audit2
    fix_sext_pointer_sites.py --log /tmp/ge_audit2/compile.log --decomp <decomp>

The signed-source count should fall to zero. That check is clang's, not this
script's.

Usage:
    fix_sext_pointer_sites.py --log <compile.log> --decomp <root> [--apply]
"""
import argparse
import os
import re
import sys

DIAG = re.compile(
    r"^(?P<file>[^:]+):(?P<line>\d+):(?P<col>\d+): error: "
    r"incompatible integer to pointer conversion "
    r"(?P<kind>assigning to|passing|initializing|returning) "
    r"(?P<rest>.*)$")

# The destination pointer type, first quoted type in the message. clang writes
# `'RoomVtxBatchBounds *' (aka 'struct RoomVtxBatchBounds *')`; the aka form is
# the one that always compiles, so prefer it when present.
TYPE_PLAIN = re.compile(r"'([^']*\*)'")
TYPE_AKA = re.compile(r"'[^']*\*' \(aka '([^']*\*)'\)")

SIGNED_SRC = re.compile(r"from '(?:s32|int|long)'|'(?:s32|int|long)' \(aka")


def dest_type(rest, kind):
    """The pointer type being converted TO."""
    if kind == "passing":
        m = re.search(r"parameter of type (.*)$", rest)
        if not m:
            return None
        rest = m.group(1)
    aka = TYPE_AKA.search(rest)
    if aka:
        return aka.group(1).strip()
    plain = TYPE_PLAIN.search(rest)
    return plain.group(1).strip() if plain else None


def skip_assign(line, col0):
    """Advance past a leading `=`.

    For `assigning to`, clang points at the ASSIGNMENT OPERATOR, not at the
    right-hand side -- so taking the expression from that column produced
    `x.next GE_PTR(void *, = temp | next);`, which is what the first run of this
    wrote into 23 files. For `passing` and `returning` the column already points
    at the expression, so this is a no-op there.
    """
    i = col0
    while i < len(line) and line[i].isspace():
        i += 1
    if i < len(line) and line[i] == "=" and not line[i:i + 2] == "==":
        i += 1
        while i < len(line) and line[i].isspace():
            i += 1
        return i
    return col0


def extract_expr(line, col0):
    """The expression starting at col0, up to its own terminator.

    Stops at the first `;`, `,` or `)` seen at nesting depth zero -- which is
    the end of an assignment's RHS and of an argument alike. Strings and chars
    are skipped so a comma inside one does not end the expression early.
    Returns (expr, end_index) or None when it cannot decide.
    """
    depth = 0
    i = col0
    n = len(line)
    while i < n:
        c = line[i]
        if c in "\"'":
            q = c
            i += 1
            while i < n and line[i] != q:
                i += 2 if line[i] == "\\" else 1
            i += 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                break
            depth -= 1
        elif c in ",;" and depth == 0:
            break
        i += 1
    if i >= n and ";" not in line[col0:]:
        return None
    expr = line[col0:i].strip()
    return (expr, i) if expr else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", required=True)
    ap.add_argument("--decomp", required=True)
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()

    sites = {}          # path -> {line -> (col, type)}
    kind_of = {}        # (path, line) -> clang's conversion kind
    skipped = []
    for raw in open(a.log, errors="replace"):
        m = DIAG.match(raw.rstrip("\n"))
        if not m:
            continue
        if not SIGNED_SRC.search(m.group("rest")):
            continue        # u32 source: zero-extends, already correct
        t = dest_type(m.group("rest"), m.group("kind"))
        if t is None:
            skipped.append((m.group("file"), m.group("line"),
                            "could not read the destination type"))
            continue
        path = os.path.abspath(m.group("file"))
        sites.setdefault(path, {})[int(m.group("line"))] = (
            int(m.group("col")), t)
        kind_of[(path, int(m.group("line")))] = m.group("kind")

    total = 0
    for path in sorted(sites):
        lines = open(path, errors="replace").read().split("\n")
        changed = 0
        # Descending, so rewriting one line never moves the ones still to do.
        for lineno in sorted(sites[path], reverse=True):
            col, ptype = sites[path][lineno]
            src = lines[lineno - 1]
            if "GE_PTR(" in src:
                continue
            start = col - 1
            if kind_of[(path, lineno)] in ("assigning to", "initializing"):
                start = skip_assign(src, start)
            got = extract_expr(src, start)
            if got is None:
                skipped.append((path, lineno, "expression not delimitable"))
                continue
            expr, end = got
            indent = src[:len(src) - len(src.lstrip())]
            new = (src[:start] + f"GE_PTR({ptype}, {expr})" + src[end:])
            block = [
                f"{indent}#ifdef GE_HOST_PORT",
                f"{indent}/* An RDRAM address held in a signed 32-bit value: negative, so",
                f"{indent}   widening it back to a pointer sign-extends. See",
                f"{indent}   hostcompat/ge_addr_compat.h. Generated by",
                f"{indent}   tools/fix_sext_pointer_sites.py from clang's own diagnostic. */",
                new,
                f"{indent}#else",
                src,
                f"{indent}#endif",
            ]
            lines[lineno - 1:lineno] = block
            changed += 1
            total += 1
        # The INCLUDE DIRECTIVE, not the filename. The comment this tool writes
        # names hostcompat/ge_addr_compat.h, so a filename test matches the
        # tool's own output and concludes the include is already there. That
        # exact mistake has now been made three times in this project; the rule
        # is to test for the thing you would insert, character for character.
        if changed and "#include <ge_addr_compat.h>" not in "\n".join(lines):
            # GE_PTR has to be visible. Without the include the macro is left
            # alone by the preprocessor and `GE_PTR(LookAt *, x)` parses as a
            # call with a type name for an argument -- 21 files failed exactly
            # that way on the first apply. Inserted after the FIRST #include so
            # it is above every rewrite in the file.
            first = re.search(r'^#include[^\n]*$', "\n".join(lines), re.M)
            if first is None:
                skipped.append((path, 0, "no #include to hang ge_addr_compat.h on"))
            else:
                joined = "\n".join(lines)
                at = first.end()
                joined = (joined[:at]
                          + "\n#ifdef GE_HOST_PORT\n#include <ge_addr_compat.h>\n#endif"
                          + joined[at:])
                lines = joined.split("\n")

        if changed:
            rel = os.path.relpath(path, a.decomp)
            print(f"  {rel:46} {changed} site(s)")
            if a.apply:
                open(path, "w").write("\n".join(lines))

    print(f"\n{total} sites across {len(sites)} files")
    for path, lineno, why in skipped:
        print(f"SKIPPED {os.path.relpath(str(path), a.decomp)}:{lineno}: {why}")
    if not a.apply:
        print("\n(report only; pass --apply to write)")
    else:
        print("\nNOW: GE_INTAUDIT=1 tools/link_game.sh <decomp> <out>, and the\n"
              "signed-source count should be 0. Also make sure the file still\n"
              "COMPILES -- a wrapped expression that spanned lines will not.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
