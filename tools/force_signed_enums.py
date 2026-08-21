#!/usr/bin/env python3
"""
force_signed_enums.py -- make enums signed, the way IDO made them.

THE BUG THIS EXISTS FOR

C leaves an enum's underlying type implementation-defined. IDO chose `int`.
clang chooses `unsigned int` whenever every enumerator is non-negative. The game
depends on the IDO choice, in a way that is completely silent when it breaks:

    struct intro_char intro_char_table[] = {
        ...
        {0xFFFFFFFF, 0, 0, 0, 0, 0, 0}      /* enum BODIES body; */
    };

    if (intro_char_table[f].body < 0) { ... }    /* end of table */

With a signed enum, 0xFFFFFFFF reads back as -1 and the test fires. With an
unsigned one it reads back as 4294967295, `< 0` is never true, and the loop
walks off the end of the table into whatever follows. Measured: the cast intro
reached index 34 of a 20-entry table and called langGet() with a slot ID made of
neighbouring data.

There is no compiler flag for this -- -fshort-enums is about SIZE -- and no way
to see it in a diagnostic, because both readings are legal C.

WHAT IT DOES

Appends one negative enumerator, under `#ifdef GE_HOST_PORT`, to every enum that

  - has no negative enumerator already (those are signed anyway), and
  - has no enumerator that cannot fit in a signed int (adding a negative one
    there would make the enum ill-formed).

The added name is never used. Its only job is to force the underlying type. The
N64 token stream is unchanged, so `make` is unaffected.

APPLYING IT TO EVERYTHING DOES NOT WORK -- MEASURED

Running this over all of src/bondconstants.h forces 75 enums signed and the port
regresses: it dies at frame 2650 instead of 4960, deterministically, on a
0xFFFFFFFF dereference. At least one of those enums is compared or switched on in
a way that depends on the unsigned reading, and finding which one costs a bisect
nobody has run yet.

So this is a REPORT-ONLY instrument until that bisect happens. The two enums the
port actually needed -- HEADS and BODIES -- were each forced by hand, from an
observed crash, and are guarded in place in bondconstants.h. That is the bar:
evidence per enum, not a blanket edit to a matching decompilation.

WHAT IT CANNOT DECIDE

Whether a given enum actually NEEDS this. Deciding that means knowing every
place a value of the type is compared against zero or assigned a sentinel, which
is the whole codebase. Forcing all of them costs nothing at runtime -- the
representation is identical, only the compiler's inference changes -- and
removes the class rather than the instances.

Usage:
    force_signed_enums.py --header <path> [--apply]
"""
import argparse
import re
import sys

MARKER = "GE_FORCE_SIGNED"

NOTE = """#ifdef GE_HOST_PORT
        /* Forces the underlying type to int, as IDO chose. clang picks
           `unsigned int` when no enumerator is negative, and the game stores
           0xFFFFFFFF sentinels in enum fields and tests them with `< 0`.
           Unused; see tools/force_signed_enums.py. */
        , %s = -1
#endif
"""

ENUM = re.compile(r'typedef enum\s+(\w+)\s*\{', re.M)


def body_span(text, open_brace):
    depth, i = 0, open_brace
    while i < len(text):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--header", required=True)
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()

    text = open(a.header).read()
    out = []
    last = 0
    added, skipped = [], []

    for m in ENUM.finditer(text):
        name = m.group(1)
        ob = text.index("{", m.start())
        cb = body_span(text, ob)
        if cb < 0:
            continue
        body = text[ob + 1:cb]
        if MARKER in body:
            continue

        # Already signed? Any `= -N` enumerator settles it.
        if re.search(r'=\s*-\s*\d', body):
            skipped.append((name, "already has a negative enumerator"))
            continue

        # Any value that does not fit in a signed int makes the enum
        # necessarily unsigned; adding -1 would be a constraint violation.
        too_big = False
        for lit in re.finditer(r'=\s*(0[xX][0-9a-fA-F]+|\d+)', body):
            try:
                v = int(lit.group(1), 0)
            except ValueError:
                continue
            if v > 0x7FFFFFFF:
                too_big = True
                break
        if too_big:
            skipped.append((name, "has an enumerator above INT_MAX"))
            continue

        out.append(text[last:cb])
        out.append("\n" + NOTE % f"{name}_{MARKER}" + "    ")
        last = cb
        added.append(name)

    out.append(text[last:])

    print(f"{len(added)} enums forced signed, {len(skipped)} left alone")
    for n, why in skipped:
        print(f"  skip {n}: {why}")
    if not a.apply:
        print("\n(report only; pass --apply to write)")
        return 0
    open(a.header, "w").write("".join(out))
    print(f"\nwrote {a.header}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
