#!/usr/bin/env python3
"""
find_implicit_ptr_returns.py -- the sign-extension bugs the compiler can find.

WHAT THIS IS FOR

A function called without a prototype in scope is assumed to return `int`. On
the N64 that was free: `int` and every pointer were both 32 bits, so the value
survived. On a 64-bit host the returned pointer is truncated to 32 bits and then
SIGN-EXTENDED on the way back into a pointer variable -- and every RDRAM address
has the top bit set, so it comes back as 0xFFFFFFFF8xxxxxxx.

That is the same failure family as patches/HOST-PORT-PATCHES.md 18, but it has a
property that makes it much worse: there is no cast to grep for. The call site
looks completely ordinary.

    front.c:  return retrieve_display_rareware_logo(DL);   /* no prototype */
    title.c:  Gfx *retrieve_display_rareware_logo(Gfx *gdl)

That one cost the port a crash 540 frames in, on the Rare logo, and nothing
about the crash pointed at the missing declaration.

WHY NOT JUST FIX ALL 275

Most undeclared functions return `int` or `void` and are harmless -- the
truncation is a no-op. Turning the whole class into errors would mean editing
two hundred files of a MATCHING decompilation for no behavioural gain, and every
edit is a chance to break the token stream. This narrows the list to the ones
that can actually corrupt a value: those whose definition returns a pointer.

HOW IT DECIDES

Two independent sources, deliberately:

  - the CALL sites come from clang, via a strict compile. Not from a regex over
    the source, because "is this name declared here" depends on the include
    graph and no regex knows that.
  - the RETURN TYPE comes from the definition in the decomp.

A name is reported only when both agree. The failure mode of the definition
regex is that it misses a definition, and a miss produces silence rather than a
false alarm -- so the count here is a floor, not a total.

Usage:
    GE_STRICT=1 tools/link_game.sh <decomp> /tmp/ge_audit   # produces compile.log
    tools/find_implicit_ptr_returns.py --log /tmp/ge_audit/compile.log \
                                       --decomp <decomp>
"""
import argparse
import os
import re
import sys

CALL = re.compile(
    r"^(?P<file>[^:]+):(?P<line>\d+):\d+: error: call to undeclared function "
    r"'(?P<name>[A-Za-z_]\w*)'")

# `Gfx *name(`, `struct Foo **name(`, `u8 * GE_N64PTR name(`. The `\*` before
# the name is the whole test: a pointer return is the only thing that can be
# corrupted by the implicit-int rule.
def def_regex(name):
    return re.compile(
        r'^[ \t]*(?:static[ \t]+|extern[ \t]+|inline[ \t]+)*'
        r'(?P<ret>[A-Za-z_]\w*(?:[ \t]+\w+)*)[ \t]*'
        r'(?P<stars>\*+)[ \t]*(?:GE_N64PTR[ \t]+)?'
        + re.escape(name) + r'[ \t]*\(', re.M)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", required=True)
    ap.add_argument("--decomp", required=True)
    a = ap.parse_args()

    if not os.path.exists(a.log):
        print(f"no such log: {a.log}\n"
              f"Run: GE_STRICT=1 tools/link_game.sh <decomp> <outdir>",
              file=sys.stderr)
        return 2

    # name -> set of "file:line"
    calls = {}
    for raw in open(a.log, errors="replace"):
        m = CALL.match(raw.strip())
        if not m:
            continue
        calls.setdefault(m.group("name"), set()).add(
            f"{os.path.relpath(m.group('file'), a.decomp)}:{m.group('line')}")

    if not calls:
        print("no undeclared calls in the log.\n"
              "If that is a surprise, check that GE_STRICT=1 was set and that\n"
              "$LENIENT's -Wno-implicit-function-declaration is not winning:\n"
              "the LAST -W flag on the command line is the one that applies.")
        return 0

    # Index every source file once. 200 files x 275 names of re-reading is
    # slower than this by two orders of magnitude.
    sources = []
    for root, _dirs, files in os.walk(os.path.join(a.decomp, "src")):
        for f in files:
            if f.endswith((".c", ".h")):
                p = os.path.join(root, f)
                try:
                    sources.append((p, open(p, errors="replace").read()))
                except OSError:
                    pass

    pointer_returning = {}
    for name in sorted(calls):
        rx = def_regex(name)
        for path, text in sources:
            m = rx.search(text)
            if m:
                ret = f"{m.group('ret')} {m.group('stars')}"
                pointer_returning[name] = (ret, os.path.relpath(path, a.decomp))
                break

    print(f"{len(calls)} functions called without a prototype.")
    print(f"{len(pointer_returning)} of them RETURN A POINTER -- these truncate "
          f"and sign-extend.\n")

    for name, (ret, where) in sorted(pointer_returning.items()):
        sites = sorted(calls[name])
        print(f"  {name}")
        print(f"      returns {ret.strip():28} defined in {where}")
        for s in sites:
            print(f"      called at {s}")
    if not pointer_returning:
        print("  (none -- every undeclared call returns an integer or void, "
              "which the implicit-int rule leaves intact)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
