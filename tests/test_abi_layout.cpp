/*
 * test_abi_layout.cpp — do the game and the shim agree on struct layout?
 *
 * This test exists because they did not, and the way that failed was awful.
 *
 * The game allocates its own OSThreads as statics (`OSThread mainThread;` in
 * src/init.c) compiled against the decomp's <PR/os.h>. The shim had appended
 * three host-only pointers to its own OSThread. So the game's was 448 bytes,
 * the shim's was 472, and every osCreateThread() wrote 24 bytes past the end of
 * the caller's object, over whatever static followed it.
 *
 * The symptom was a segfault inside swapcontext() at the first context switch —
 * a completely different subsystem, with nothing pointing back at a struct
 * definition. It cost a debugging session to find, and it would have come back
 * the moment anyone added a field.
 *
 * So: every type the game and the shim BOTH see gets checked here. A mismatch
 * fails at build time with the two numbers side by side, instead of much later
 * as memory corruption.
 *
 * Note this compares against constants recorded from the decomp's own headers
 * rather than including both definitions in one file — they define the same
 * type names, so they cannot coexist in a translation unit. That is exactly why
 * the drift was invisible in the first place.
 */

#include <cstddef>
#include <cstdio>

#include "ultra/os.h"

static int failures = 0;

static void expect(const char *what, size_t got, size_t want)
{
    const bool ok = (got == want);
    std::printf("  %-46s %4zu (game: %4zu) %s\n", what, got, want,
                ok ? "ok" : "MISMATCH");
    if (!ok) ++failures;
}

int main()
{
    std::printf("test_abi_layout\n");

    /*
     * These come from tools/check_abi.sh, which compiles a probe against the
     * decomp's own headers and prints the lines below. They are MEASURED, not
     * written from memory -- the first draft of this test asserted
     * sizeof(OSMesgQueue) == 32 from memory, the real answer is 40, and the
     * "mismatch" it reported was the test being wrong rather than the shim.
     * Re-run the tool after touching src/ultra/os.h.
     */
    expect("sizeof(OSThread)", sizeof(OSThread), 448);
    expect("offsetof(OSThread, context)", offsetof(OSThread, context), 48);
    expect("sizeof(__OSThreadContext)", sizeof(__OSThreadContext), 400);
    expect("sizeof(OSMesgQueue)", sizeof(OSMesgQueue), 40);
    expect("sizeof(OSTimer)", sizeof(OSTimer), 48);
    /*
     * OSTask and OSViMode are NOT checked here: the shim does not define them
     * (they come from <PR/sptask.h> and <PR/os.h>, which only the game side
     * includes), so there is nothing on this side to compare. They are printed
     * by tools/check_abi.sh so the numbers are on record if the shim ever grows
     * its own definitions.
     */
    expect("sizeof(OSContPad)", sizeof(OSContPad), 6);

    /* The host state has to fit in the space the register context occupies. */
    expect("sizeof(GeThreadHost) fits in context",
           sizeof(GeThreadHost) <= sizeof(__OSThreadContext) ? 1u : 0u, 1u);

    if (failures == 0) std::printf("  all layouts agree\n");
    return failures != 0;
}
