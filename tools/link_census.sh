#!/usr/bin/env bash
# link_census.sh — compile the game to real objects and measure the link gap.
#
# -fsyntax-only proves the source parses. Only codegen and the symbol table
# prove there is an implementation behind it, which is why this is a separate
# measurement from tools/compile_census.sh.
#
# src/libultra and src/libultrare are deliberately NOT compiled: the whole point
# of src/ultra/ is to replace them on a host.
#
# Usage: tools/link_census.sh <decomp-root> [objdir]
set -u
REPO="${1:?usage: link_census.sh <decomp-root> [objdir]}"
OBJ="${2:-/tmp/ge_link_obj}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
rm -rf "$OBJ"; mkdir -p "$OBJ/game" "$OBJ/sys" "$OBJ/shim" "$OBJ/lu"

INC="-I $HERE/hostcompat -I $HERE/src/host -I $REPO -I $REPO/include -I $REPO/include/PR -I $REPO/src -I $REPO/src/game -I $REPO/src/inflate -I $REPO/src/libultra -I $REPO/src/libultra/gu -I $REPO/src/libultra/audio -I $REPO/src/libultrare"
# _LANGUAGE_C is PREDEFINED by IDO; GCC does not define it. Without it,
# PR/ultratypes.h compiles to nothing while still setting its include guard, so
# any file that reaches <PR/os.h> before <ultra64.h> never gets s32/u32 at all
# (src/boss.c: 4,464 errors, all of them "unknown type name").
DEF="-D_LANGUAGE_C -DGE_HOST_PORT -DVERSION_US -DLANG_US -DREFRESH_NTSC -DLEFTOVERDEBUG -DLEFTOVERSPECTRUM -DBUGFIX_R0 -DBYTEMATCH"
CC="${CC:-cc}"
# See compile_census.sh for why these are needed under clang.
LENIENT="-Wno-int-conversion -Wno-implicit-function-declaration -Wno-incompatible-pointer-types"
CFLAGS="-c -std=gnu99 -fms-extensions -w -fno-strict-aliasing $LENIENT"

build_group() {  # <label> <destdir> <files...>
    local label="$1" dest="$2"; shift 2
    local ok=0 fail=0
    for f in "$@"; do
        [ -e "$f" ] || continue
        if $CC $CFLAGS $INC $DEF "$f" -o "$dest/$(basename "${f%.c}").o" 2>>"$OBJ/compile-errors.txt"; then
            ok=$((ok+1))
        else
            fail=$((fail+1)); echo "FAILED: $f" >> "$OBJ/compile-errors.txt"
        fi
    done
    printf '%-22s %3d / %d objects\n' "$label" "$ok" "$((ok+fail))"
}

