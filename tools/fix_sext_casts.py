#!/usr/bin/env python3
"""
fix_sext_casts.py -- the sign-extension sites the compiler cannot see.

THREE TOOLS, THREE MECHANISMS

  find/fix_implicit_ptr_returns.py   a missing prototype makes the return int
  fix_sext_pointer_sites.py          an implicit int-to-pointer conversion
  this one                           an EXPLICIT cast, which silences both

The first two read clang's diagnostics, because the compiler is the only thing
that knows what is declared where. This one cannot: an explicit cast is exactly
what stops clang from saying anything.

    animation = (struct ModelAnimation *)((s32)ptr_animation_table +
                                          (s32)&ANIM_DATA_bond_eye_walk);

That is well-formed C and the compiler is content. On the N64 it is also
correct: both operands are 32 bits, the sum wraps, and the wrap is the whole
mechanism -- a segment-relative offset plus an RDRAM base like 0x8078B860 comes
back as the right address only because the top bits are discarded. On a 64-bit
host `(s32)ptr` is negative, it sign-extends, and the sum lands at
0xFFFFFFFF80xxxxxx.

WHAT IT MATCHES

A cast to a pointer type whose operand contains `(s32)` or `(int)`:

    (T *)( ... (s32) ... )   ->   GE_PTR(T *, ... (s32) ... )

GE_PTR truncates to u32 before widening, which reproduces the N64's arithmetic
exactly rather than approximating it (hostcompat/ge_addr_compat.h).

WHAT IT DELIBERATELY LEAVES ALONE

- COMPARISONS. `if ((s32)a == (s32)b + (s32)c)` is correct as it stands: both
  sides are truncated the same way, so the comparison has the same answer it had
  on the N64. Rewriting them would be churn in a matching decompilation.
- Casts to non-pointer types, and casts whose operand has no `(s32)`/`(int)` in
  it -- those cannot be members of this family.

Each rewrite is guarded by `#ifdef GE_HOST_PORT` with the original kept verbatim
in the `#else`, so the IDO token stream is unchanged.

VERIFY

There is no compiler check for this one, which is precisely the problem. The
check is the game: run it and see how far it gets, and use src/host/ge_fault.c's
report -- a fault at 0xFFFFFFFF8xxxxxxx means one of these was missed.

Usage:
    fix_sext_casts.py --decomp <root> [--apply]
"""
import argparse
import os
import re
import sys

# `(T *)` or `(struct T **)` immediately followed by a parenthesised operand.
CAST = re.compile(
    r'\((?P<type>(?:struct |union |unsigned |const )*[A-Za-z_]\w*\s*\*+)\)\s*\(')


def matching_paren(s, open_idx):
    depth = 0
    i = open_idx
    while i < len(s):
        if s[i] == "(":
            depth += 1
        elif s[i] == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


CASTDIAG = re.compile(
    r"^(?P<file>[^:]+):(?P<line>\d+):(?P<col>\d+): error: cast to "
    r"'(?P<dst>[^']*)'(?: \(aka '(?P<aka>[^']*)'\))? from smaller integer "
    r"type '(?P<src>[^']*)'")

SIGNED = ("s32", "int", "long", "short", "signed int")


def from_log(a):
    """Sites from clang, for casts whose operand is a plain signed variable.

    The regex path in main() only sees a cast whose operand TEXT contains
    `(s32)`. `(ModelNode *)sp1C`, where sp1C is an s32 local holding a pointer,
    is invisible to it and just as fatal -- that one crashed the gunbarrel
    intro. clang knows the type; the regex cannot.

    Casts from an UNSIGNED 32-bit type are skipped: those zero-extend and are
    already correct, and 127 of the 248 sites are of that kind.
    """
    sites = {}
    for raw in open(a.log, errors="replace"):
        m = CASTDIAG.match(raw.rstrip("\n"))
        if not m or m.group("src") not in SIGNED:
            continue
        sites.setdefault(os.path.abspath(m.group("file")), {})[
            int(m.group("line"))] = (int(m.group("col")),
                                     (m.group("aka") or m.group("dst")).strip())

    total, touched = 0, 0
    for path in sorted(sites):
        if not os.path.exists(path):
            # clang reports the path it was GIVEN, and a couple of files are
            # compiled through a relative path from a different directory. A
            # missing file is a reporting artefact, not a site to skip silently.
            print(f"NOT FOUND {path}")
            continue
        lines = open(path, errors="replace").read().split("\n")
        changed = 0
        for lineno in sorted(sites[path], reverse=True):
            col, dst = sites[path][lineno]
            src = lines[lineno - 1]
            if src.lstrip().startswith("#"):
                continue
            # Idempotence, tested AT THE CAST rather than anywhere on the line.
            # The first version skipped any line already containing GE_PTR, so a
            # statement with two conversions got one of them fixed and the other
            # silently dropped -- which is how
            #   stanDetermineEOF((StanPrefixRecord *)gptr_stan, 0,
            #                    GE_PTR(unsigned char *, gptr_stan));
            # kept its sign-extending first argument through a whole audit.
            if src[col - 1:].startswith("GE_PTR("):
                continue
            if "(*)" in dst:
                # A FUNCTION-pointer cast. The type itself contains parentheses,
                # so the "step over the type" scan below finds the wrong `)` and
                # produces nonsense -- and a function pointer needs the code in
                # the low 4 GB to be callable at all, which is a separate
                # problem with a separate answer (see tools/pin_structs.py).
                # Reported so it is not silently dropped.
                print(f"  SKIP fn-ptr cast {os.path.relpath(path, a.decomp)}:{lineno}")
                continue
            # col points at the `(` of the cast. Step over the type, then take
            # the operand up to its own end.
            i = src.find(")", col - 1)
            if i < 0:
                continue
            got = extract_expr(src, i + 1)
            if got is None:
                continue
            expr, end = got
            new = src[:col - 1] + f"GE_PTR({dst}, {expr})" + src[end:]
            indent = src[:len(src) - len(src.lstrip())]
            lines[lineno - 1:lineno] = [
                f"{indent}#ifdef GE_HOST_PORT",
                f"{indent}/* A signed 32-bit value cast to a pointer. RDRAM has the top bit set,",
                f"{indent}   so it is negative and widening sign-extends. GE_PTR truncates to u32",
                f"{indent}   first. See hostcompat/ge_addr_compat.h; found by clang's",
                f"{indent}   -Wint-to-pointer-cast under GE_INTAUDIT=1. */",
                new,
                f"{indent}#else",
                src,
                f"{indent}#endif",
            ]
            changed += 1
            total += 1
        if changed:
            joined = "\n".join(lines)
            if "#include <ge_addr_compat.h>" not in joined:
                first = re.search(r'^#include[^\n]*$', joined, re.M)
                if first is None:
                    print(f"SKIPPED {os.path.relpath(path, a.decomp)}")
                    continue
                joined = (joined[:first.end()]
                          + "\n#ifdef GE_HOST_PORT\n#include <ge_addr_compat.h>\n#endif"
                          + joined[first.end():])
            touched += 1
            print(f"  {os.path.relpath(path, a.decomp):46} {changed} cast(s)")
            if a.apply:
                open(path, "w").write(joined)
    print(f"\n{total} casts across {touched} files")
    if not a.apply:
        print("\n(report only; pass --apply to write)")
    return 0


