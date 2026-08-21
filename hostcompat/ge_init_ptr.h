/*
 * ge_init_ptr.h -- assign a pointer at startup that the N64 assigned at link time.
 *
 * WHY THIS IS NEEDED AT ALL
 *
 * The game has statics whose pointer members are initialised with the address of
 * another object:
 *
 *     ModelSkeleton skeleton_guard = {2, 0, jointlist_guard, 3, 0};
 *     ModelNode player_gait_hdr    = {1, &player_gait_obj, 0, 0, 0, ...};
 *
 * IDO folded those at link time. A host toolchain can too -- while the member is
 * an ordinary 8-byte pointer. The moment it is pinned to the N64's 4 bytes (see
 * hostcompat/ge_n64ptr.h), it cannot: a static initialiser may not contain a
 * truncation or an address-space cast of a relocatable address. clang says
 * "Unsupported expression in static initializer: addrspacecast"; the `u32`
 * spelling of the same idea fails as "initializer element is not computable at
 * load time".
 *
 * So this is NOT a quirk of the pinning mechanism -- it is a property of the
 * host toolchain, and PRIORITIES.md P3 identified it long before the pinning
 * work started:
 *
 *   > The 59 non-computable initialisers ... may need a runtime init pass, not a
 *   > macro fix -- the first item so far that is a real porting decision rather
 *   > than a toolchain difference.
 *
 * That was right. This is that pass.
 *
 * HOW
 *
 * The field is left zero in the initialiser and assigned before main() runs, so
 * by the time the game's own init() executes, the data looks exactly as it did
 * on the cartridge. Constructors run in unspecified order, which is fine here:
 * every assignment is independent, and none of them reads another.
 *
 * PORTABILITY
 *
 * `constructor` is a GCC/clang attribute. MSVC has no equivalent attribute, but
 * it does support the same effect through a CRT initialiser section
 * (`#pragma section(".CRT$XCU", ...)` plus `__declspec(allocate)`), which is how
 * the C++ compiler runs its own static constructors. That is written out below
 * rather than left as an exercise, because the Windows build is the target.
 */
#ifndef GE_INIT_PTR_H
#define GE_INIT_PTR_H

#define GE_INIT_CAT_(a, b) a##b
#define GE_INIT_CAT(a, b)  GE_INIT_CAT_(a, b)

#if defined(GE_HOST_PORT) && !defined(TARGET_N64)

#  if defined(_MSC_VER)
#    pragma section(".CRT$XCU", read)
#    define GE_INIT_STMT(stmt)                                                \
       static void GE_INIT_CAT(geInitFn_, __COUNTER__)(void) { stmt; }           \
       __declspec(allocate(".CRT$XCU"))                                       \
       void (*GE_INIT_CAT(geInitPtr_, __COUNTER__))(void) =                      \
           GE_INIT_CAT(geInitFn_, __COUNTER__);
#  else
#    define GE_INIT_STMT(stmt)                                                \
       __attribute__((constructor))                                           \
       static void GE_INIT_CAT(geInitFn_, __COUNTER__)(void) { stmt; }
#  endif

#else
   /* N64: the linker did it. Emits nothing, so the token stream is unchanged. */
#  define GE_INIT_STMT(stmt)
#endif

#endif /* GE_INIT_PTR_H */