: > "$OBJ/compile-errors.txt"
build_group "src/game/"  "$OBJ/game" "$REPO"/src/game/*.c
build_group "src/ (system)" "$OBJ/sys" "$REPO"/src/*.c "$REPO"/src/inflate/*.c

# libultra's SOFTWARE-ONLY parts compile for a host unchanged, and they are the
# game's real implementations rather than reimplementations of them. gu/ is pure
# matrix math; audio/ is the libaudio sequencer and bank parser, which runs on
# the CPU (only the synthesis kernels run on the RSP, and those are the ACMD
# interpreter's job); libc/ is string and printf.
#
# io/ and os/ are deliberately NOT here: those are the hardware ones -- PI, SI,
# AI, DP, VI, SP, threads, TLB, caches -- and replacing them is the entire point
# of src/ultra/.
# segments.c is built with ONLY the includes it needs. Putting $HERE/src/ultra
# on the global include path shadows headers the decomp and libultra expect and
# drags C++ headers into C translation units (9 files failed with "cstdint: No
# such file" the first time).
# The port's own C: segments.c needs only hostcompat, but os_hw.c and os_sp.c
# implement libultra entry points and so must see the decomp's <PR/os.h> to get
# the exact signatures. hostcompat comes FIRST on both -- it shadows the
# decomp's IRIX stubs for stddef.h and stdarg.h.
PORT_INC="-I $HERE/hostcompat -I $HERE/src/host -I $HERE/src/ultra"
build_group "port (C)" "$OBJ/shim" "$HERE"/src/ultra/segments.c \
    "$HERE"/src/ultra/random.c "$HERE"/src/ultra/os_tlb.c \
    "$HERE"/src/ultra/ucode_blobs.c
for f in "$HERE"/src/ultra/os_hw.c "$HERE"/src/ultra/os_sp.c; do
    $CC $CFLAGS $PORT_INC $INC $DEF "$f" -o "$OBJ/shim/$(basename "${f%.c}").o" \
        2>>"$OBJ/compile-errors.txt" || echo "FAILED: $f" >> "$OBJ/compile-errors.txt"
done

build_group "libultra (software)" "$OBJ/lu" \
    "$REPO"/src/libultra/gu/*.c "$REPO"/src/libultra/audio/*.c \
    "$REPO"/src/libultra/libc/*.c "$REPO"/src/libultrare/audio/*.c \
    "$REPO"/src/libultrare/libc/*.c \
    "$REPO"/src/libultrare/io/vitbl.c "$REPO"/src/libultrare/io/vimodepallan1.c

for f in "$HERE"/src/ultra/*.cpp "$HERE"/src/ultra/audio/*.cpp; do
    [ -e "$f" ] || continue
    c++ -c -std=c++17 -w -I "$HERE/src" -I "$HERE/include" -I "$HERE" \
        "$f" -o "$OBJ/shim/$(basename "${f%.cpp}").o" 2>>"$OBJ/compile-errors.txt"
done
printf '%-22s %3d objects\n' "src/ultra/ (shim)" "$(ls "$OBJ"/shim/*.o 2>/dev/null | wc -l)"

if grep -q FAILED "$OBJ/compile-errors.txt"; then
    echo; echo "files that failed to compile:"; grep FAILED "$OBJ/compile-errors.txt"
    echo; echo "errors by kind:"
    grep -oE "error: .*" "$OBJ/compile-errors.txt" | sed "s/'[^']*'/'X'/g" | sort | uniq -c | sort -rn | head -12
fi

nm -u             "$OBJ"/game/*.o "$OBJ"/sys/*.o "$OBJ"/lu/*.o 2>/dev/null | awk '{print $2}' | sort -u > "$OBJ/undef.txt"
nm --defined-only "$OBJ"/game/*.o "$OBJ"/sys/*.o "$OBJ"/lu/*.o "$OBJ"/shim/*.o 2>/dev/null | awk '{print $3}' | sort -u > "$OBJ/def.txt"
comm -23 "$OBJ/undef.txt" "$OBJ/def.txt" > "$OBJ/missing.txt"

# Categorise.
#
# The raw count is misleading on its own: most unresolved symbols are ASSET DATA
# (character models, animations, props, global display lists) that the decomp's
# own pipeline emits into object files from an extracted ROM. Those are not port
# work -- they are data that has to be on the link line. Separating them is what
# makes the remainder actionable.
#
# Ground truth beats pattern-matching: if the decomp has been built, ask its
# asset objects what they define rather than guessing from symbol names.
ASSET_SYMS="$OBJ/asset-syms.txt"
: > "$ASSET_SYMS"
NM_MIPS="$(command -v mips-linux-gnu-nm || command -v mips64-linux-gnuabi64-nm || true)"
ASSET_OBJS=$(find "$REPO/build" -name '*.o' -path '*asset*' 2>/dev/null | head -20000)
if [ -n "$NM_MIPS" ] && [ -n "$ASSET_OBJS" ]; then
    $NM_MIPS --defined-only $ASSET_OBJS 2>/dev/null | awk '{print $3}' | sort -u > "$ASSET_SYMS"
    echo "asset symbols known from $REPO/build: $(wc -l < "$ASSET_SYMS")"
else
    echo "NOTE: $REPO has no build/ (or no MIPS nm) -- asset symbols cannot be"
    echo "      identified exactly and will be counted under 'everything else'."
fi

comm -23 "$OBJ/missing.txt" "$ASSET_SYMS" > "$OBJ/gap.txt"
n_asset=$(( $(wc -l < "$OBJ/missing.txt") - $(wc -l < "$OBJ/gap.txt") ))

grep -E 'Segment(Start|End|RomStart|RomEnd)$' "$OBJ/gap.txt" > "$OBJ/seg.txt" || true
grep -E '^al[A-Z]'  "$OBJ/gap.txt" > "$OBJ/al.txt" || true
grep -E '^gu[A-Z]'  "$OBJ/gap.txt" > "$OBJ/gu.txt" || true
grep -E '^_?_?os'   "$OBJ/gap.txt" > "$OBJ/os.txt" || true
# Symbols the host toolchain resolves at link time. They show up in `nm -u`
# because this census never actually links, so counting them as port work
# would overstate what is left.
TOOLCHAIN='^(mem(cpy|set|cmp|move)|str(len|cmp|cpy|cat|ncpy|tol)|sprintf|printf|sqrtf?|fabsf?|_GLOBAL_OFFSET_TABLE_|__stack_chk_fail)$'
grep -E  "$TOOLCHAIN" "$OBJ/gap.txt" > "$OBJ/toolchain.txt" || true
grep -vE "$TOOLCHAIN|Segment(Start|End|RomStart|RomEnd)$|^al[A-Z]|^gu[A-Z]|^_?_?os" \
    "$OBJ/gap.txt" > "$OBJ/other.txt" || true

echo
echo "unresolved: $(wc -l < "$OBJ/missing.txt")"
printf '  %-42s %4d\n' "asset data (built from the user's ROM)" "$n_asset"
echo "  ---- actual port work: $(wc -l < "$OBJ/gap.txt") ----"
printf '  %-42s %4d\n' "linker segment symbols"        "$(wc -l < "$OBJ/seg.txt")"
printf '  %-42s %4d\n' "libaudio sequencer (al*)"      "$(wc -l < "$OBJ/al.txt")"
printf '  %-42s %4d\n' "libultra (os*/__os*)"          "$(wc -l < "$OBJ/os.txt")"
printf '  %-42s %4d\n' "gu* matrix helpers"            "$(wc -l < "$OBJ/gu.txt")"
printf '  %-42s %4d\n' "everything else"               "$(wc -l < "$OBJ/other.txt")"
printf '  %-42s %4d\n' "(host libc/compiler, resolved at link)" "$(wc -l < "$OBJ/toolchain.txt")"
echo
echo "Full lists: $OBJ/{seg,al,os,gu,other}.txt"
