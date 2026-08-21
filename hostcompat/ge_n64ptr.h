/*
 * ge_n64ptr.h -- a pointer that is 4 bytes on a 64-bit host.
 *
 * THE PROBLEM THIS SOLVES
 *
 * 89 of the 209 structs in src/bondtypes.h have a different layout on the host
 * than on the N64, 88 of them because they contain a pointer
 * (tools/check_struct_layout.py). 34 of those describe data read straight from
 * the cartridge, so their layout is not ours to choose -- the bytes arrive in
 * the N64's. `Vertex` is 16 bytes there and was 24 here, for every vertex in
 * the game.
 *
 * The obvious fix is to redeclare each pointer member as `u32` and convert at
 * every use site. That works -- it is what ModelAnimation had first -- and it
 * costs a `GE_PTR()` at hundreds of accessors, each one a chance to miss.
 *
 * THE BETTER ONE
 *
 * `__ptr32` stores a pointer in 4 bytes on a 64-bit target, and `__uptr` makes
 * the widening ZERO-extend. Both are Microsoft extensions, supported by MSVC on
 * x64 and by clang with -fms-extensions. The struct then matches the cartridge
 * byte for byte, and the member is still a POINTER: every use site compiles and
 * works unchanged.
 *
 * `__uptr` is load-bearing, not decoration. RDRAM lives at 0x80000000 and up, so
 * the top bit is set; the default widening sign-extends and produces
 * 0xFFFFFFFF8xxxxxxx -- the exact fault ge_fault.c exists to name. Measured:
 *
 *     struct { int a; unsigned char * __uptr __ptr32 p; }   sizeof 8
 *     p = 0x800036D4  ->  stored as 0x800036D4, reads back 0x800036d4  OK
 *     the same value through (s32)  ->  0xFFFFFFFF800036D4   SIGSEGV
 *
 * WHAT IT COSTS
 *
 * GCC does not implement `__ptr32`. The port therefore builds with clang or
 * MSVC, which is where it was already heading: the target is Windows, and
 * tools/link_census.sh has run under both compilers since P3 precisely because
 * clang catches what GCC accepts quietly.
 *
 * WHERE TO USE IT
 *
 * Only on structs whose instances are cartridge data. A struct the port itself
 * allocates has no layout obligation and should keep ordinary pointers -- 4-byte
 * pointers there would cap it to the low 4 GB for no benefit.
 */
#ifndef GE_N64PTR_H
#define GE_N64PTR_H

#if defined(GE_HOST_PORT) && !defined(TARGET_N64)
/* Only a 64-bit host needs this. On a 32-bit one a pointer is already 4 bytes,
   which also keeps the -m32 layout oracle in tools/check_struct_layout.py
   honest -- it has to measure the same declarations the N64 sees. */
#  if defined(__LP64__) || defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
#    if defined(_MSC_VER) || defined(__clang__)
#      define GE_N64PTR __uptr __ptr32
#    else
#    error "ge_n64ptr.h: this port needs clang or MSVC -- GCC has no __ptr32, \
and without it every cartridge-data struct is silently the wrong size. See \
PRIORITIES.md P3h."
#    endif
#  else
#    define GE_N64PTR             /* 32-bit host: already 4 bytes */
#  endif
#else
#  define GE_N64PTR                 /* nothing: a pointer is already 4 bytes */
#endif

#endif /* GE_N64PTR_H */