def extract_expr(line, col0):
    """Shared with the regex path below; defined once here."""
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
    if i >= n:
        # Ran off the end of the line: the operand continues onto the next one,
        # so this cast cannot be rewritten a line at a time. Refusing is the
        # point -- the first version returned the partial text and produced
        # `GE_PTR(T *, ()` with the rest of the expression stranded below the
        # #else.
        return None
    expr = line[col0:i].strip()
    return (expr, i) if expr else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decomp", required=True)
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--log",
                    help="compile.log from GE_INTAUDIT=1. With this, the sites "
                         "come from clang's -Wint-to-pointer-cast rather than "
                         "from the `(s32)` regex -- which finds the casts whose "
                         "operand is a plain s32 VARIABLE, where there is no "
                         "`(s32)` in the line to match on.")
    a = ap.parse_args()

    if a.log:
        return from_log(a)

    files = []
    for root, _d, names in os.walk(os.path.join(a.decomp, "src")):
        for n in names:
            if n.endswith(".c"):
                files.append(os.path.join(root, n))

    total, touched = 0, 0
    for path in sorted(files):
        text = open(path, errors="replace").read()
        out_lines = []
        changed = 0
        prev_continues = False
        for line in text.split("\n"):
            # A line inside a multi-line macro body, or a directive itself,
            # cannot carry #ifdef around it: the result is a #else inside a
            # #define, which is what happened to ANIM_PTR in
            # src/game/initactorpropstuff.c on the first run. Those sites need a
            # hand-written guard around the WHOLE macro, and there is already
            # one there.
            in_macro = prev_continues
            prev_continues = line.rstrip().endswith("\\")
            if in_macro or line.lstrip().startswith("#"):
                out_lines.append(line)
                continue
            if "GE_PTR(" in line or "GE_RELOC(" in line:
                out_lines.append(line)
                continue
            new = line
            # Right to left, so earlier offsets stay valid.
            for m in reversed(list(CAST.finditer(line))):
                open_idx = m.end() - 1
                close = matching_paren(line, open_idx)
                if close < 0:
                    continue                      # spans lines: leave it
                operand = line[open_idx:close + 1]
                if "(s32)" not in operand and "(int)" not in operand:
                    continue
                # A comparison is already correct on both sides; see the
                # docstring. `==`/`!=` anywhere in the operand is the signal.
                if "==" in operand or "!=" in operand:
                    continue
                inner = operand[1:-1].strip()
                new = (new[:m.start()]
                       + f"GE_PTR({m.group('type').strip()}, {inner})"
                       + new[close + 1:])
            if new != line:
                indent = line[:len(line) - len(line.lstrip())]
                out_lines += [
                    f"{indent}#ifdef GE_HOST_PORT",
                    f"{indent}/* A 32-bit address computation cast to a pointer. On the N64 the",
                    f"{indent}   arithmetic is 32-bit and the wrap is the mechanism; here `(s32)ptr`",
                    f"{indent}   sign-extends. GE_PTR truncates to u32 first, reproducing it exactly.",
                    f"{indent}   See hostcompat/ge_addr_compat.h. */",
                    new,
                    f"{indent}#else",
                    line,
                    f"{indent}#endif",
                ]
                changed += 1
            else:
                out_lines.append(line)
        if changed:
            joined = "\n".join(out_lines)
            if "#include <ge_addr_compat.h>" not in joined:
                first = re.search(r'^#include[^\n]*$', joined, re.M)
                if first is None:
                    print(f"SKIPPED {os.path.relpath(path, a.decomp)}: "
                          f"no #include to hang ge_addr_compat.h on")
                    continue
                joined = (joined[:first.end()]
                          + "\n#ifdef GE_HOST_PORT\n#include <ge_addr_compat.h>\n#endif"
                          + joined[first.end():])
            total += changed
            touched += 1
            print(f"  {os.path.relpath(path, a.decomp):46} {changed} cast(s)")
            if a.apply:
                open(path, "w").write(joined)

    print(f"\n{total} casts across {touched} files")
    if not a.apply:
        print("\n(report only; pass --apply to write)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
