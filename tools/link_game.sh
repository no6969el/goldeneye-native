#!/usr/bin/env bash
# link_game.sh — try to produce an actual executable.
#
# tools/link_census.sh answers "does every symbol have a definition somewhere?".
# This answers the harder question: does it LINK? Those differ — duplicate
# definitions, ABI mismatches and missing entry points only appear here.
#
# Usage: tools/link_game.sh <decomp-root> [outdir]
set -u
REPO="${1:?usage: link_game.sh <decomp-root> [outdir]}"
OUT="${2:-/tmp/ge_game}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
rm -rf "$OUT"; mkdir -p "$OUT/obj"

# clang, not gcc: cartridge-data structs use __uptr __ptr32 (GE_N64PTR) so
# their host layout matches the N64's, and GCC does not implement it. Without
# it every one of those structs is silently the wrong size --
# hostcompat/ge_n64ptr.h has the measurement. MSVC also supports it, which is
# where the Windows build goes.
CC="${CC:-clang}"
CXX="${CXX:-clang++}"
LENIENT="-ferror-limit=0 -Wno-int-conversion -Wno-implicit-function-declaration -Wno-incompatible-pointer-types"
INC="-I $HERE/hostcompat -I $HERE/src/host -I $REPO -I $REPO/include -I $REPO/include/PR -I $REPO/src -I $REPO/src/game -I $REPO/src/inflate -I $REPO/src/libultra -I $REPO/src/libultra/gu -I $REPO/src/libultra/audio -I $REPO/src/libultrare"
DEF="-D_LANGUAGE_C -DGE_HOST_PORT -DVERSION_US -DLANG_US -DREFRESH_NTSC -DLEFTOVERDEBUG -DLEFTOVERSPECTRUM -DBUGFIX_R0 -DBYTEMATCH"
# -fms-extensions is load-bearing: it is what makes `inherits` (a bare
# `struct X;` inside a struct body) an anonymous member rather than a forward
# declaration. Dropping it here while the census had it made 16 files fail and
# produced 350 "undefined reference" lines that looked like missing code.
# -O2 is not just for speed here, it is LOAD-BEARING.
#
# clang 18's x86 backend crashes at -O0 on the pinned cartridge structs --
# "Cannot emit physreg copy instruction", in the post-RA pseudo expansion pass,
# on four of the game's larger functions (chr.c, front.c, model.c, propobj.c).
# -O1 and above do not hit it. Measured across -O0/-O1/-O2/-Os.
#
# So the optimised build is the one that works, which is a happy accident: it is
# also the one worth shipping. If a future clang fixes the bug this stays anyway.
GE_OPT="${GE_OPT:--O2}"
# GE_SAN=1 builds with AddressSanitizer. The port spends a lot of its time
# chasing memory corruption in code that was correct on a 32-bit big-endian
# machine, and a smash that lands in a return address costs a debugging session
# where ASan names the line for free.
GE_SAN_FLAGS=""
if [ "${GE_SAN:-0}" = "1" ]; then
    GE_SAN_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
fi
# -w silences everything, which is right for a 1997 codebase full of warnings
# nobody is going to fix -- with ONE exception.
#
# An implicitly declared function is assumed to return `int`. On the N64 that
# was harmless: int and every pointer were both 32 bits. Here the returned
# pointer is truncated to 32 bits and then SIGN-EXTENDED, so any RDRAM address
# (top bit set) comes back as 0xFFFFFFFF8xxxxxxx. front.c calls
# retrieve_display_rareware_logo() with no prototype in scope and the display
# list pointer it returns died exactly that way, 540 frames in.
#
# There is no way to find these by testing -- each one only fires when that
# code path runs -- so the compiler has to find them. GE_STRICT=1 turns the
# whole class into errors for an audit pass.
GE_IMPLICIT=""
if [ "${GE_STRICT:-0}" = "1" ]; then
    GE_IMPLICIT="-Werror=implicit-function-declaration"
