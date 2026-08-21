/*
 * hostcompat/stddef.h — host build shim.
 *
 * The decomp ships include/stddef.h as a near-empty stub. It exists to satisfy
 * the IRIX toolchain and defines nothing, so on a host build PR/ultratypes.h:78
 * does `#include <stddef.h>`, picks up the stub, and then fails on
 * `typedef ptrdiff_t ssize_t;` because ptrdiff_t was never declared.
 *
 * This directory shadows the decomp's N64-only stubs for a host compile, the
 * same way src/ultra/os.h replaces PR/os.h. Put it FIRST on the include path.
 *
 * Two things this deliberately does NOT do:
 *
 *   - It does not #include_next. The next stddef.h on the search path is the
 *     decomp's stub, not the compiler's, so that would find nothing.
 *   - It does not vendor a copy of the compiler's stddef.h. That header is
 *     licensed, and copying it into a project that gets distributed is an
 *     avoidable problem. The compiler's own builtin macros give the same
 *     types with nothing to redistribute.
 */
#ifndef GE_HOSTCOMPAT_STDDEF_H
#define GE_HOSTCOMPAT_STDDEF_H

typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __SIZE_TYPE__    size_t;

/*
 * max_align_t is not optional. The C++ standard library reaches for it through
 * <cstddef> (memory_resource.h does `alignof(max_align_t)`), so any C++ file
 * that includes <string> while this header is on the path fails with
 * "'max_align_t' has not been declared in '::'" -- again in a system header,
 * with nothing in the failing file to suggest why.
 */
#if !defined(__cplusplus) || __cplusplus < 201103L
typedef struct { long long __ll; long double __ld; } max_align_t;
#else
typedef struct { alignas(alignof(long long)) long long __ll;
                 alignas(alignof(long double)) long double __ld; } max_align_t;
#endif
#ifndef __cplusplus
typedef __WCHAR_TYPE__   wchar_t;
#endif

#ifndef NULL
#  ifdef __cplusplus
#    define NULL nullptr
#  else
#    define NULL ((void *)0)
#  endif
#endif

#ifndef offsetof
#  define offsetof(t, m) __builtin_offsetof(t, m)
#endif

#endif /* GE_HOSTCOMPAT_STDDEF_H */
