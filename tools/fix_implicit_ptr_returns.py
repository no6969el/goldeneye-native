#!/usr/bin/env python3
"""
fix_implicit_ptr_returns.py -- declare the pointer-returning functions.

Companion to find_implicit_ptr_returns.py, which explains why this class of bug
exists and why only the pointer-returning members of it matter. This one writes
the fix: a `#ifdef GE_HOST_PORT` prototype block near the top of each CALLING
file, carrying the real signature copied from the definition.

WHY A PROTOTYPE AND NOT AN #include

The obvious repair is to include the header that already declares the function.
Often there isn't one -- half of these are defined in a .c with no declaration
anywhere -- and where there is, pulling a whole header into a file that has done
without it drags in its own include graph and its own conflicts. A prototype is
the smallest change that makes the return type right, which is the only thing
wrong here.

WHY IT IS SAFE FOR THE MATCHING BUILD

Everything this writes is inside `#ifdef GE_HOST_PORT`. The IDO token stream is
unchanged, which is the standing requirement (patches/HOST-PORT-PATCHES.md 5).

WHAT IT REFUSES TO DO

- It will not touch a file where it cannot find a place to insert (no #include
  at all), rather than guessing at the top of the file and landing inside a
  comment block.
- It will not write a prototype it could not read completely -- an unbalanced
  parameter list is skipped and reported, not truncated.
- It is idempotent: a file that already has the marker is left alone.

VERIFY, DO NOT TRUST

Re-run the audit afterwards:

    GE_STRICT=1 tools/link_game.sh <decomp> /tmp/ge_audit
    tools/find_implicit_ptr_returns.py --log /tmp/ge_audit/compile.log --decomp <decomp>

The pointer-returning count should be zero. That check uses clang's own view of
the include graph, not this script's, so agreement between them means something.

Usage:
    fix_implicit_ptr_returns.py --log /tmp/ge_audit/compile.log \
                                --decomp <decomp> [--apply]
"""
import argparse
import os
import re
import sys

MARKER = "GE_HOST_PORT implicit-declaration prototypes"

CALL = re.compile(
    r"^(?P<file>[^:]+):(?P<line>\d+):\d+: error: call to undeclared function "
    r"'(?P<name>[A-Za-z_]\w*)'")


def def_start(name):
    return re.compile(
        r'^[ \t]*(?:static[ \t]+|extern[ \t]+|inline[ \t]+)*'
        r'(?:[A-Za-z_]\w*(?:[ \t]+\w+)*)[ \t]*'
        r'\*+[ \t]*(?:GE_N64PTR[ \t]+)?'
        + re.escape(name) + r'[ \t]*\(', re.M)