fi
# The OTHER half of the same family, and the bigger half: an integer used where
# a pointer is expected. `s32 virtualaddress;` holding an RDRAM address is
# lossless on the N64 and sign-extends here. GE_INTAUDIT=1 lists every site.
GE_INTCONV=""
if [ "${GE_INTAUDIT:-0}" = "1" ]; then
    # -Wint-to-pointer-cast catches the EXPLICIT casts too: `(T *)someS32`,
    # where the cast silences -Wint-conversion but the value still sign-extends.
    GE_INTCONV="-Werror=int-conversion -Werror=int-to-pointer-cast -Werror=int-to-void-pointer-cast"
fi
# -w has to come OFF for the cast audit. `-Werror=int-to-pointer-cast` looks
# like it should survive it -- it says error, not warning -- but clang treats
# -w as "this diagnostic is not emitted at all", and a diagnostic that is not
# emitted cannot be promoted. (implicit-function-declaration and int-conversion
# are unaffected because they are errors by default in C99 and later, so -w has
# nothing to suppress.) Measured: the cast audit reported zero sites with -w in
# place, and 45 without it.
GE_QUIET="-w"
if [ -n "$GE_INTCONV" ]; then
    GE_QUIET=""
fi
CFLAGS="-c -g $GE_OPT $GE_SAN_FLAGS -std=gnu99 $GE_QUIET -fms-extensions -fno-strict-aliasing $LENIENT $GE_IMPLICIT $GE_INTCONV"

n=0
compile_c() {
    for f in "$@"; do
        [ -e "$f" ] || continue
        o="$OUT/obj/$(echo "$f" | md5sum | cut -c1-8)_$(basename "${f%.c}").o"
        $CC $CFLAGS $INC $DEF "$f" -o "$o" 2>>"$OUT/compile.log" || echo "FAILED $f" >> "$OUT/compile.log"
        n=$((n+1))
    done
}

