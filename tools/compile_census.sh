#!/usr/bin/env bash
# compile_census.sh — how much of the game's C compiles on a host target?
#
# This is the measurement that reordered the project's priorities. Until it was
# run, every part of the port was validated against synthetic tests or extracted
# data and the game's own C had never been put in front of a compiler.
#
# Syntax-only: no objects, no link. The point is to categorise what breaks.
#
# Usage: tools/compile_census.sh <decomp-root> [errors.txt]
set -u
REPO="${1:?usage: compile_census.sh <decomp-root> [errors.txt]}"
ERRS="${2:-/tmp/ge_census_errors.txt}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"

# hostcompat FIRST: it shadows the decomp's N64-only stubs (see hostcompat/).
INC="-I $HERE/hostcompat -I $HERE/src/host -I $REPO -I $REPO/include -I $REPO/include/PR -I $REPO/src -I $REPO/src/game"

# Mirrors the decomp Makefile's US build (line 74) MINUS -DTARGET_N64, which
# would select 32-bit N64 typedefs for size_t and friends on a 64-bit host.
# _LANGUAGE_C is PREDEFINED by IDO; GCC does not define it. Without it,
# PR/ultratypes.h compiles to nothing while still setting its include guard, so
# any file that reaches <PR/os.h> before <ultra64.h> never gets s32/u32 at all
# (src/boss.c: 4,464 errors, all of them "unknown type name").
DEF="-D_LANGUAGE_C -DGE_HOST_PORT -DVERSION_US -DLANG_US -DREFRESH_NTSC -DLEFTOVERDEBUG -DLEFTOVERSPECTRUM -DBUGFIX_R0 -DBYTEMATCH"

# Compiler is overridable: the port targets Windows too, where clang-cl or
# MinGW clang is the likely toolchain, and clang is far stricter than GCC about
# exactly the IDO-lenient patterns this codebase is full of.
CC="${CC:-cc}"

# Clang 16+ made these hard ERRORS, and -w does not suppress errors. They are
# precisely the two leniencies the decomp's own Makefile switches off in IDO
# (warnings 709 "incompatible pointer type assignment" and 712 "illegal
# combination of pointer and integer"), so the codebase is full of them by
# design. Downgrade rather than "fix" thousands of intentional sites.
LENIENT="-Wno-int-conversion -Wno-implicit-function-declaration -Wno-incompatible-pointer-types"

ok=0; fail=0; : > "$ERRS"
for f in "$REPO"/src/game/*.c; do
    if $CC -fsyntax-only -std=gnu99 -fms-extensions -w $LENIENT $INC $DEF "$f" 2>>"$ERRS"; then
        ok=$((ok+1))
    else
        fail=$((fail+1)); echo "$f" >> "$ERRS.failed"
    fi
done

echo "compiled OK: $ok / $((ok+fail))"
echo
echo "errors by kind:"
grep -oE "error: .*" "$ERRS" | sed "s/'[^']*'/'X'/g" | sort | uniq -c | sort -rn | head -12