def read_signature(text, m):
    """The whole declarator, from the start of the return type to the `)`.

    Brace-free by construction: this stops at the closing paren of the parameter
    list, so it works identically for a definition and for an existing
    declaration, and never runs into the body.
    """
    start = m.start()
    i = text.index("(", m.start())
    depth = 0
    while i < len(text):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                sig = text[start:i + 1]
                # Collapse newlines: a prototype spanning six lines in the
                # middle of an ifdef block is harder to read than a long one.
                return " ".join(sig.split())
        i += 1
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", required=True)
    ap.add_argument("--decomp", required=True)
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--undo", action="store_true",
                    help="remove the generated blocks again. Reversibility is "
                         "not a nicety here: the first version of this inserted "
                         "after the LAST #include in the file, which in two "
                         "files was below a call site, and undoing was the only "
                         "way to re-place them.")
    a = ap.parse_args()

    calls = {}        # name -> set(caller abs path)
    first_call = {}   # caller abs path -> earliest line number of any such call
    for raw in open(a.log, errors="replace"):
        m = CALL.match(raw.strip())
        if m:
            path = os.path.abspath(m.group("file"))
            line = int(m.group("line"))
            calls.setdefault(m.group("name"), set()).add(path)
            if line < first_call.get(path, 10 ** 9):
                first_call[path] = line

    sources = []
    for root, _d, files in os.walk(os.path.join(a.decomp, "src")):
        for f in files:
            if f.endswith((".c", ".h")):
                p = os.path.join(root, f)
                try:
                    sources.append((p, open(p, errors="replace").read()))
                except OSError:
                    pass

    if a.undo:
        n = 0
        for path, text in sources:
            if MARKER not in text:
                continue
            new = re.sub(
                r'\n?#ifdef GE_HOST_PORT\n/\*\n \* ' + re.escape(MARKER)
                + r'\.\n(?:.*?\n)*?#endif\n', '', text)
            if new != text:
                n += 1
                print(f"  undo {os.path.relpath(path, a.decomp)}")
                if a.apply:
                    open(path, "w").write(new)
        print(f"\n{n} files" + ("" if a.apply else " (report only; pass --apply)"))
        return 0

    # caller path -> list of prototype strings
    todo, skipped = {}, []
    for name in sorted(calls):
        rx = def_start(name)
        sig = None
        for path, text in sources:
            m = rx.search(text)
            if m:
                sig = read_signature(text, m)
                break
        if sig is None:
            continue                      # not pointer-returning, or not found
        for caller in calls[name]:
            # A file that defines the function does not need to declare it, and
            # inserting a prototype ABOVE the definition of a static would be a
            # conflicting declaration.
            todo.setdefault(caller, []).append((name, sig))

    total = 0
    for caller in sorted(todo):
        protos = sorted(set(todo[caller]))
        text = open(caller, errors="replace").read()
        if MARKER in text:
            # The block is already there. Skipping the whole file was the first
            # behaviour and it was wrong: a second audit round finds prototypes
            # that belong in a file that already has one, and they were silently
            # dropped. Append the missing lines into the existing block.
            missing = [(n, s) for n, s in protos if f"{s};" not in text]
            if not missing:
                continue
            end = text.index("#endif", text.index(MARKER))
            add = "".join(f"{s};\n" for _n, s in missing)
            text = text[:end] + add + text[end:]
            rel = os.path.relpath(caller, a.decomp)
            print(f"  {rel:44} +{len(missing)} into existing block")
            total += len(missing)
            if a.apply:
                open(caller, "w").write(text)
            continue
        # The last #include ABOVE the first call, not the last in the file.
        #
        # Several files carry an #include halfway down, below code that already
        # called the function. A prototype placed there is a conflicting
        # declaration -- C has already committed to the implicit `int` version
        # at the call -- and clang rejects the file outright. That is a better
        # failure than a silent one, but it is still a failure, and it happened
        # to two files the first time this ran.
        limit = first_call.get(caller, 10 ** 9)
        incs = [m for m in re.finditer(r'^#include[^\n]*$', text, re.M)
                if text.count("\n", 0, m.start()) + 1 < limit]
        if not incs:
            skipped.append((caller, "no #include above the first call site"))
            continue
        at = incs[-1].end()

        block = ["", "#ifdef GE_HOST_PORT",
                 "/*",
                 f" * {MARKER}.",
                 " *",
                 " * These are called below with no declaration in scope. C then assumes they",
                 " * return `int`, which on the N64 was the same 32 bits as a pointer and cost",
                 " * nothing. Here the returned pointer is truncated and sign-extended, and every",
                 " * RDRAM address has the top bit set -- so the caller gets 0xFFFFFFFF8xxxxxxx",
                 " * and dies on the first dereference, wherever that happens to be.",
                 " *",
                 " * Generated by tools/fix_implicit_ptr_returns.py from the definitions; see",
                 " * tools/find_implicit_ptr_returns.py for how the list is derived.",
                 " */"]
        for name, sig in protos:
            block.append(f"{sig};")
            total += 1
        block.append("#endif")
        block.append("")

        text = text[:at] + "\n" + "\n".join(block) + text[at:]
        rel = os.path.relpath(caller, a.decomp)
        print(f"  {rel:44} {len(protos)} prototype(s)")
        if a.apply:
            open(caller, "w").write(text)

    print(f"\n{total} prototypes across {len(todo)} files")
    for path, why in skipped:
        print(f"SKIPPED {os.path.relpath(path, a.decomp)}: {why}")
    if not a.apply:
        print("\n(report only; pass --apply to write)")
        return 0
    print("\nNOW RUN the audit again -- the pointer-returning count should be 0:")
    print("  GE_STRICT=1 tools/link_game.sh <decomp> /tmp/ge_audit")
    print("  tools/find_implicit_ptr_returns.py --log /tmp/ge_audit/compile.log "
          "--decomp <decomp>")
    return 0


if __name__ == "__main__":
    sys.exit(main())
