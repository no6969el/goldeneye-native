/*
 * ge_fault.c -- name the fault instead of just taking it.
 *
 * WHY THIS EXISTS
 *
 * The port maps RDRAM at KSEG0 so the game's own N64 addresses can be
 * dereferenced directly. That works for the address VALUE; it does not survive
 * the game storing one in an `s32`. An RDRAM address is 0x80000000 or above, so
 * it is NEGATIVE as a 32-bit int, and widening it back to a pointer on a 64-bit
 * host sign-extends it:
 *
 *     0x80703 6d4  ->  s32  ->  0xFFFFFFFF807036D4
 *
 * That is not a rare accident. Half of all RDRAM addresses have the top bit
 * set, so it fails for half the heap and works for the rest -- which is exactly
 * the kind of bug that looks like data corruption or a logic error for a day
 * before anyone reads the pointer carefully.
 *
 * There are 103 sites in the decomp that cast an address through `(s32)` or
 * `(int)`, in 14 files, 72 of them in src/game/chraction.c. Each one that is
 * wrong produces a SIGSEGV at an address of a very recognisable shape. Left to
 * the default handler they all read "Segmentation fault" and cost a debugger
 * session each. Named, they cost one line each.
 *
 * WHAT THIS DELIBERATELY DOES NOT DO
 *
 * It does not repair the pointer and continue. That was tempting and it is the
 * wrong call: a handler that silently fixes up the access would hide the very
 * sites this is meant to enumerate, and would leave the shipped port depending
 * on a signal handler for correctness. It reports and dies.
 */
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "ge_fault.h"

/*
 * The shape of a sign-extended 32-bit address: the top 32 bits all set, and the
 * low half with bit 31 set. Anything else is an ordinary wild pointer and is
 * reported as such -- claiming this diagnosis for every fault would make it
 * worthless.
 */
#define GE_SEXT_MASK 0xFFFFFFFF00000000ull

static int isSignExtended(unsigned long long a)
{
    return (a & GE_SEXT_MASK) == GE_SEXT_MASK && (a & 0x80000000ull) != 0;
}

static void report(int sig, siginfo_t *info, void *ucontext)
{
    const unsigned long long addr = (unsigned long long)(uintptr_t)
        (info != 0 ? info->si_addr : 0);

    (void)ucontext;

    /* write(2) only: the fault may have come from anywhere, including inside
       malloc, and printf is not async-signal-safe. */
    static const char hdr[] = "\n[ge-fault] ";
    char line[1024];
    int n;

    if (isSignExtended(addr)) {
        n = snprintf(line, sizeof(line),
            "%sSIGSEGV at 0x%016llX\n"
            "  This is an RDRAM address that was truncated to s32 and\n"
            "  sign-extended back: 0x%08X became 0x%016llX.\n"
            "  The real address is 0x%08X, which IS mapped.\n"
            "\n"
            "  Cause: the game stored a pointer in an s32 or cast one through\n"
            "  (s32)/(int). On the N64 that is lossless; here the top bit makes\n"
            "  it negative. Find the site and route it through uintptr_t/u32,\n"
            "  guarded by GE_HOST_PORT.\n"
            "\n"
            "  Run under gdb to get the line:\n"
            "    gdb -q -batch -ex run -ex \"bt 12\" ./ge007 --rom <rom>\n"
            "  See patches/HOST-PORT-PATCHES.md 18 for the family.\n",
            hdr, addr, (unsigned)(addr & 0xFFFFFFFFu), addr,
            (unsigned)(addr & 0xFFFFFFFFu));
    } else if (addr < 0x1000ull) {
        n = snprintf(line, sizeof(line),
            "%sSIGSEGV at 0x%016llX -- a null or near-null dereference,\n"
            "  not the sign-extension family.\n", hdr, addr);
    } else {
        n = snprintf(line, sizeof(line),
            "%sSIGSEGV at 0x%016llX -- not a sign-extended RDRAM address.\n"
            "  RDRAM is 0x80000000..0x80800000; the TLB windows are at\n"
            "  0x70000000 and 0x7F000000; the image is at 0x20000000.\n",
            hdr, addr, addr);
    }

    /* snprintf returns the length it WOULD have written. Writing that many
       bytes reads past the buffer -- which this handler did on its first run,
       and printed the fault report followed by a page of stack garbage. */
    if (n > 0) {
        size_t len = (size_t)n;
        if (len >= sizeof(line)) {
            len = sizeof(line) - 1u;
        }
        {
            ssize_t ignored = write(2, line, len);
            (void)ignored;
        }
    }

    /* Re-raise with the default handler so the exit status and any core dump
       are what a crash is supposed to produce. */
    signal(sig, SIG_DFL);
    raise(sig);
}

void geFaultInstall(void)
{
#ifndef _WIN32
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = report;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, 0);
    sigaction(SIGBUS, &sa, 0);
#endif
}

/*
 * The game calls assert() -- src/sched.c:566 and a dozen sites in bg.c. On the
 * N64 it resolved to libultra's __assert; GCC folded the ones it could prove and
 * left no reference, clang does not, so it appears at link time under clang and
 * not under GCC.
 *
 * Deliberately NOT defined away to nothing. These are the game's own invariants,
 * and on a port that is still finding layout and endianness bugs they are worth
 * hearing about. It reports and aborts rather than continuing on a broken one.
 */
void __assert(const char *file, int line, const char *expr);
void assert(int cond);

void assert(int cond)
{
    if (!cond) {
        static const char m[] = "\n[ge-assert] a game invariant failed. "
                                "Run under gdb for the site.\n";
        ssize_t ignored = write(2, m, sizeof(m) - 1);
        (void)ignored;
        abort();
    }
}