: > "$OUT/compile.log"
# src/motor.c is the decomp's own osMotor* implementation, talking to the SI
# bus directly. src/ultra/os_io.cpp provides those entry points for the host, so
# compiling both gives "multiple definition of osMotorInit". The shim wins: it
# is the one that knows there is no PIF.
SYS_C=$(ls "$REPO"/src/*.c | grep -v '/motor\.c$')
compile_c "$REPO"/src/game/*.c $SYS_C "$REPO"/src/inflate/*.c \
          "$REPO"/src/libultra/gu/*.c "$REPO"/src/libultra/audio/*.c \
          "$REPO"/src/libultra/libc/*.c "$REPO"/src/libultrare/audio/*.c \
          "$REPO"/src/libultrare/libc/*.c "$REPO"/src/libultrare/io/vitbl.c \
          "$REPO"/src/libultrare/io/vimodepallan1.c

# The port's own C. hostcompat first; os_hw/os_sp additionally need <PR/os.h>.
PORT_INC="-I $HERE/hostcompat -I $HERE/src/host -I $HERE/src/ultra -I $HERE/src/host"
for f in "$HERE"/src/ultra/segments.c "$HERE"/src/ultra/random.c \
         "$HERE"/src/ultra/os_tlb.c "$HERE"/src/ultra/ucode_blobs.c \
         "$HERE"/src/host/sha1.c "$HERE"/src/host/ge_fault.c "$HERE"/src/host/ge_swap.c "$HERE"/src/host/ge_swap_util.c "$HERE"/src/host/ge_swap_prop_unions.c "$HERE"/src/host/ge_expand.c "$HERE"/src/host/ge_assets.c \
         "$HERE"/src/host/ge_assets_load.c; do
    $CC $CFLAGS $PORT_INC "$f" -o "$OUT/obj/port_$(basename "${f%.c}").o" \
        2>>"$OUT/compile.log" || echo "FAILED $f" >> "$OUT/compile.log"
done
for f in "$HERE"/src/ultra/os_hw.c "$HERE"/src/ultra/os_sp.c; do
    $CC $CFLAGS $PORT_INC $INC $DEF "$f" -o "$OUT/obj/port_$(basename "${f%.c}").o" \
        2>>"$OUT/compile.log" || echo "FAILED $f" >> "$OUT/compile.log"
done

# C++ side: the shim proper and the host entry point. No hostcompat here — it
# shadows headers the C++ standard library needs.
for f in "$HERE"/src/ultra/*.cpp "$HERE"/src/ultra/audio/*.cpp "$HERE"/src/gbi/*.cpp "$HERE"/src/rhi/vertex_pipeline.cpp "$HERE"/src/host/main.cpp "$HERE"/src/host/ge_gfx_probe.cpp; do
    [ -e "$f" ] || continue
    $CXX -c -g $GE_OPT $GE_SAN_FLAGS -std=c++17 -w -I "$HERE/src" -I "$HERE/src/ultra" -I "$HERE/src/host" \
        "$f" -o "$OUT/obj/cxx_$(basename "${f%.cpp}").o" \
        2>>"$OUT/compile.log" || echo "FAILED $f" >> "$OUT/compile.log"
done

echo "objects: $(ls "$OUT"/obj/*.o 2>/dev/null | wc -l)"
if grep -q FAILED "$OUT/compile.log"; then
    echo; echo "compile failures:"; grep FAILED "$OUT/compile.log" | sed 's/^/  /' | head -20
fi

echo
echo "linking..."
# -no-pie: the game takes the address of its own statics and stores them in
# display lists, and the port maps memory at fixed addresses. A fixed load
# address also makes the diagnostic output directly comparable with `nm`, and
# clears the DT_TEXTREL warnings the game's own relocations produce in a PIE.
#
# -Ttext-segment=0x20000000: LOAD-BEARING, not tidiness.
#
# The game hands the PI the ADDRESS of an asset symbol as a cartridge offset,
# so the port translates host addresses back to ROM offsets. At the default
# load address the host image sits at 0x400000..0x840000 and the ROM offset
# space is 0..0xC00000 -- the same numbers. 548 of 821 genuine ROM offsets were
# captured by that translation and rewritten to a different offset, and the
# game's decompressor spun forever on the result. Relocating the image above
# the ROM makes the two spaces disjoint, so the question cannot be asked.
#
# It must stay under 4 GB: osPiStartDma takes a u32 devAddr, so a host pointer
# above 4 GB is truncated before the translator ever sees it. 0x20000000 clears
# the ROM (12 MB), the PI bus window at 0x10000000, RDRAM at 0x80000000 and the
# TLB windows at 0x70000000/0x7F000000.
#
# On Windows the equivalent is /BASE:0x20000000 /FIXED (MSVC) or
# -Wl,--image-base=0x20000000 (lld). Note MSVC's x64 default of 0x140000000 is
# ABOVE 4 GB and would break the truncation, so it has to be set explicitly
# there too -- for a different reason than here.
#
# src/host/ge_assets_load.c re-checks this at startup and refuses to run if it
# was dropped, because dropping it does not fail: it loads the wrong data.
GE_IMAGE_BASE="${GE_IMAGE_BASE:-0x20000000}"
$CXX -no-pie $GE_SAN_FLAGS -Wl,-Ttext-segment="$GE_IMAGE_BASE" \
     -o "$OUT/ge007" "$OUT"/obj/*.o -lm 2>"$OUT/link.log"
rc=$?
if [ $rc -eq 0 ]; then
    echo "LINKED: $OUT/ge007  ($(stat -c%s "$OUT/ge007") bytes)"
else
    echo "link failed. Distinct problems:"
    grep -oE "undefined reference to \`[^']+'" "$OUT/link.log" | sort -u | head -30
    echo "  ($(grep -cE 'undefined reference' "$OUT/link.log") lines, $(grep -oE "undefined reference to \`[^']+'" "$OUT/link.log" | sort -u | wc -l) distinct)"
    grep -vE "undefined reference|^/usr/bin/ld: [^ ]+\.o: in function" "$OUT/link.log" | head -10
fi
exit $rc
